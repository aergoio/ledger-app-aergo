#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <cmocka.h>

#include "testing.h"
#include "../src/globals.h"

int current_state = 0;
#define STATIC_SCREEN   0
#define DYNAMIC_SCREEN  1

int requested_part = 0;
#define FIRST_PART  1
#define NEXT_PART   2

void request_first_part() {
  requested_part = FIRST_PART;
}

void request_next_part() {
  requested_part = NEXT_PART;
}

void start_display() {
  current_state = STATIC_SCREEN;
  strcpy(global_title, "Review");
  strcpy(global_text,  "Transaction");
}

void update_screen() {
  if (current_state == STATIC_SCREEN) {
    strcpy(global_title, "Review");
    strcpy(global_text,  "Transaction");
  }
}


#include "../src/display_pages.h"
#include "../src/display_text.h"

#include "../src/transaction.h"
#include "../src/selection.h"


////////////////////////////////////////////////////////////////////////////////
// CALLBACKS
////////////////////////////////////////////////////////////////////////////////

void on_first_page_cb(bool has_dynamic_data) {
  if (has_dynamic_data) {
    current_state = DYNAMIC_SCREEN;
  }
  update_screen();
}

void on_prev_page_cb(bool has_dynamic_data) {
  if (!has_dynamic_data) {
    current_state = STATIC_SCREEN;
  }
  update_screen();
}

void on_last_page_cb(bool has_dynamic_data) {
  if (has_dynamic_data) {
    current_state = DYNAMIC_SCREEN;
  }
  update_screen();
}

void on_next_page_cb(bool has_dynamic_data) {
  if (!has_dynamic_data) {
    current_state = STATIC_SCREEN;
  }
  update_screen();
}

////////////////////////////////////////////////////////////////////////////////
// DELIMITERS
////////////////////////////////////////////////////////////////////////////////

void on_anterior_delimiter() {
  if (current_state == STATIC_SCREEN) {
    get_next_data(PAGE_LAST, on_last_page_cb);
  } else {
    get_next_data(PAGE_PREV, on_prev_page_cb);
  }
}

void on_posterior_delimiter() {
  if (current_state == STATIC_SCREEN) {
    get_next_data(PAGE_FIRST, on_first_page_cb);
  } else {
    get_next_data(PAGE_NEXT, on_next_page_cb);
  }
}

////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////

unsigned char *txn_ptr;
unsigned int txn_size;
unsigned int txn_offset;

static void send_next_txn_part() {
  bool is_first, is_last;

  requested_part = 0; // reset

  unsigned int bytes_now = txn_size - txn_offset;
  if (bytes_now > MAX_TX_PART) bytes_now = MAX_TX_PART;
  if (bytes_now <= 0) return;

  is_first = (txn_offset == 0);
  is_last  = (txn_offset + bytes_now == txn_size);

  on_new_transaction_part(txn_ptr + txn_offset, bytes_now, is_first, is_last);

  txn_offset += bytes_now;

}

static void send_transaction(const unsigned char *buf, unsigned int len){

  memset(&txn, 0, sizeof(struct txn));

  txn_ptr = (unsigned char *) buf;
  txn_size = len;
  txn_offset = 0;
  send_next_txn_part();

}

static void check_send_txn_part() {

  while (requested_part != 0) {
    if (requested_part == FIRST_PART) {
      txn_offset = 0;
      send_next_txn_part();
    } else if (requested_part == NEXT_PART) {
      send_next_txn_part();
    } else {
      THROW(0x2727);
    }
  }

}

static void click_next() {
  on_posterior_delimiter();
  check_send_txn_part();
}

static void click_prev() {
  on_anterior_delimiter();
  check_send_txn_part();
}

////////////////////////////////////////////////////////////////////////////////
// TEST CASES
////////////////////////////////////////////////////////////////////////////////

// NORMAL / LEGACY
static void test_tx_display_1(void **state) {
    (void) state;

    // clang-format off
    uint8_t raw_tx[] = {
        // tx type
        0x00,
        // transaction
        0x08, 0x01, 0x12, 0x21, 0x03, 0x4f, 0xea, 0xa6,
        0xed, 0xd6, 0xcf, 0x2a, 0x0e, 0x35, 0x5c, 0x88,
        0xe9, 0xbe, 0x9a, 0xc6, 0x98, 0x30, 0x83, 0x88,
        0x27, 0xbe, 0xda, 0xa3, 0x85, 0xc5, 0x81, 0x8e,
        0xb7, 0x25, 0xcb, 0x1d, 0x87, 0x1a, 0x21, 0x02,
        0x5d, 0x22, 0x30, 0xba, 0x75, 0x21, 0x7e, 0x60,
        0x37, 0x99, 0xe9, 0xa3, 0xd5, 0xb9, 0x1a, 0x63,
        0x61, 0x48, 0x3f, 0x9d, 0xa7, 0x37, 0x96, 0x41,
        0x0f, 0x6b, 0xc1, 0xce, 0x58, 0x01, 0xfd, 0xf2,
        0x22, 0x09, 0x06, 0xb1, 0x4b, 0xd1, 0xe6, 0xee,
        0xa0, 0x00, 0x00, 0x2a, 0x20, 0x30, 0x31, 0x30,
        0x32, 0x30, 0x33, 0x30, 0x34, 0x30, 0x35, 0x30,
        0x36, 0x30, 0x37, 0x30, 0x38, 0x30, 0x39, 0x30,
        0x41, 0x30, 0x42, 0x30, 0x43, 0x30, 0x44, 0x30,
        0x45, 0x30, 0x46, 0x46, 0x46, 0x3a, 0x01, 0x00,
        0x4a, 0x20, 0x52, 0x48, 0x45, 0xc2, 0x4c, 0xd3,
        0xe5, 0x3a, 0xec, 0xbc, 0xda, 0x8e, 0x31, 0x5d,
        0x62, 0xdc, 0x95, 0xa7, 0xf2, 0xf8, 0x25, 0x48,
        0x93, 0x0b, 0xc2, 0xfc, 0xc9, 0x86, 0xbf, 0x74,
        0x53, 0xbd,
    };

    int ret = setjmp(jump_buffer);
    assert_int_equal(ret, 0);

    send_transaction(raw_tx, sizeof(raw_tx));

    assert_string_equal(global_title, "Review");
    assert_string_equal(global_text, "Transaction");

    click_next();
    assert_string_equal(global_title, "..");
    assert_string_equal(global_text, "..");



}

int main() {
    const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_tx_display_1),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
