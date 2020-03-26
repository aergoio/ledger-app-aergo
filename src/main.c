#include "os.h"
#include "cx.h"
#include "ux.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define APP_VERSION_MAJOR   1
#define APP_VERSION_MINOR   0

#define CLA      0xAE
#define INS_GET_APP_VERSION 0x01
#define INS_GET_PUBLIC_KEY  0x02
#define INS_SIGN_TXN        0x04
#define P1_FIRST 0x01
#define P1_LAST  0x02

#define MAX_CHARS_PER_LINE 13  // some strings do not appear entirely on the screen if bigger than this

unsigned char G_io_seproxyhal_spi_buffer[IO_SEPROXYHAL_BUFFER_SIZE_B];

struct items {
  char *title;
  char *value;
  unsigned int vsize;
  bool in_hex;
};

static struct items screens[8];
static unsigned int num_screens;

static unsigned int current_screen;

static char line1[20];
static char line2[20];
static char line2b[30];

static unsigned int  line2_size;
static unsigned int  last_utf8_char;

static unsigned char*text_to_display;
static unsigned int  len_to_display;
static unsigned int  current_text_pos;  // current position in the text to display

bool txn_is_complete;
bool has_partial_payload;

static cx_sha256_t hash;
unsigned char txn_hash[32];

// UI currently displayed
enum UI_STATE { UI_IDLE, UI_FIRST, UI_TEXT, UI_SIGN, UI_REJECT };
enum UI_STATE uiState;

ux_state_t ux;

// private and public keys
cx_ecfp_private_key_t privateKey;
cx_ecfp_public_key_t  publicKey;

// functions declarations

static const bagl_element_t *io_seproxyhal_touch_exit(const bagl_element_t *e);
static const bagl_element_t *io_seproxyhal_touch_approve(const bagl_element_t *e);
static const bagl_element_t *io_seproxyhal_touch_deny(const bagl_element_t *e);

static void ui_idle(void);
static void ui_first(void);
static void ui_text(void);
static void ui_sign(void);
static void ui_reject(void);

static void next_screen();
static void previous_screen();

static void request_next_part();
static void on_new_transaction_part(unsigned char *text, unsigned int len, bool is_first, bool is_last);
static bool display_text_part(void);
static bool update_display_buffer();

static bool derive_keys(unsigned char *bip32Path, unsigned char bip32PathLength);

#define MAX_BIP32_PATH 10

/*
** This lookup table is used to help decode the first byte of
** a multi-byte UTF8 character.
*/
static const unsigned char UTF8Trans1[] = {
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
  0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
  0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x00, 0x01, 0x02, 0x03, 0x00, 0x01, 0x00, 0x00,
};

#define READ_UTF8(zIn, zEnd, c)                            \
  c = *(zIn++);                                            \
  is_utf8 = (c >= 0xc0);                                   \
  if( is_utf8 ){                                           \
    c = UTF8Trans1[c-0xc0];                                \
    while( zIn!=zEnd && (*zIn & 0xc0)==0x80 ){             \
      c = (c<<6) + (0x3f & *(zIn++));                      \
    }                                                      \
    if( c<0x80                                             \
        || (c&0xFFFFF800)==0xD800                          \
        || (c&0xFFFFFFFE)==0xFFFE ){ c = 0xFFFD; }         \
  }

#define READ_REMAINING_UTF8(zIn, zEnd, c)                  \
  while( zIn!=zEnd && (*zIn & 0xc0)==0x80 ){               \
    c = (c<<6) + (0x3f & *(zIn++));                        \
  }                                                        \
  if( c<0x80                                               \
      || (c&0xFFFFF800)==0xD800                            \
      || (c&0xFFFFFFFE)==0xFFFE ){ c = 0xFFFD; }           \

/*
** Array for converting from half-bytes (nybbles) into ASCII hex
** digits.
*/
static const char hexdigits[] = {
  '0', '1', '2', '3', '4', '5', '6', '7',
  '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
};

////////////////////////////////////////////////////////////////////////////////
// IDLE SCREEN
////////////////////////////////////////////////////////////////////////////////

static const bagl_element_t bagl_ui_idle_nanos[] = {
    // {
    //     {type, userid, x, y, width, height, stroke, radius, fill, fgcolor,
    //      bgcolor, font_id, icon_id},
    //     text,
    //     touch_area_brim,
    //     overfgcolor,
    //     overbgcolor,
    //     tap,
    //     out,
    //     over,
    // },
    {
        {BAGL_RECTANGLE, 0x00, 0, 0, 128, 32, 0, 0, BAGL_FILL, 0x000000,
         0xFFFFFF, 0, 0},
        NULL,
    },
    {
        {BAGL_LABELINE, 0x02, 0, 12, 128, 11, 0, 0, 0, 0xFFFFFF, 0x000000,
         BAGL_FONT_OPEN_SANS_REGULAR_11px | BAGL_FONT_ALIGNMENT_CENTER, 0},
        "Waiting for",
    },
    {
        {BAGL_LABELINE, 0x02, 0, 26, 128, 11, 0, 0, 0, 0xFFFFFF, 0x000000,
         BAGL_FONT_OPEN_SANS_REGULAR_11px | BAGL_FONT_ALIGNMENT_CENTER, 0},
        "transaction",
    },
    {
        {BAGL_ICON, 0x00, 3, 12, 7, 7, 0, 0, 0, 0xFFFFFF, 0x000000, 0,
         BAGL_GLYPH_ICON_CROSS},
        NULL,
    },
};

static unsigned int
bagl_ui_idle_nanos_button(unsigned int button_mask,
                          unsigned int button_mask_counter) {
    switch (button_mask) {
    case BUTTON_EVT_RELEASED | BUTTON_LEFT:
    case BUTTON_EVT_RELEASED | BUTTON_LEFT | BUTTON_RIGHT:
        io_seproxyhal_touch_exit(NULL);
        break;
    }

    return 0;
}


////////////////////////////////////////////////////////////////////////////////
// FIRST TEXT SCREEN
////////////////////////////////////////////////////////////////////////////////

static const bagl_element_t bagl_ui_first_nanos[] = {
    // {
    //     {type, userid, x, y, width, height, stroke, radius, fill, fgcolor,
    //      bgcolor, font_id, icon_id},
    //     text,
    //     touch_area_brim,
    //     overfgcolor,
    //     overbgcolor,
    //     tap,
    //     out,
    //     over,
    // },
    {
        {BAGL_RECTANGLE, 0x00, 0, 0, 128, 32, 0, 0, BAGL_FILL, 0x000000,
         0xFFFFFF, 0, 0},
        NULL,
    },
    {
        {BAGL_LABELINE, 0x02, 0, 12, 128, 11, 0, 0, 0, 0xFFFFFF, 0x000000,
         BAGL_FONT_OPEN_SANS_REGULAR_11px | BAGL_FONT_ALIGNMENT_CENTER, 0},
        line1,
    },
    {
        {BAGL_LABELINE, 0x02, 20, 26, 88, 11, 0, 0, 0, 0xFFFFFF, 0x000000,
         BAGL_FONT_OPEN_SANS_REGULAR_11px | BAGL_FONT_ALIGNMENT_CENTER, 0},
        line2,
    },
    {
        {BAGL_ICON, 0x00, 117, 13, 8, 6, 0, 0, 0, 0xFFFFFF, 0x000000, 0,
         BAGL_GLYPH_ICON_RIGHT},
        NULL,
    },
};

static unsigned int
bagl_ui_first_nanos_button(unsigned int button_mask,
                           unsigned int button_mask_counter) {
    switch (button_mask) {
    case BUTTON_EVT_RELEASED | BUTTON_RIGHT:
        next_screen();
        break;
    }
    return 0;
}


////////////////////////////////////////////////////////////////////////////////
// TEXT SCREEN
////////////////////////////////////////////////////////////////////////////////

static const bagl_element_t bagl_ui_text_nanos[] = {
    // {
    //     {type, userid, x, y, width, height, stroke, radius, fill, fgcolor,
    //      bgcolor, font_id, icon_id},
    //     text,
    //     touch_area_brim,
    //     overfgcolor,
    //     overbgcolor,
    //     tap,
    //     out,
    //     over,
    // },
    {
        {BAGL_RECTANGLE, 0x00, 0, 0, 128, 32, 0, 0, BAGL_FILL, 0x000000,
         0xFFFFFF, 0, 0},
        NULL,
    },
    {
        {BAGL_LABELINE, 0x02, 0, 12, 128, 11, 0, 0, 0, 0xFFFFFF, 0x000000,
         BAGL_FONT_OPEN_SANS_REGULAR_11px | BAGL_FONT_ALIGNMENT_CENTER, 0},
        line1,
    },
    {
        {BAGL_LABELINE, 0x02, 20, 26, 88, 11, 0, 0, 0, 0xFFFFFF, 0x000000,
         BAGL_FONT_OPEN_SANS_REGULAR_11px | BAGL_FONT_ALIGNMENT_CENTER, 0},
        line2,
    },
    {
        {BAGL_ICON, 0x00, 3, 12, 7, 7, 0, 0, 0, 0xFFFFFF, 0x000000, 0,
         BAGL_GLYPH_ICON_LEFT},
        NULL,
    },
    {
        {BAGL_ICON, 0x00, 117, 13, 8, 6, 0, 0, 0, 0xFFFFFF, 0x000000, 0,
         BAGL_GLYPH_ICON_RIGHT},
        NULL,
    },
};

static unsigned int
bagl_ui_text_nanos_button(unsigned int button_mask,
                          unsigned int button_mask_counter) {
    switch (button_mask) {
    case BUTTON_EVT_RELEASED | BUTTON_LEFT:
        previous_screen();
        break;
    case BUTTON_EVT_RELEASED | BUTTON_RIGHT:
        next_screen();
        break;
    }
    return 0;
}


////////////////////////////////////////////////////////////////////////////////
// SIGN SCREEN
////////////////////////////////////////////////////////////////////////////////

static const bagl_element_t bagl_ui_sign_nanos[] = {
    // {
    //     {type, userid, x, y, width, height, stroke, radius, fill, fgcolor,
    //      bgcolor, font_id, icon_id},
    //     text,
    //     touch_area_brim,
    //     overfgcolor,
    //     overbgcolor,
    //     tap,
    //     out,
    //     over,
    // },
    {
        {BAGL_RECTANGLE, 0x00, 0, 0, 128, 32, 0, 0, BAGL_FILL, 0x000000,
         0xFFFFFF, 0, 0},
        NULL,
    },
    {
        {BAGL_ICON, 0x00, 25, 12, 7, 7, 0, 0, 0, 0xFFFFFF, 0x000000, 0,
         BAGL_GLYPH_ICON_CHECK},
        NULL,
    },
    {
        {BAGL_LABELINE, 0x02, 45, 12, 128, 11, 0, 0, 0, 0xFFFFFF, 0x000000,
         BAGL_FONT_OPEN_SANS_REGULAR_11px, 0},
        "Sign",
    },
    {
        {BAGL_LABELINE, 0x02, 45, 26, 128, 11, 0, 0, 0, 0xFFFFFF, 0x000000,
         BAGL_FONT_OPEN_SANS_REGULAR_11px, 0},
        "Transaction",
    },
    {
        {BAGL_ICON, 0x00, 3, 12, 7, 7, 0, 0, 0, 0xFFFFFF, 0x000000, 0,
         BAGL_GLYPH_ICON_LEFT},
        NULL,
    },
    {
        {BAGL_ICON, 0x00, 117, 13, 8, 6, 0, 0, 0, 0xFFFFFF, 0x000000, 0,
         BAGL_GLYPH_ICON_RIGHT},
        NULL,
    },
};

static unsigned int
bagl_ui_sign_nanos_button(unsigned int button_mask,
                              unsigned int button_mask_counter) {
    switch (button_mask) {
    case BUTTON_EVT_RELEASED | BUTTON_LEFT:
        previous_screen();
        break;
    case BUTTON_EVT_RELEASED | BUTTON_RIGHT:
        next_screen();
        break;
    case BUTTON_EVT_RELEASED | BUTTON_LEFT | BUTTON_RIGHT:
        io_seproxyhal_touch_approve(NULL);
        break;
    }
    return 0;
}


////////////////////////////////////////////////////////////////////////////////
// REJECT SCREEN
////////////////////////////////////////////////////////////////////////////////

static const bagl_element_t bagl_ui_reject_nanos[] = {
    // {
    //     {type, userid, x, y, width, height, stroke, radius, fill, fgcolor,
    //      bgcolor, font_id, icon_id},
    //     text,
    //     touch_area_brim,
    //     overfgcolor,
    //     overbgcolor,
    //     tap,
    //     out,
    //     over,
    // },
    {
        {BAGL_RECTANGLE, 0x00, 0, 0, 128, 32, 0, 0, BAGL_FILL, 0x000000,
         0xFFFFFF, 0, 0},
        NULL,
    },
    {
        {BAGL_ICON, 0x00, 25, 12, 7, 7, 0, 0, 0, 0xFFFFFF, 0x000000, 0,
         BAGL_GLYPH_ICON_CROSS},
        NULL,
    },
    {
        {BAGL_LABELINE, 0x02, 45, 12, 128, 11, 0, 0, 0, 0xFFFFFF, 0x000000,
         BAGL_FONT_OPEN_SANS_REGULAR_11px, 0},
        "Reject",
    },
    {
        {BAGL_LABELINE, 0x02, 45, 26, 128, 11, 0, 0, 0, 0xFFFFFF, 0x000000,
         BAGL_FONT_OPEN_SANS_REGULAR_11px, 0},
        "Transaction",
    },
    {
        {BAGL_ICON, 0x00, 3, 12, 7, 7, 0, 0, 0, 0xFFFFFF, 0x000000, 0,
         BAGL_GLYPH_ICON_LEFT},
        NULL,
    },
};

static unsigned int
bagl_ui_reject_nanos_button(unsigned int button_mask,
                              unsigned int button_mask_counter) {
    switch (button_mask) {
    case BUTTON_EVT_RELEASED | BUTTON_LEFT:
        previous_screen();
        break;
    case BUTTON_EVT_RELEASED | BUTTON_RIGHT:
        // do nothing
        break;
    case BUTTON_EVT_RELEASED | BUTTON_LEFT | BUTTON_RIGHT:
        io_seproxyhal_touch_deny(NULL);
        break;
    }
    return 0;
}


////////////////////////////////////////////////////////////////////////////////


static const bagl_element_t *io_seproxyhal_touch_exit(const bagl_element_t *e) {
    // Go back to the dashboard
    os_sched_exit(0);
    return NULL; // do not redraw the widget
}

static const bagl_element_t *io_seproxyhal_touch_approve(const bagl_element_t *e) {
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
    ui_idle();
    return 0; // do not redraw the widget
}

static const bagl_element_t *io_seproxyhal_touch_deny(const bagl_element_t *e) {
    G_io_apdu_buffer[0] = 0x69;
    G_io_apdu_buffer[1] = 0x82;
    // Send back the response and return without waiting for new APDU
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, 2);
    // Display back the original UX
    ui_idle();
    return 0; // do not redraw the widget
}

static void request_next_part() {
    G_io_apdu_buffer[0] = 0x90;
    G_io_apdu_buffer[1] = 0x00;
    // Send back the response and return without waiting for new APDU
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, 2);
}

unsigned short io_exchange_al(unsigned char channel, unsigned short tx_len) {

    switch (channel & ~(IO_FLAGS)) {
    case CHANNEL_KEYBOARD:
        break;

    // multiplexed io exchange over a SPI channel and TLV encapsulated protocol
    case CHANNEL_SPI:
        if (tx_len) {
            io_seproxyhal_spi_send(G_io_apdu_buffer, tx_len);
            if (channel & IO_RESET_AFTER_REPLIED) {
                reset();
            }
            return 0; // nothing received from the master so far (it's a tx transaction)
        } else {
            return io_seproxyhal_spi_recv(G_io_apdu_buffer, sizeof(G_io_apdu_buffer), 0);
        }

    default:
        THROW(INVALID_PARAMETER);
    }
    return 0;
}

static void sample_main(void) {
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

                switch (G_io_apdu_buffer[1]) {
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

                    if (derive_keys(path,len) == false) {
                        THROW(0x6700);  // wrong length
                    }

                    os_memmove(G_io_apdu_buffer, publicKey.W, 33);
                    tx = 33;
                    THROW(0x9000);
                } break;

                case INS_SIGN_TXN: {
                    unsigned char *text;
                    unsigned int len;
                    bool is_first = false;
                    bool is_last = false;

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

                case 0xFF: // return to dashboard
                    goto return_to_dashboard;

                default:
                    THROW(0x6D00);
                    break;
                }
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

void io_seproxyhal_display(const bagl_element_t *element) {
    io_seproxyhal_display_default((bagl_element_t *)element);
}

static void clear_screens() {
  num_screens = 0;
  memset(screens, 0, sizeof(screens));
  last_utf8_char = 0;
}

static void add_screens(char *title, char *value, unsigned int len, bool scroll_value) {
  int i;

  if (scroll_value) {

    i = num_screens++;
    screens[i].title = title;
    screens[i].value = value;
    screens[i].vsize = len;

  } else {

    char *ptr = value;
    while (len > 0) {
      i = num_screens++;
      screens[i].title = title;
      screens[i].value = ptr;
      screens[i].vsize = len > MAX_CHARS_PER_LINE ? MAX_CHARS_PER_LINE : len;
      ptr += screens[i].vsize;
      len -= screens[i].vsize;
    }

  }

}

static void display_screen(unsigned int n) {
    current_screen = n;

    strcpy(line1, screens[current_screen].title);

    text_to_display = (unsigned char*)
                     screens[current_screen].value;
    len_to_display = screens[current_screen].vsize;
    current_text_pos = 0;

    line2_size = 0;
    display_text_part();

    if (current_screen == 0) {
      ui_first();
    } else {
      if (uiState != UI_TEXT) {
        ui_text();
      } else {
        UX_REDISPLAY();
      }
    }

}

static void next_screen() {

    switch (uiState) {

    case UI_FIRST:
    case UI_TEXT:
      if (current_screen == num_screens - 1) {
        ui_sign();
      } else {
        display_screen(current_screen + 1);
      }
      break;

    case UI_SIGN:
      ui_reject();
      break;

    case UI_IDLE:
    case UI_REJECT:
      // here just to avoid compiler warnings
      break;

    }

}

static void previous_screen() {

    switch (uiState) {

    case UI_FIRST:
    case UI_TEXT:
      if (current_screen > 0) {
        display_screen(current_screen - 1);
      }
      break;

    case UI_SIGN:
      display_screen(num_screens - 1);
      break;

    case UI_REJECT:
      ui_sign();
      break;

    case UI_IDLE:
      // here just to avoid compiler warnings
      break;

    }

}

static unsigned char text_part_completely_displayed() {
    if (current_text_pos >= len_to_display &&
        line2_size <= MAX_CHARS_PER_LINE) {
      return 1;
    }
    return 0;
}

static bool is_first_display;

/* pick the text part to be displayed */
static bool display_text_part() {
    unsigned int len;

    is_first_display = (line2_size == 0);

    if (text_part_completely_displayed() && txn_is_complete) {
      return false;
    }

    if (line2_size <= MAX_CHARS_PER_LINE) {
      if (update_display_buffer() == false) {
        request_next_part();
        return false;
      }
    }

    if (!is_first_display) {
      memmove(line2b, line2b+1, line2_size-1);
      line2_size--;
    }

    len = line2_size;
    if (len > MAX_CHARS_PER_LINE) {
      len = MAX_CHARS_PER_LINE;
    }
    memcpy(line2, line2b, len);
    line2[len] = '\0';

    return true;
}

/*
** Reads characters from the source text and writes into the
** display buffer until it has enough content to display or
** the source buffer was all read.
*/
static bool update_display_buffer() {
    unsigned char *zIn, *zEnd;

    zIn  = &text_to_display[current_text_pos];
    zEnd = &text_to_display[len_to_display];

    while (zIn < zEnd && line2_size <= MAX_CHARS_PER_LINE) {
        unsigned int c = 0;
        bool is_utf8 = false;

        if (screens[current_screen].in_hex) {
          c = *(zIn++);
        } else {
          if (last_utf8_char != 0) {
            c = last_utf8_char;
            last_utf8_char = 0;
            READ_REMAINING_UTF8(zIn, zEnd, c);
          } else {
            READ_UTF8(zIn, zEnd, c);
          }
        }

        current_text_pos = zIn - text_to_display;

        /* do we have a partial UTF8 char at the end? */
        if (zIn == zEnd && is_utf8 && has_partial_payload) {
          last_utf8_char = c;
          return false;
        }

        if (screens[current_screen].in_hex) {
            line2b[line2_size++] = hexdigits[(c >> 4) & 0xf];
            line2b[line2_size++] = hexdigits[c & 0xf];
        } else if (c > 0x7F) { /* non-ascii chars */
            line2b[line2_size++] = '\\';
            line2b[line2_size++] = 'u';
            snprintf(&line2b[line2_size], sizeof(line2b) - line2_size, "%X", c);
            line2_size += strlen(&line2b[line2_size]);
        } else if (c == '\n' || c == '\r') {
            line2b[line2_size++] = ' ';
            line2b[line2_size++] = '|';
            line2b[line2_size++] = ' ';
        } else if (c == 0x08) { /* backspace should not be hidden */
            line2b[line2_size++] = '?';
        } else {
            line2b[line2_size++] = c;
        }
    }

    if (zIn == zEnd && has_partial_payload) {
      return false;
    }
    return true;
}

static void display_updated_buffer() {

    text_to_display = (unsigned char*)
                     screens[current_screen].value;
    len_to_display = screens[current_screen].vsize;

    current_text_pos = 0;
    display_text_part();

    UX_REDISPLAY();
    UX_CALLBACK_SET_INTERVAL(200);

}

static void ui_idle(void) {
    uiState = UI_IDLE;
    UX_DISPLAY(bagl_ui_idle_nanos, NULL);
}

static void ui_first(void) {
    uiState = UI_FIRST;
    UX_DISPLAY(bagl_ui_first_nanos, NULL);
}

static void ui_text(void) {
    uiState = UI_TEXT;
    UX_DISPLAY(bagl_ui_text_nanos, NULL);
}

static void ui_sign(void) {
    uiState = UI_SIGN;
    UX_DISPLAY(bagl_ui_sign_nanos, NULL);
}

static void ui_reject(void) {
    uiState = UI_REJECT;
    UX_DISPLAY(bagl_ui_reject_nanos, NULL);
}

unsigned char io_event(unsigned char channel) {
    // nothing done with the event, throw an error on the transport layer if needed

    // can't have more than one tag in the reply, not supported yet.
    switch (G_io_seproxyhal_spi_buffer[0]) {
    case SEPROXYHAL_TAG_FINGER_EVENT:
        UX_FINGER_EVENT(G_io_seproxyhal_spi_buffer);
        break;

    case SEPROXYHAL_TAG_BUTTON_PUSH_EVENT: // for Nano S
        UX_BUTTON_PUSH_EVENT(G_io_seproxyhal_spi_buffer);
        break;

    case SEPROXYHAL_TAG_DISPLAY_PROCESSED_EVENT:
        if (UX_DISPLAYED()) {
            // perform action after screen elements have been displayed
            if (uiState == UI_FIRST || uiState == UI_TEXT) {
              if (is_first_display) {
                UX_CALLBACK_SET_INTERVAL(1500);
              } else if (text_part_completely_displayed() && txn_is_complete) {
                // do nothing
              } else {
                UX_CALLBACK_SET_INTERVAL(200);
              }
            }
        } else {
            UX_DISPLAYED_EVENT();
        }
        break;

    case SEPROXYHAL_TAG_TICKER_EVENT:
        UX_TICKER_EVENT(G_io_seproxyhal_spi_buffer, {
          if (UX_ALLOWED) {
            if (uiState == UI_FIRST || uiState == UI_TEXT) {
                if (text_part_completely_displayed() && txn_is_complete) {
                    // do nothing
                } else {
                    // scroll the text
                    if (display_text_part()) {
                        UX_REDISPLAY();
                    }
                }
            }
          } else {
            UX_CALLBACK_SET_INTERVAL(200);
          }
        });
        break;

    // unknown events are acknowledged
    default:
        UX_DEFAULT_EVENT();
        break;
    }

    // close the event if not done previously (by a display or whatever)
    if (!io_seproxyhal_spi_is_status_sent()) {
        io_seproxyhal_general_status();
    }

    // command has been processed, DO NOT reset the current APDU transport
    return 1;
}


#include "transaction.h"


static bool derive_keys(unsigned char *bip32Path, unsigned char bip32PathLength) {
    unsigned int  path[MAX_BIP32_PATH];
    unsigned char i;
    unsigned char privateKeyData[64];

    /* the length must be a multiple of 4 */
    if ((bip32PathLength & 0x03) != 0) {
        return false;
    }
    bip32PathLength /= 4;
    if (bip32PathLength < 1 || bip32PathLength > MAX_BIP32_PATH) {
        return false;
    }
    for (i = 0; i < bip32PathLength; i++) {
        path[i] = U4BE(bip32Path, 0);
        bip32Path += 4;
    }

    io_seproxyhal_io_heartbeat();
    os_perso_derive_node_bip32(CX_CURVE_256K1, path, bip32PathLength, privateKeyData, NULL);
    cx_ecdsa_init_private_key(CX_CURVE_256K1, privateKeyData, 32, &privateKey);
    io_seproxyhal_io_heartbeat();
    cx_ecfp_generate_pair(CX_CURVE_256K1, &publicKey, &privateKey, 1);

    publicKey.W[0] = ((publicKey.W[64] & 1) ? 0x03 : 0x02);

    memset(privateKeyData, 0, sizeof privateKeyData);
    return true;
}

__attribute__((section(".boot"))) int main(void) {
    // exit critical section
    __asm volatile("cpsie i");

    current_text_pos = 0;
    line2_size = 0;
    uiState = UI_IDLE;

    // ensure exception will work as planned
    os_boot();

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

            ui_idle();

            sample_main();
        }
        CATCH_OTHER(e) {
        }
        FINALLY {
        }
    }
    END_TRY;
}
