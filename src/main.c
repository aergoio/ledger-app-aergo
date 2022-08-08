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
#include "sw.h"

#include "globals.h"

#include "os_io_seproxyhal.h"


unsigned char G_io_seproxyhal_spi_buffer[IO_SEPROXYHAL_BUFFER_SIZE_B];

ux_state_t G_ux;
bolos_ux_params_t G_ux_params;


// selected account - BIP32 path & public key

#define MAX_BIP32_PATH 10

static bool account_selected;

uint32_t bip32_path[MAX_BIP32_PATH];
uint8_t bip32_path_len;

cx_ecfp_public_key_t public_key;


// functions declarations

static void request_first_part();
static void request_next_part();

static void on_new_transaction_part(unsigned char *text, unsigned int len, bool is_first, bool is_last);
static void on_new_message(unsigned char *text, unsigned int len, bool as_hex);
static void on_display_account(unsigned char *pubkey, int pklen);


void ui_menu_main();
void ui_menu_about();
void start_display();


#include "io.h"

#include "common/sha256.h"

#include "crypto.h"

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
  tx = crypto_sign_message(G_io_apdu_buffer+32, sizeof(G_io_apdu_buffer)-32);

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
        // check the length of the received APDU
        if (rx < 5 || G_io_apdu_buffer[4] != rx - 5) {
          THROW(SW_WRONG_DATA_LENGTH);
        }

        if (G_io_apdu_buffer[0] != CLA) {
          THROW(SW_CLA_NOT_SUPPORTED);
        }

        cmd_type = G_io_apdu_buffer[1];
        switch (cmd_type) {

        case INS_GET_APP_VERSION: {
          G_io_apdu_buffer[0] = APP_VERSION_MAJOR;
          G_io_apdu_buffer[1] = APP_VERSION_MINOR;
          tx = 2;
          THROW(SW_OK);
        } break;

        case INS_GET_PUBLIC_KEY: {
          unsigned char *path;
          unsigned char len;

          len = G_io_apdu_buffer[4];
          path = G_io_apdu_buffer + 5;

          if (len > 0) {
            if (crypto_select_account(path,len) == false) {
              THROW(SW_WRONG_LENGTH);
            }
          } else if (!account_selected) {
            THROW(SW_INVALID_STATE);
          }

          memmove(G_io_apdu_buffer, public_key.W, 33);
          tx = 33;
          THROW(SW_OK);
        } break;

        case INS_DISPLAY_ACCOUNT: {

          if (!account_selected) {
            THROW(SW_INVALID_STATE);
          }

          on_display_account(public_key.W, 33);

          tx = 0;
          THROW(SW_OK);
        } break;

        case INS_SIGN_TXN: {
          unsigned char *text;
          unsigned int len;
          bool is_first = false;
          bool is_last = false;

          if (!account_selected) {
            THROW(SW_INVALID_STATE);
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
            THROW(SW_WRONG_LENGTH);
          }
          if (!is_last && len < 50) {
            THROW(SW_WRONG_LENGTH);
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
            THROW(SW_INVALID_STATE);
          }

          if (G_io_apdu_buffer[2] & P1_HEX) {
            as_hex = true;
          }
          // check the message length
          len = G_io_apdu_buffer[4];
          if (len > 250) {
            THROW(SW_WRONG_LENGTH);
          }
          //
          text = G_io_apdu_buffer + 5;
          on_new_message(text, len, as_hex);
          flags |= IO_ASYNCH_REPLY;
        } break;

        default:
          THROW(SW_INS_NOT_SUPPORTED);
          break;
        }
      }
      CATCH(EXCEPTION_IO_RESET) {
        THROW(EXCEPTION_IO_RESET);
      }
      CATCH_OTHER(e) {
        switch (e & 0xF000) {
        case 0x6000:
        case SW_OK:
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
