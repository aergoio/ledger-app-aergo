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

char display_title[20];
char display_text[20];

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
  strcpy(display_title, "Review");
  strcpy(display_text,  "Transaction");
}

void update_screen() {
  if (current_state == STATIC_SCREEN) {
    strcpy(display_title, "Review");
    strcpy(display_text,  "Transaction");
  } else {
    strcpy(display_title, global_title);
    strcpy(display_text,  global_text);
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

    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    click_next();
    assert_string_equal(display_title, "Amount");
    assert_string_equal(display_text, "123.456 AERGO");

    click_next();
    assert_string_equal(display_title, "Recipient");
    assert_string_equal(display_text, "AmMDEyc36FNXB");

    click_next();
    assert_string_equal(display_title, "Recipient");
    assert_string_equal(display_text, "3Fq1a61HeVJRT");

    click_next();
    assert_string_equal(display_title, "Recipient");
    assert_string_equal(display_text, "4yssMEP11NWWE");

    click_next();
    assert_string_equal(display_title, "Recipient");
    assert_string_equal(display_text, "9Qx8yhfRKexvq");

    click_next();
    assert_string_equal(display_title, "Payload");
    assert_string_equal(display_text, "0102030405060708090A0B0C0D0E0FFF");

    click_next();
    assert_string_equal(display_title, "..");
    assert_string_equal(display_text, "..");

    // BACKWARDS

    click_prev();
    assert_string_equal(display_title, "..");
    assert_string_equal(display_text, "..");


}

// TRANSFER
static void test_tx_display_2(void **state) {
    (void) state;

    // clang-format off
    uint8_t raw_tx[] = {
        // tx type
        0x04,
        // transaction
        0x08, 0x0a, 0x12, 0x21, 0x02, 0x9d, 0x02, 0x05,
        0x91, 0xe7, 0xfb, 0x7b, 0x09, 0x21, 0x53, 0x68,
        0x19, 0x95, 0xf8, 0x06, 0x09, 0xf0, 0xac, 0x98,
        0x8a, 0x4d, 0x93, 0x5e, 0x0e, 0xa6, 0x3c, 0x06,
        0x0f, 0x19, 0x54, 0xb0, 0x5f, 0x1a, 0x21, 0x03,
        0x8c, 0xb9, 0x2c, 0xde, 0xbf, 0x39, 0x98, 0x69,
        0x09, 0x3c, 0xac, 0x47, 0xe3, 0x70, 0xd8, 0xa9,
        0xfa, 0x50, 0x17, 0x30, 0x42, 0x23, 0xf9, 0xad,
        0x1a, 0x8c, 0x0a, 0x05, 0xa9, 0x06, 0xa9, 0xcb,
        0x22, 0x08, 0x14, 0xd1, 0x12, 0x0d, 0x7b, 0x16,
        0x00, 0x00, 0x3a, 0x01, 0x00, 0x40, 0x04, 0x4a,
        0x20, 0x52, 0x48, 0x45, 0xc2, 0x4c, 0xd3, 0xe5,
        0x3a, 0xec, 0xbc, 0xda, 0x8e, 0x31, 0x5d, 0x62,
        0xdc, 0x95, 0xa7, 0xf2, 0xf8, 0x25, 0x48, 0x93,
        0x0b, 0xc2, 0xfc, 0xc9, 0x86, 0xbf, 0x74, 0x53,
        0xbd,
    };

    int ret = setjmp(jump_buffer);
    assert_int_equal(ret, 0);

    send_transaction(raw_tx, sizeof(raw_tx));

    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    click_next();
    assert_string_equal(display_title, "Amount");
    assert_string_equal(display_text, "1.5 AERGO");

    click_next();
    assert_string_equal(display_title, "Recipient");
    assert_string_equal(display_text, "AmPWwmdgpvPRP");

    click_next();
    assert_string_equal(display_title, "Recipient");
    assert_string_equal(display_text, "tykgCCWvVdZS6");

    click_next();
    assert_string_equal(display_title, "Recipient");
    assert_string_equal(display_text, "h7b6w9UzcLcsE");

    click_next();
    assert_string_equal(display_title, "Recipient");
    assert_string_equal(display_text, "d64mzKJ9RCAhp");

    click_next();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    // BACKWARDS

    click_prev();
    assert_string_equal(display_title, "Recipient");
    assert_string_equal(display_text, "d64mzKJ9RCAhp");

    click_prev();
    assert_string_equal(display_title, "Recipient");
    assert_string_equal(display_text, "h7b6w9UzcLcsE");

    click_prev();
    assert_string_equal(display_title, "Recipient");
    assert_string_equal(display_text, "tykgCCWvVdZS6");

    click_prev();
    assert_string_equal(display_title, "Recipient");
    assert_string_equal(display_text, "AmPWwmdgpvPRP");

    click_prev();
    assert_string_equal(display_title, "Amount");
    assert_string_equal(display_text, "1.5 AERGO");

    click_prev();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    // again backwards 2 more times

    click_prev();
    assert_string_equal(display_title, "Recipient");
    assert_string_equal(display_text, "d64mzKJ9RCAhp");

    click_prev();
    assert_string_equal(display_title, "Recipient");
    assert_string_equal(display_text, "h7b6w9UzcLcsE");

    // then forward 4 times

    click_next();
    assert_string_equal(display_title, "Recipient");
    assert_string_equal(display_text, "d64mzKJ9RCAhp");

    click_next();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    click_next();
    assert_string_equal(display_title, "Amount");
    assert_string_equal(display_text, "1.5 AERGO");

    click_next();
    assert_string_equal(display_title, "Recipient");
    assert_string_equal(display_text, "AmPWwmdgpvPRP");
}

// CALL
static void test_tx_display_3(void **state) {
    (void) state;

    // clang-format off
    uint8_t raw_tx[] = {
        // tx type
        0x05,
        // transaction
        0x08, 0x19, 0x12, 0x21, 0x02, 0x9d, 0x02, 0x05,
        0x91, 0xe7, 0xfb, 0x7b, 0x09, 0x21, 0x53, 0x68,
        0x19, 0x95, 0xf8, 0x06, 0x09, 0xf0, 0xac, 0x98,
        0x8a, 0x4d, 0x93, 0x5e, 0x0e, 0xa6, 0x3c, 0x06,
        0x0f, 0x19, 0x54, 0xb0, 0x5f, 0x1a, 0x21, 0x03,
        0x8c, 0xb9, 0x2c, 0xde, 0xbf, 0x39, 0x98, 0x69,
        0x09, 0x3c, 0xac, 0x47, 0xe3, 0x70, 0xd8, 0xa9,
        0xfa, 0x50, 0x17, 0x30, 0x42, 0x23, 0xf9, 0xad,
        0x1a, 0x8c, 0x0a, 0x05, 0xa9, 0x06, 0xa9, 0xcb,
        0x22, 0x01, 0x00, 0x2a, 0x21, 0x7b, 0x22, 0x4e,
        0x61, 0x6d, 0x65, 0x22, 0x3a, 0x22, 0x68, 0x65,
        0x6c, 0x6c, 0x6f, 0x22, 0x2c, 0x22, 0x41, 0x72,
        0x67, 0x73, 0x22, 0x3a, 0x5b, 0x22, 0x77, 0x6f,
        0x72, 0x6c, 0x64, 0x22, 0x5d, 0x7d, 0x3a, 0x01,
        0x00, 0x40, 0x05, 0x4a, 0x20, 0x52, 0x48, 0x45,
        0xc2, 0x4c, 0xd3, 0xe5, 0x3a, 0xec, 0xbc, 0xda,
        0x8e, 0x31, 0x5d, 0x62, 0xdc, 0x95, 0xa7, 0xf2,
        0xf8, 0x25, 0x48, 0x93, 0x0b, 0xc2, 0xfc, 0xc9,
        0x86, 0xbf, 0x74, 0x53, 0xbd,
    };

    int ret = setjmp(jump_buffer);
    assert_int_equal(ret, 0);

    send_transaction(raw_tx, sizeof(raw_tx));

    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

// test also: call with amount
// test also: call to default function

    //click_next();
    //assert_string_equal(display_title, "Amount");
    //assert_string_equal(display_text, "1.5 AERGO");

    click_next();
    assert_string_equal(display_title, "Contract");
    assert_string_equal(display_text, "AmPWwmdgpvPRP");

    click_next();
    assert_string_equal(display_title, "Contract");
    assert_string_equal(display_text, "tykgCCWvVdZS6");

    click_next();
    assert_string_equal(display_title, "Contract");
    assert_string_equal(display_text, "h7b6w9UzcLcsE");

    click_next();
    assert_string_equal(display_title, "Contract");
    assert_string_equal(display_text, "d64mzKJ9RCAhp");

    click_next();
    assert_string_equal(display_title, "Function");
    assert_string_equal(display_text, "hello");

    click_next();
    assert_string_equal(display_title, "Parameters");
    assert_string_equal(display_text, "\"world\"");

    click_next();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    // BACKWARDS

    click_prev();
    assert_string_equal(display_title, "Parameters");
    assert_string_equal(display_text, "\"world\"");

    click_prev();
    assert_string_equal(display_title, "Function");
    assert_string_equal(display_text, "hello");

    click_prev();
    assert_string_equal(display_title, "Contract");
    assert_string_equal(display_text, "d64mzKJ9RCAhp");

    click_prev();
    assert_string_equal(display_title, "Contract");
    assert_string_equal(display_text, "h7b6w9UzcLcsE");

    click_prev();
    assert_string_equal(display_title, "Contract");
    assert_string_equal(display_text, "tykgCCWvVdZS6");

    click_prev();
    assert_string_equal(display_title, "Contract");
    assert_string_equal(display_text, "AmPWwmdgpvPRP");

    //click_prev();
    //assert_string_equal(display_title, "Amount");
    //assert_string_equal(display_text, "1.5 AERGO");

    click_prev();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    // again backwards 2 more times

    click_prev();
    assert_string_equal(display_title, "Parameters");
    assert_string_equal(display_text, "\"world\"");

    click_prev();
    assert_string_equal(display_title, "Function");
    assert_string_equal(display_text, "hello");

    // then forward 4 times

    click_next();
    assert_string_equal(display_title, "Parameters");
    assert_string_equal(display_text, "\"world\"");

    click_next();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    click_next();
    assert_string_equal(display_title, "Contract");
    assert_string_equal(display_text, "AmPWwmdgpvPRP");

    click_next();
    assert_string_equal(display_title, "Contract");
    assert_string_equal(display_text, "tykgCCWvVdZS6");
}

// MULTICALL
static void test_tx_display_4(void **state) {
    (void) state;

    // clang-format off
    uint8_t raw_tx[] = {
        // tx type
        0x07,
        // transaction
        0x08, 0xfa, 0x01, 0x12, 0x21, 0x03, 0x8c, 0xb9,
        0x2c, 0xde, 0xbf, 0x39, 0x98, 0x69, 0x09, 0x3c,
        0xac, 0x47, 0xe3, 0x70, 0xd8, 0xa9, 0xfa, 0x50,
        0x17, 0x30, 0x42, 0x23, 0xf9, 0xad, 0x1a, 0x8c,
        0x0a, 0x05, 0xa9, 0x06, 0xa9, 0xcb, 0x22, 0x01,
        0x00, 0x2a, 0x4e, 0x5b, 0x5b, 0x22, 0x6c, 0x65,
        0x74, 0x22, 0x2c, 0x22, 0x6f, 0x62, 0x6a, 0x22,
        0x2c, 0x7b, 0x22, 0x6f, 0x6e, 0x65, 0x22, 0x3a,
        0x31, 0x2c, 0x22, 0x74, 0x77, 0x6f, 0x22, 0x3a,
        0x32, 0x7d, 0x5d, 0x2c, 0x5b, 0x22, 0x73, 0x65,
        0x74, 0x22, 0x2c, 0x22, 0x25, 0x6f, 0x62, 0x6a,
        0x25, 0x22, 0x2c, 0x22, 0x74, 0x68, 0x72, 0x65,
        0x65, 0x22, 0x2c, 0x33, 0x5d, 0x2c, 0x5b, 0x22,
        0x72, 0x65, 0x74, 0x75, 0x72, 0x6e, 0x22, 0x2c,
        0x22, 0x25, 0x6f, 0x62, 0x6a, 0x25, 0x22, 0x5d,
        0x5d, 0x3a, 0x01, 0x00, 0x40, 0x07, 0x4a, 0x20,
        0x52, 0x48, 0x45, 0xc2, 0x4c, 0xd3, 0xe5, 0x3a,
        0xec, 0xbc, 0xda, 0x8e, 0x31, 0x5d, 0x62, 0xdc,
        0x95, 0xa7, 0xf2, 0xf8, 0x25, 0x48, 0x93, 0x0b,
        0xc2, 0xfc, 0xc9, 0x86, 0xbf, 0x74, 0x53, 0xbd,
    };

    int ret = setjmp(jump_buffer);
    assert_int_equal(ret, 0);

    send_transaction(raw_tx, sizeof(raw_tx));

    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "=> let");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "obj");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "{\"one\":1,\"two");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "\":2}");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "=> set");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "%obj%");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "three");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "3");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "=> return");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "%obj%");

    click_next();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    // BACKWARDS

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "%obj%");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "=> return");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "3");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "three");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "%obj%");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "=> set");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "\":2}");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "{\"one\":1,\"two");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "obj");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "=> let");

    click_prev();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    // again backwards 2 more times

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "%obj%");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "=> return");

    // then forward 4 times

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "%obj%");

    click_next();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "=> let");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "obj");
}

// DEPLOY
static void test_tx_display_5(void **state) {
    (void) state;

    // clang-format off
    uint8_t raw_tx[] = {
        // tx type
        0x06,
        // transaction
        0x08, 0x81, 0x08, 0x12, 0x21, 0x03, 0x8c, 0xb9,
        0x2c, 0xde, 0xbf, 0x39, 0x98, 0x69, 0x09, 0x3c,
        0xac, 0x47, 0xe3, 0x70, 0xd8, 0xa9, 0xfa, 0x50,
        0x17, 0x30, 0x42, 0x23, 0xf9, 0xad, 0x1a, 0x8c,
        0x0a, 0x05, 0xa9, 0x06, 0xa9, 0xcb, 0x22, 0x01,
        0x00, 0x2a, 0x40, 0x30, 0x31, 0x30, 0x32, 0x30,
        0x33, 0x30, 0x34, 0x30, 0x35, 0x30, 0x36, 0x30,
        0x37, 0x30, 0x38, 0x30, 0x39, 0x30, 0x41, 0x30,
        0x42, 0x30, 0x43, 0x30, 0x44, 0x30, 0x45, 0x30,
        0x46, 0x46, 0x46, 0x30, 0x31, 0x30, 0x32, 0x30,
        0x33, 0x30, 0x34, 0x30, 0x35, 0x30, 0x36, 0x30,
        0x37, 0x30, 0x38, 0x30, 0x39, 0x30, 0x41, 0x30,
        0x42, 0x30, 0x43, 0x30, 0x44, 0x30, 0x45, 0x30,
        0x46, 0x46, 0x46, 0x3a, 0x01, 0x00, 0x40, 0x06,
        0x4a, 0x20, 0x52, 0x48, 0x45, 0xc2, 0x4c, 0xd3,
        0xe5, 0x3a, 0xec, 0xbc, 0xda, 0x8e, 0x31, 0x5d,
        0x62, 0xdc, 0x95, 0xa7, 0xf2, 0xf8, 0x25, 0x48,
        0x93, 0x0b, 0xc2, 0xfc, 0xc9, 0x86, 0xbf, 0x74,
        0x53, 0xbd,
    };

    int ret = setjmp(jump_buffer);
    assert_int_equal(ret, 0);

    send_transaction(raw_tx, sizeof(raw_tx));

    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

// test also: deploy with amount
// test also: deploy with parameters to constructor

    //click_next();
    //assert_string_equal(display_title, "Amount");
    //assert_string_equal(display_text, "1.5 AERGO");

    click_next();
    assert_string_equal(display_title, "New Contract 1/6");
    assert_string_equal(display_text, "EF6F33393A9F");

    click_next();
    assert_string_equal(display_title, "New Contract 2/6");
    assert_string_equal(display_text, "1C1837A12CAD");

    click_next();
    assert_string_equal(display_title, "New Contract 3/6");
    assert_string_equal(display_text, "803E9F4BBA89");

    click_next();
    assert_string_equal(display_title, "New Contract 4/6");
    assert_string_equal(display_text, "5B1E78F24C1D");

    click_next();
    assert_string_equal(display_title, "New Contract 5/6");
    assert_string_equal(display_text, "BDCF6B5C7CA3");

    click_next();
    assert_string_equal(display_title, "New Contract 6/6");
    assert_string_equal(display_text, "AD41");

    click_next();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    // BACKWARDS

    click_prev();
    assert_string_equal(display_title, "New Contract 6/6");
    assert_string_equal(display_text, "AD41");

    click_prev();
    assert_string_equal(display_title, "New Contract 5/6");
    assert_string_equal(display_text, "BDCF6B5C7CA3");

    click_prev();
    assert_string_equal(display_title, "New Contract 4/6");
    assert_string_equal(display_text, "5B1E78F24C1D");

    click_prev();
    assert_string_equal(display_title, "New Contract 3/6");
    assert_string_equal(display_text, "803E9F4BBA89");

    click_prev();
    assert_string_equal(display_title, "New Contract 2/6");
    assert_string_equal(display_text, "1C1837A12CAD");

    click_prev();
    assert_string_equal(display_title, "New Contract 1/6");
    assert_string_equal(display_text, "EF6F33393A9F");

    click_prev();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    // again backwards 2 more times

    click_prev();
    assert_string_equal(display_title, "New Contract 6/6");
    assert_string_equal(display_text, "AD41");

    click_prev();
    assert_string_equal(display_title, "New Contract 5/6");
    assert_string_equal(display_text, "BDCF6B5C7CA3");

    // then forward 4 times

    click_next();
    assert_string_equal(display_title, "New Contract 6/6");
    assert_string_equal(display_text, "AD41");

    click_next();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    click_next();
    assert_string_equal(display_title, "New Contract 1/6");
    assert_string_equal(display_text, "EF6F33393A9F");

    click_next();
    assert_string_equal(display_title, "New Contract 2/6");
    assert_string_equal(display_text, "1C1837A12CAD");
}

// GOVERNANCE
static void test_tx_display_6(void **state) {
    (void) state;

    // clang-format off
    uint8_t raw_tx[] = {
        // tx type
        0x01,
        // transaction
        0x08, 0x82, 0x10, 0x12, 0x21, 0x03, 0x8c, 0xb9,
        0x2c, 0xde, 0xbf, 0x39, 0x98, 0x69, 0x09, 0x3c,
        0xac, 0x47, 0xe3, 0x70, 0xd8, 0xa9, 0xfa, 0x50,
        0x17, 0x30, 0x42, 0x23, 0xf9, 0xad, 0x1a, 0x8c,
        0x0a, 0x05, 0xa9, 0x06, 0xa9, 0xcb, 0x1a, 0x10,
        0x61, 0x65, 0x72, 0x67, 0x6f, 0x2e, 0x65, 0x6e,
        0x74, 0x65, 0x72, 0x70, 0x72, 0x69, 0x73, 0x65,
        0x22, 0x01, 0x00, 0x2a, 0x56, 0x7b, 0x22, 0x4e,
        0x61, 0x6d, 0x65, 0x22, 0x3a, 0x22, 0x61, 0x70,
        0x70, 0x65, 0x6e, 0x64, 0x41, 0x64, 0x6d, 0x69,
        0x6e, 0x22, 0x2c, 0x22, 0x41, 0x72, 0x67, 0x73,
        0x22, 0x3a, 0x5b, 0x22, 0x41, 0x6d, 0x4d, 0x44,
        0x45, 0x79, 0x63, 0x33, 0x36, 0x46, 0x4e, 0x58,
        0x42, 0x33, 0x46, 0x71, 0x31, 0x61, 0x36, 0x31,
        0x48, 0x65, 0x56, 0x4a, 0x52, 0x54, 0x34, 0x79,
        0x73, 0x73, 0x4d, 0x45, 0x50, 0x31, 0x31, 0x4e,
        0x57, 0x57, 0x45, 0x39, 0x51, 0x78, 0x38, 0x79,
        0x68, 0x66, 0x52, 0x4b, 0x65, 0x78, 0x76, 0x71,
        0x22, 0x5d, 0x7d, 0x3a, 0x01, 0x00, 0x40, 0x01,
        0x4a, 0x20, 0x52, 0x48, 0x45, 0xc2, 0x4c, 0xd3,
        0xe5, 0x3a, 0xec, 0xbc, 0xda, 0x8e, 0x31, 0x5d,
        0x62, 0xdc, 0x95, 0xa7, 0xf2, 0xf8, 0x25, 0x48,
        0x93, 0x0b, 0xc2, 0xfc, 0xc9, 0x86, 0xbf, 0x74,
        0x53, 0xbd,
    };

    int ret = setjmp(jump_buffer);
    assert_int_equal(ret, 0);

    send_transaction(raw_tx, sizeof(raw_tx));

    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    click_next();
    assert_string_equal(display_title, "Add Admin");
    assert_string_equal(display_text, "\"AmMDEyc36FNX");

    click_next();
    assert_string_equal(display_title, "Add Admin");
    assert_string_equal(display_text, "B3Fq1a61HeVJR");

    click_next();
    assert_string_equal(display_title, "Add Admin");
    assert_string_equal(display_text, "T4yssMEP11NWW");

    click_next();
    assert_string_equal(display_title, "Add Admin");
    assert_string_equal(display_text, "E9Qx8yhfRKexv");

    click_next();
    assert_string_equal(display_title, "Add Admin");
    assert_string_equal(display_text, "q\"");

    click_next();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    // BACKWARDS

    click_prev();
    assert_string_equal(display_title, "Add Admin");
    assert_string_equal(display_text, "q\"");

    click_prev();
    assert_string_equal(display_title, "Add Admin");
    assert_string_equal(display_text, "E9Qx8yhfRKexv");

    click_prev();
    assert_string_equal(display_title, "Add Admin");
    assert_string_equal(display_text, "T4yssMEP11NWW");

    click_prev();
    assert_string_equal(display_title, "Add Admin");
    assert_string_equal(display_text, "B3Fq1a61HeVJR");

    click_prev();
    assert_string_equal(display_title, "Add Admin");
    assert_string_equal(display_text, "\"AmMDEyc36FNX");

    click_prev();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    // again backwards 2 more times

    click_prev();
    assert_string_equal(display_title, "Add Admin");
    assert_string_equal(display_text, "q\"");

    click_prev();
    assert_string_equal(display_title, "Add Admin");
    assert_string_equal(display_text, "E9Qx8yhfRKexv");

    // then forward 4 times

    click_next();
    assert_string_equal(display_title, "Add Admin");
    assert_string_equal(display_text, "q\"");

    click_next();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    click_next();
    assert_string_equal(display_title, "Add Admin");
    assert_string_equal(display_text, "\"AmMDEyc36FNX");

    click_next();
    assert_string_equal(display_title, "Add Admin");
    assert_string_equal(display_text, "B3Fq1a61HeVJR");
}

// payload: test<0102AF5D>when in hex

int main() {
    const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_tx_display_1),
      cmocka_unit_test(test_tx_display_2),
      cmocka_unit_test(test_tx_display_3),
      cmocka_unit_test(test_tx_display_4),
      cmocka_unit_test(test_tx_display_5),
      cmocka_unit_test(test_tx_display_6),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
