#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define APP_VERSION_MAJOR   1
#define APP_VERSION_MINOR   1

#include "os.h"
#include "cx.h"
#include "ux.h"

#include "glyphs.h"

#include "apdu.h"

#include "globals.h"

#include "os_io_seproxyhal.h"


unsigned char G_io_seproxyhal_spi_buffer[IO_SEPROXYHAL_BUFFER_SIZE_B];

ux_state_t G_ux;
bolos_ux_params_t G_ux_params;


// private and public keys
cx_ecfp_private_key_t privateKey;
cx_ecfp_public_key_t  publicKey;
static bool account_selected;


// functions declarations

static void request_first_part();
static void request_next_part();

static void on_new_transaction_part(unsigned char *text, unsigned int len, bool is_first, bool is_last);
static void on_new_message(unsigned char *text, unsigned int len, bool as_hex);
static void on_display_account(unsigned char *pubkey, int pklen);

static bool derive_keys(unsigned char *bip32Path, unsigned char bip32PathLength);


void ui_menu_main();
void ui_menu_about();
void start_display();


////////////////////////////////////////////////////////////////////////////////
// ACTIONS
////////////////////////////////////////////////////////////////////////////////

static void sign_transaction() {
  unsigned int tx = 0;

  if (!txn_is_complete) {
    THROW(0x6986);
  }

  /* copy the transaction hash */
  memcpy(G_io_apdu_buffer, txn_hash, 32);

  /* create a signature for the transaction hash */
  tx = cx_ecdsa_sign((void*) &privateKey, CX_RND_RFC6979 | CX_LAST, CX_SHA256,
                     txn_hash, sizeof txn_hash,
                     G_io_apdu_buffer+32, sizeof(G_io_apdu_buffer)-32, NULL);
  G_io_apdu_buffer[32] &= 0xF0; // discard the parity information

  if (tx > 0) {
    tx += 32; /* the txn hash */
    G_io_apdu_buffer[tx++] = 0x90;
    G_io_apdu_buffer[tx++] = 0x00;
  } else {
    G_io_apdu_buffer[0] = 0x69;
    G_io_apdu_buffer[1] = 0x85;  // invalid state
    tx = 2;
  }

  // Send back the response and return without waiting for new APDU
  io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, tx);

  // Display back the original UX
  ui_menu_main();
}

static void reject_transaction() {
  G_io_apdu_buffer[0] = 0x69;
  G_io_apdu_buffer[1] = 0x82;
  // Send back the response and return without waiting for new APDU
  io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, 2);
  // Display back the original UX
  ui_menu_main();
}

////////////////////////////////////////////////////////////////////////////////
// TRANSACTION PARTS
////////////////////////////////////////////////////////////////////////////////

static void request_first_part() {
  G_io_apdu_buffer[0] = 0x90;
  G_io_apdu_buffer[1] = 0x01;
  // Send back the response and return without waiting for new APDU
  io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, 2);
}

static void request_next_part() {
  G_io_apdu_buffer[0] = 0x90;
  G_io_apdu_buffer[1] = 0x00;
  // Send back the response and return without waiting for new APDU
  io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, 2);
}

////////////////////////////////////////////////////////////////////////////////
// EXTERNAL FILES
////////////////////////////////////////////////////////////////////////////////

#include "io.h"

#include "common/sha256.h"

#include "crypto.h"

#include "menu.h"

#include "display_pages.h"

#include "display_text.h"

#include "display.h"

#include "transaction.h"

#include "selection.h"

////////////////////////////////////////////////////////////////////////////////
// APP MAIN LOOP
////////////////////////////////////////////////////////////////////////////////

void app_main() {
  volatile unsigned int rx = 0;
  volatile unsigned int tx = 0;
  volatile unsigned int flags = 0;

  // DESIGN NOTE: the bootloader ignores the way APDU are fetched. The only
  // goal is to retrieve APDU.
  // When APDU are to be fetched from multiple IOs, like NFC+USB+BLE, make
  // sure the io_event is called with a
  // switch event, before the apdu is replied to the bootloader. This avoid
  // APDU injection faults.
  for (;;) {
    volatile unsigned short sw = 0;

    BEGIN_TRY {
      TRY {
        rx = tx;
        tx = 0; // ensure no race in catch_other if io_exchange throws
                // an error
        rx = io_exchange(CHANNEL_APDU | flags, rx);
        flags = 0;

        // no apdu received, well, reset the session, and reset the
        // bootloader configuration
        if (rx == 0) {
          THROW(0x6982);
        }

        if (G_io_apdu_buffer[0] != CLA) {
          THROW(0x6E00);
        }

        cmd_type = G_io_apdu_buffer[1];
        switch (cmd_type) {

        case INS_GET_APP_VERSION: {
          G_io_apdu_buffer[0] = APP_VERSION_MAJOR;
          G_io_apdu_buffer[1] = APP_VERSION_MINOR;
          tx = 2;
          THROW(0x9000);
        } break;

        case INS_GET_PUBLIC_KEY: {
          unsigned char *path;
          unsigned char len;

          len = G_io_apdu_buffer[4];
          path = G_io_apdu_buffer + 5;

          if (len > 0) {
            if (derive_keys(path,len) == false) {
              THROW(0x6700);  // wrong length
            }
          } else if (!account_selected) {
            THROW(0x6985);  // invalid state
          }

          memmove(G_io_apdu_buffer, publicKey.W, 33);
          tx = 33;
          THROW(0x9000);
        } break;

        case INS_DISPLAY_ACCOUNT: {

          if (!account_selected) {
            THROW(0x6985);  // invalid state
          }

          on_display_account(publicKey.W, 33);

          tx = 0;
          THROW(0x9000);
        } break;

        case INS_SIGN_TXN: {
          unsigned char *text;
          unsigned int len;
          bool is_first = false;
          bool is_last = false;

          if (!account_selected) {
            THROW(0x6985);  // invalid state
          }

          if (G_io_apdu_buffer[2] & P1_FIRST) {
            is_first = true;
          }
          if (G_io_apdu_buffer[2] & P1_LAST) {
            is_last = true;
          }
          // check the message length
          len = G_io_apdu_buffer[4];
          if (len > 250) {
            THROW(0x6700);  // wrong length
          }
          if (!is_last && len < 50) {
            THROW(0x6700);  // wrong length
          }
          //
          text = G_io_apdu_buffer + 5;
          on_new_transaction_part(text, len, is_first, is_last);
          flags |= IO_ASYNCH_REPLY;
        } break;

        case INS_SIGN_MSG: {
          unsigned char *text;
          unsigned int len;
          bool as_hex = false;

          if (!account_selected) {
            THROW(0x6985);  // invalid state
          }

          if (G_io_apdu_buffer[2] & P1_HEX) {
            as_hex = true;
          }
          // check the message length
          len = G_io_apdu_buffer[4];
          if (len > 250) {
            THROW(0x6700);  // wrong length
          }
          //
          text = G_io_apdu_buffer + 5;
          on_new_message(text, len, as_hex);
          flags |= IO_ASYNCH_REPLY;
        } break;

        case 0xFF: // return to dashboard
          goto return_to_dashboard;

        default:
          THROW(0x6D00);
          break;
        }
      }
      CATCH(EXCEPTION_IO_RESET) {
        THROW(EXCEPTION_IO_RESET);
      }
      CATCH_OTHER(e) {
        switch (e & 0xF000) {
        case 0x6000:
        case 0x9000:
          sw = e;
          break;
        default:
          sw = 0x6800 | (e & 0x7FF);
          break;
        }
        // Unexpected exception => report
        G_io_apdu_buffer[tx] = sw >> 8;
        G_io_apdu_buffer[tx + 1] = sw;
        tx += 2;
      }
      FINALLY {
      }
    }
    END_TRY;
  }

return_to_dashboard:
  return;
}

/**
 * Exit the application and go back to the dashboard.
 */
void app_exit() {
  BEGIN_TRY_L(exit) {
    TRY_L(exit) {
      os_sched_exit(-1);
    }
    FINALLY_L(exit) {
    }
  }
  END_TRY_L(exit);
}

/**
 * Main loop to setup USB, Bluetooth, UI and launch app_main().
 */
__attribute__((section(".boot"))) int main(void) {
  // exit critical section
  __asm volatile("cpsie i");

  // ensure exception will work as planned
  os_boot();

  while (1) {

    account_selected = false;
    txn_is_complete = true;
    page_to_display = 0;
    input_pos = 0;
    reset_page();

    UX_INIT();

    BEGIN_TRY {
      TRY {
        io_seproxyhal_init();

#ifdef LISTEN_BLE
        if (os_seph_features() & SEPROXYHAL_TAG_SESSION_START_EVENT_FEATURE_BLE) {
          BLE_power(0, NULL);
          // restart IOs
          BLE_power(1, NULL);
        }
#endif

        USB_power(0);
        USB_power(1);

        ui_menu_main();

        app_main();
      }
      CATCH(EXCEPTION_IO_RESET) {
        // reset IO and UX before continuing
        continue;
      }
      CATCH_OTHER(e) {
      }
      FINALLY {
      }
    }
    END_TRY;

  }

}
