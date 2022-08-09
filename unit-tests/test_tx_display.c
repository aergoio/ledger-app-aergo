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
#define STATIC_SCREEN   1
#define DYNAMIC_SCREEN  2

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

static void send_transaction(const unsigned char *buf, unsigned int len){

  memset(&txn, 0, sizeof(struct txn));

  txn_ptr = (unsigned char *) buf;
  txn_size = len;
  txn_offset = 0;

  requested_part = FIRST_PART;
  check_send_txn_part();

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
// TEST CASE GENERATOR
////////////////////////////////////////////////////////////////////////////////

char quote_buf[32];

char *quote(char *str) {
  char *dest = quote_buf;
  while (*str) {
    char c = *str++;
    if (c == '"' || c == '\\') {
      *dest++ = '\\';
    }
    *dest++ = c;
  }
  *dest++ = 0;
  return quote_buf;
}

static void generate_test_case() {

  assert_string_equal(display_title, "Review");
  assert_string_equal(display_text, "Transaction");

  printf("\n");

  printf("    assert_string_equal(display_title, \"%s\");\n", quote(display_title));
  printf("    assert_string_equal(display_text, \"%s\");\n", quote(display_text));
  printf("\n");

  int i;

  do {
    click_next();
    printf("    click_next();\n");
    printf("    assert_string_equal(display_title, \"%s\");\n", quote(display_title));
    printf("    assert_string_equal(display_text, \"%s\");\n", quote(display_text));
    printf("\n");
  } while (strcmp(display_title,"Review")!=0);

  puts("    // BACKWARDS");
  puts("");

  do {
    click_prev();
    printf("    click_prev();\n");
    printf("    assert_string_equal(display_title, \"%s\");\n", quote(display_title));
    printf("    assert_string_equal(display_text, \"%s\");\n", quote(display_text));
    printf("\n");
  } while (strcmp(display_title,"Review")!=0);

  puts("    // again backwards 2 more times");
  puts("");

  for(i=0; i<2; i++){
    click_prev();
    printf("    click_prev();\n");
    printf("    assert_string_equal(display_title, \"%s\");\n", quote(display_title));
    printf("    assert_string_equal(display_text, \"%s\");\n", quote(display_text));
    printf("\n");
  }

  puts("    // then forward 4 times");
  puts("");

  for(i=0; i<4; i++){
    click_next();
    printf("    click_next();\n");
    printf("    assert_string_equal(display_title, \"%s\");\n", quote(display_title));
    printf("    assert_string_equal(display_text, \"%s\");\n", quote(display_text));
    printf("\n");
  }

}

////////////////////////////////////////////////////////////////////////////////
// TEST CASES
////////////////////////////////////////////////////////////////////////////////

// NORMAL / LEGACY
static void test_tx_display_normal(void **state) {
    (void) state;

    // clang-format off
    uint8_t raw_tx[] = {
        // tx type
        0x00,
        // transaction
        0x08, 0x01, 0x12, 0x21, 0x02, 0x9d, 0x02, 0x05,
        0x91, 0xe7, 0xfb, 0x7b, 0x09, 0x21, 0x53, 0x68,
        0x19, 0x95, 0xf8, 0x06, 0x09, 0xf0, 0xac, 0x98,
        0x8a, 0x4d, 0x93, 0x5e, 0x0e, 0xa6, 0x3c, 0x06,
        0x0f, 0x19, 0x54, 0xb0, 0x5f, 0x1a, 0x21, 0x02,
        0x5d, 0x22, 0x30, 0xba, 0x75, 0x21, 0x7e, 0x60,
        0x37, 0x99, 0xe9, 0xa3, 0xd5, 0xb9, 0x1a, 0x63,
        0x61, 0x48, 0x3f, 0x9d, 0xa7, 0x37, 0x96, 0x41,
        0x0f, 0x6b, 0xc1, 0xce, 0x58, 0x01, 0xfd, 0xf2,
        0x22, 0x09, 0x06, 0xb1, 0x4b, 0xd1, 0xe6, 0xee,
        0xa0, 0x00, 0x00, 0x2a, 0x31, 0x00, 0x01, 0x02,
        0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a,
        0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12,
        0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a,
        0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22,
        0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a,
        0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30,
        0x3a, 0x01, 0x00, 0x4a, 0x20, 0x52, 0x48,
        0x45, 0xc2, 0x4c, 0xd3, 0xe5, 0x3a, 0xec, 0xbc,
        0xda, 0x8e, 0x31, 0x5d, 0x62, 0xdc, 0x95, 0xa7,
        0xf2, 0xf8, 0x25, 0x48, 0x93, 0x0b, 0xc2, 0xfc,
        0xc9, 0x86, 0xbf, 0x74, 0x53, 0xbd,
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
    assert_string_equal(display_text, "<000102030405");

    click_next();
    assert_string_equal(display_title, "Payload");
    assert_string_equal(display_text, "060708090A0B0");

    click_next();
    assert_string_equal(display_title, "Payload");
    assert_string_equal(display_text, "C0D0E0F101112");

    click_next();
    assert_string_equal(display_title, "Payload");
    assert_string_equal(display_text, "1314151617181");

    click_next();
    assert_string_equal(display_title, "Payload");
    assert_string_equal(display_text, "91A1B1C1D1E1F");

    click_next();
    assert_string_equal(display_title, "Payload");
    assert_string_equal(display_text, "> !\"#$%&'()*+");

    click_next();
    assert_string_equal(display_title, "Payload");
    assert_string_equal(display_text, ",-./0");

    click_next();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    // BACKWARDS

    click_prev();
    assert_string_equal(display_title, "Payload");
    assert_string_equal(display_text, ",-./0");

    click_prev();
    assert_string_equal(display_title, "Payload");
    assert_string_equal(display_text, "> !\"#$%&'()*+");

    click_prev();
    assert_string_equal(display_title, "Payload");
    assert_string_equal(display_text, "91A1B1C1D1E1F");

    click_prev();
    assert_string_equal(display_title, "Payload");
    assert_string_equal(display_text, "1314151617181");

    click_prev();
    assert_string_equal(display_title, "Payload");
    assert_string_equal(display_text, "C0D0E0F101112");

    click_prev();
    assert_string_equal(display_title, "Payload");
    assert_string_equal(display_text, "060708090A0B0");

    click_prev();
    assert_string_equal(display_title, "Payload");
    assert_string_equal(display_text, "<000102030405");

    click_prev();
    assert_string_equal(display_title, "Recipient");
    assert_string_equal(display_text, "9Qx8yhfRKexvq");

    click_prev();
    assert_string_equal(display_title, "Recipient");
    assert_string_equal(display_text, "4yssMEP11NWWE");

    click_prev();
    assert_string_equal(display_title, "Recipient");
    assert_string_equal(display_text, "3Fq1a61HeVJRT");

    click_prev();
    assert_string_equal(display_title, "Recipient");
    assert_string_equal(display_text, "AmMDEyc36FNXB");

    click_prev();
    assert_string_equal(display_title, "Amount");
    assert_string_equal(display_text, "123.456 AERGO");

    click_prev();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    // forward

    click_next();
    assert_string_equal(display_title, "Amount");
    assert_string_equal(display_text, "123.456 AERGO");

    click_next();
    assert_string_equal(display_title, "Recipient");
    assert_string_equal(display_text, "AmMDEyc36FNXB");
}

// NORMAL / LEGACY
static void test_tx_display_normal_long_payload(void **state) {
    (void) state;

    // clang-format off
    uint8_t raw_tx[] = {
        // tx type
        0x00,
        // transaction
        0x08, 0x82, 0x20, 0x12, 0x21, 0x02, 0x9d, 0x02,
        0x05, 0x91, 0xe7, 0xfb, 0x7b, 0x09, 0x21, 0x53,
        0x68, 0x19, 0x95, 0xf8, 0x06, 0x09, 0xf0, 0xac,
        0x98, 0x8a, 0x4d, 0x93, 0x5e, 0x0e, 0xa6, 0x3c,
        0x06, 0x0f, 0x19, 0x54, 0xb0, 0x5f, 0x1a, 0x21,
        0x02, 0x5d, 0x22, 0x30, 0xba, 0x75, 0x21, 0x7e,
        0x60, 0x37, 0x99, 0xe9, 0xa3, 0xd5, 0xb9, 0x1a,
        0x63, 0x61, 0x48, 0x3f, 0x9d, 0xa7, 0x37, 0x96,
        0x41, 0x0f, 0x6b, 0xc1, 0xce, 0x58, 0x01, 0xfd,
        0xf2, 0x22, 0x01, 0x01, 0x2a, 0x9b, 0x01, 0x54,
        0x65, 0x73, 0x74, 0x69, 0x6e, 0x67, 0x20, 0x61,
        0x20, 0x6c, 0x6f, 0x6e, 0x67, 0x20, 0x70, 0x61,
        0x79, 0x6c, 0x6f, 0x61, 0x64, 0x20, 0x74, 0x65,
        0x78, 0x74, 0x20, 0x69, 0x6e, 0x20, 0x77, 0x68,
        0x69, 0x63, 0x68, 0x20, 0x6f, 0x6e, 0x6c, 0x79,
        0x20, 0x74, 0x68, 0x65, 0x20, 0x66, 0x69, 0x72,
        0x73, 0x74, 0x20, 0x70, 0x61, 0x72, 0x74, 0x20,
        0x77, 0x69, 0x6c, 0x6c, 0x20, 0x62, 0x65, 0x20,
        0x64, 0x69, 0x73, 0x70, 0x6c, 0x61, 0x79, 0x65,
        0x64, 0x2c, 0x20, 0x61, 0x6e, 0x64, 0x20, 0x61,
        0x6c, 0x6c, 0x20, 0x74, 0x68, 0x65, 0x20, 0x72,
        0x65, 0x6d, 0x61, 0x69, 0x6e, 0x69, 0x6e, 0x67,
        0x20, 0x77, 0x69, 0x6c, 0x6c, 0x20, 0x62, 0x65,
        0x20, 0x68, 0x69, 0x64, 0x64, 0x65, 0x6e, 0x20,
        0x62, 0x75, 0x74, 0x20, 0x74, 0x68, 0x65, 0x20,
        0x74, 0x78, 0x6e, 0x20, 0x68, 0x61, 0x73, 0x68,
        0x20, 0x77, 0x69, 0x6c, 0x6c, 0x20, 0x62, 0x65,
        0x20, 0x63, 0x6f, 0x6d, 0x70, 0x75, 0x74, 0x65,
        0x64, 0x20, 0x70, 0x72, 0x6f, 0x70, 0x65, 0x72,
        0x6c, 0x79, 0x3a, 0x01, 0x00, 0x4a, 0x20, 0x52,
        0x48, 0x45, 0xc2, 0x4c, 0xd3, 0xe5, 0x3a, 0xec,
        0xbc, 0xda, 0x8e, 0x31, 0x5d, 0x62, 0xdc, 0x95,
        0xa7, 0xf2, 0xf8, 0x25, 0x48, 0x93, 0x0b, 0xc2,
        0xfc, 0xc9, 0x86, 0xbf, 0x74, 0x53, 0xbd,
    };

    int ret = setjmp(jump_buffer);
    assert_int_equal(ret, 0);

    send_transaction(raw_tx, sizeof(raw_tx));

    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    click_next();
    assert_string_equal(display_title, "Amount");
    assert_string_equal(display_text, "0.00000000000");

    click_next();
    assert_string_equal(display_title, "Amount");
    assert_string_equal(display_text, "0000001 AERGO");

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
    assert_string_equal(display_text, "Testing a lon");

    click_next();
    assert_string_equal(display_title, "Payload");
    assert_string_equal(display_text, "g payload tex");

    click_next();
    assert_string_equal(display_title, "Payload");
    assert_string_equal(display_text, "t in which on");

    click_next();
    assert_string_equal(display_title, "Payload");
    assert_string_equal(display_text, "ly the first ");

    click_next();
    assert_string_equal(display_title, "Payload");
    assert_string_equal(display_text, "part will ...");

    click_next();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    // BACKWARDS

    click_prev();
    assert_string_equal(display_title, "Payload");
    assert_string_equal(display_text, "part will ...");

    click_prev();
    assert_string_equal(display_title, "Payload");
    assert_string_equal(display_text, "ly the first ");

    click_prev();
    assert_string_equal(display_title, "Payload");
    assert_string_equal(display_text, "t in which on");

    click_prev();
    assert_string_equal(display_title, "Payload");
    assert_string_equal(display_text, "g payload tex");

    click_prev();
    assert_string_equal(display_title, "Payload");
    assert_string_equal(display_text, "Testing a lon");

    click_prev();
    assert_string_equal(display_title, "Recipient");
    assert_string_equal(display_text, "9Qx8yhfRKexvq");

    click_prev();
    assert_string_equal(display_title, "Recipient");
    assert_string_equal(display_text, "4yssMEP11NWWE");

    click_prev();
    assert_string_equal(display_title, "Recipient");
    assert_string_equal(display_text, "3Fq1a61HeVJRT");

    click_prev();
    assert_string_equal(display_title, "Recipient");
    assert_string_equal(display_text, "AmMDEyc36FNXB");

    click_prev();
    assert_string_equal(display_title, "Amount");
    assert_string_equal(display_text, "0000001 AERGO");

    click_prev();
    assert_string_equal(display_title, "Amount");
    assert_string_equal(display_text, "0.00000000000");

    click_prev();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    // BACKWARDS AGAIN

    click_prev();
    assert_string_equal(display_title, "Payload");
    assert_string_equal(display_text, "part will ...");

    click_prev();
    assert_string_equal(display_title, "Payload");
    assert_string_equal(display_text, "ly the first ");

    // FORWARD

    click_next();
    assert_string_equal(display_title, "Payload");
    assert_string_equal(display_text, "part will ...");

    click_next();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    click_next();
    assert_string_equal(display_title, "Amount");
    assert_string_equal(display_text, "0.00000000000");

    click_next();
    assert_string_equal(display_title, "Amount");
    assert_string_equal(display_text, "0000001 AERGO");
}

// TRANSFER
static void test_tx_display_transfer_1(void **state) {
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

// TRANSFER WITH PAYLOAD - with UTF8 chars
// Also without amount & without gasPrice
static void test_tx_display_transfer_2(void **state) {
    (void) state;

    // clang-format off
    uint8_t raw_tx[] = {
        // tx type
        0x04,
        // transaction
        0x08, 0x80, 0x02, 0x12, 0x21, 0x02, 0x9d, 0x02,
        0x05, 0x91, 0xe7, 0xfb, 0x7b, 0x09, 0x21, 0x53,
        0x68, 0x19, 0x95, 0xf8, 0x06, 0x09, 0xf0, 0xac,
        0x98, 0x8a, 0x4d, 0x93, 0x5e, 0x0e, 0xa6, 0x3c,
        0x06, 0x0f, 0x19, 0x54, 0xb0, 0x5f, 0x1a, 0x0a,
        0x63, 0x72, 0x79, 0x70, 0x74, 0x6f, 0x62, 0x6f,
        0x73, 0x73, 0x2a, 0x1c, 0x41,
        0xc3, 0xa7, 0xc3, 0xa3, 0x6f, 0x20, 0xc3, 0xa0,
        0x20, 0x70, 0xc3, 0xa9, 0x09, 0x0d, 0x0a, 0xed,
        0x85, 0x8c, 0xec, 0x8a, 0xa4, 0xed, 0x8a, 0xb8,
        0xeb, 0x84, 0xb7, 0x40, 0x04,
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
    assert_string_equal(display_text, "0 AERGO");

    click_next();
    assert_string_equal(display_title, "Recipient");
    assert_string_equal(display_text, "cryptoboss");

    click_next();
    assert_string_equal(display_title, "Payload");
    assert_string_equal(display_text, "A\\uE7\\uE3o \\u");

    click_next();
    assert_string_equal(display_title, "Payload");
    assert_string_equal(display_text, "E0 p\\uE9 ||\\u");

    click_next();
    assert_string_equal(display_title, "Payload");
    assert_string_equal(display_text, "D14C\\uC2A4\\uD");

    click_next();
    assert_string_equal(display_title, "Payload");
    assert_string_equal(display_text, "2B8\\uB137");

    click_next();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    // BACKWARDS

    click_prev();
    assert_string_equal(display_title, "Payload");
    assert_string_equal(display_text, "2B8\\uB137");

    click_prev();
    assert_string_equal(display_title, "Payload");
    assert_string_equal(display_text, "D14C\\uC2A4\\uD");

    click_prev();
    assert_string_equal(display_title, "Payload");
    assert_string_equal(display_text, "E0 p\\uE9 ||\\u");

    click_prev();
    assert_string_equal(display_title, "Payload");
    assert_string_equal(display_text, "A\\uE7\\uE3o \\u");

    click_prev();
    assert_string_equal(display_title, "Recipient");
    assert_string_equal(display_text, "cryptoboss");

    click_prev();
    assert_string_equal(display_title, "Amount");
    assert_string_equal(display_text, "0 AERGO");

    click_prev();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    // again backwards 2 more times

    click_prev();
    assert_string_equal(display_title, "Payload");
    assert_string_equal(display_text, "2B8\\uB137");

    click_prev();
    assert_string_equal(display_title, "Payload");
    assert_string_equal(display_text, "D14C\\uC2A4\\uD");

    // then forward 4 times

    click_next();
    assert_string_equal(display_title, "Payload");
    assert_string_equal(display_text, "2B8\\uB137");

    click_next();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    click_next();
    assert_string_equal(display_title, "Amount");
    assert_string_equal(display_text, "0 AERGO");

    click_next();
    assert_string_equal(display_title, "Recipient");
    assert_string_equal(display_text, "cryptoboss");

}

// CALL
static void test_tx_display_call_1(void **state) {
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

// CALL
static void test_tx_display_call_2(void **state) {
    (void) state;

    // clang-format off
    uint8_t raw_tx[] = {
        // tx type
        0x05,
        // transaction
        0x08, 0x80, 0x04, 0x12, 0x21, 0x02, 0x9d, 0x02,
        0x05, 0x91, 0xe7, 0xfb, 0x7b, 0x09, 0x21, 0x53,
        0x68, 0x19, 0x95, 0xf8, 0x06, 0x09, 0xf0, 0xac,
        0x98, 0x8a, 0x4d, 0x93, 0x5e, 0x0e, 0xa6, 0x3c,
        0x06, 0x0f, 0x19, 0x54, 0xb0, 0x5f, 0x1a, 0x21,
        0x03, 0x8c, 0xb9, 0x2c, 0xde, 0xbf, 0x39, 0x98,
        0x69, 0x09, 0x3c, 0xac, 0x47, 0xe3, 0x70, 0xd8,
        0xa9, 0xfa, 0x50, 0x17, 0x30, 0x42, 0x23, 0xf9,
        0xad, 0x1a, 0x8c, 0x0a, 0x05, 0xa9, 0x06, 0xa9,
        0xcb, 0x22, 0x08, 0x01, 0xb6, 0x9b, 0x4b, 0xa6,
        0x30, 0xf3, 0x4e, 0x3a, 0x01, 0x00, 0x40, 0x05,
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
    assert_string_equal(display_text, "0.12345678901");

    click_next();
    assert_string_equal(display_title, "Amount");
    assert_string_equal(display_text, "2345678 AERGO");

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
    assert_string_equal(display_text, "default");

    click_next();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    // BACKWARDS

    click_prev();
    assert_string_equal(display_title, "Function");
    assert_string_equal(display_text, "default");

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

    click_prev();
    assert_string_equal(display_title, "Amount");
    assert_string_equal(display_text, "2345678 AERGO");

    click_prev();
    assert_string_equal(display_title, "Amount");
    assert_string_equal(display_text, "0.12345678901");

    click_prev();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    // again backwards 2 more times

    click_prev();
    assert_string_equal(display_title, "Function");
    assert_string_equal(display_text, "default");

    click_prev();
    assert_string_equal(display_title, "Contract");
    assert_string_equal(display_text, "d64mzKJ9RCAhp");

    // then forward 4 times

    click_next();
    assert_string_equal(display_title, "Function");
    assert_string_equal(display_text, "default");

    click_next();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    click_next();
    assert_string_equal(display_title, "Amount");
    assert_string_equal(display_text, "0.12345678901");

    click_next();
    assert_string_equal(display_title, "Amount");
    assert_string_equal(display_text, "2345678 AERGO");
}

// CALL
static void test_tx_display_call_big(void **state) {
    (void) state;

    // clang-format off
    uint8_t raw_tx[] = {
        // tx type
        0x05,
        // transaction
        0x08, 0x80, 0x04, 0x12, 0x21, 0x02, 0x9d, 0x02,
        0x05, 0x91, 0xe7, 0xfb, 0x7b, 0x09, 0x21, 0x53,
        0x68, 0x19, 0x95, 0xf8, 0x06, 0x09, 0xf0, 0xac,
        0x98, 0x8a, 0x4d, 0x93, 0x5e, 0x0e, 0xa6, 0x3c,
        0x06, 0x0f, 0x19, 0x54, 0xb0, 0x5f, 0x1a, 0x21,
        0x03, 0x8c, 0xb9, 0x2c, 0xde, 0xbf, 0x39, 0x98,
        0x69, 0x09, 0x3c, 0xac, 0x47, 0xe3, 0x70, 0xd8,
        0xa9, 0xfa, 0x50, 0x17, 0x30, 0x42, 0x23, 0xf9,
        0xad, 0x1a, 0x8c, 0x0a, 0x05, 0xa9, 0x06, 0xa9,
        0xcb, 0x22, 0x0c, 0x03, 0xfd, 0x35, 0xeb, 0x6d,
        0x79, 0x7a, 0x91, 0xbe, 0x38, 0xf3, 0x4e, 0x2a,
        0xa2, 0x02, 0x7b, 0x22, 0x4e, 0x61, 0x6d, 0x65,
        0x22, 0x3a, 0x22, 0x73, 0x6d, 0x61, 0x72, 0x74,
        0x5f, 0x63, 0x6f, 0x6e, 0x74, 0x72, 0x61, 0x63,
        0x74, 0x5f, 0x66, 0x75, 0x6e, 0x63, 0x74, 0x69,
        0x6f, 0x6e, 0x5f, 0x6e, 0x61, 0x6d, 0x65, 0x22,
        0x2c, 0x22, 0x41, 0x72, 0x67, 0x73, 0x22, 0x3a,
        0x5b, 0x22, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67,
        0x20, 0x70, 0x61, 0x72, 0x61, 0x6d, 0x65, 0x74,
        0x65, 0x72, 0x20, 0x77, 0x69, 0x74, 0x68, 0x20,
        0x73, 0x70, 0x61, 0x63, 0x65, 0x73, 0x22, 0x2c,
        0x31, 0x32, 0x33, 0x2c, 0x32, 0x2e, 0x35, 0x2c,
        0x74, 0x72, 0x75, 0x65, 0x2c, 0x5b, 0x31, 0x31,
        0x2c, 0x22, 0x32, 0x32, 0x22, 0x2c, 0x33, 0x2e,
        0x33, 0x5d, 0x2c, 0x7b, 0x22, 0x6f, 0x6e, 0x65,
        0x22, 0x3a, 0x31, 0x2c, 0x22, 0x74, 0x77, 0x6f,
        0x22, 0x3a, 0x32, 0x7d, 0x2c, 0x7b, 0x22, 0x66,
        0x72, 0x6f, 0x6d, 0x22, 0x3a, 0x22, 0x41, 0x6d,
        0x4d, 0x44, 0x45, 0x79, 0x63, 0x33, 0x36, 0x46,
        0x4e, 0x58, 0x42, 0x33, 0x46, 0x71, 0x31, 0x61,
        0x36, 0x31, 0x48, 0x65, 0x56, 0x4a, 0x52, 0x54,
        0x34, 0x79, 0x73, 0x73, 0x4d, 0x45, 0x50, 0x31,
        0x31, 0x4e, 0x57, 0x57, 0x45, 0x39, 0x51, 0x78,
        0x38, 0x79, 0x68, 0x66, 0x52, 0x4b, 0x65, 0x78,
        0x76, 0x71, 0x22, 0x2c, 0x22, 0x74, 0x6f, 0x22,
        0x3a, 0x22, 0x41, 0x6d, 0x50, 0x34, 0x41, 0x59,
        0x57, 0x48, 0x4b, 0x72, 0x78, 0x6e, 0x50, 0x71,
        0x76, 0x6f, 0x55, 0x41, 0x54, 0x79, 0x4a, 0x68,
        0x4d, 0x77, 0x61, 0x72, 0x7a, 0x4a, 0x41, 0x70,
        0x68, 0x57, 0x64, 0x6b, 0x6f, 0x73, 0x7a, 0x32,
        0x34, 0x41, 0x57, 0x67, 0x69, 0x44, 0x32, 0x73,
        0x51, 0x31, 0x38, 0x73, 0x69, 0x39, 0x22, 0x2c,
        0x22, 0x68, 0x61, 0x73, 0x68, 0x22, 0x3a, 0x22,
        0x30, 0x31, 0x30, 0x32, 0x30, 0x33, 0x30, 0x34,
        0x30, 0x35, 0x30, 0x36, 0x30, 0x37, 0x30, 0x38,
        0x30, 0x39, 0x30, 0x41, 0x30, 0x42, 0x30, 0x43,
        0x30, 0x44, 0x30, 0x45, 0x30, 0x46, 0x46, 0x46,
        0x22, 0x7d, 0x5d, 0x7d, 0x3a, 0x01, 0x00, 0x40,
        0x05, 0x4a, 0x20, 0x52, 0x48, 0x45, 0xc2, 0x4c,
        0xd3, 0xe5, 0x3a, 0xec, 0xbc, 0xda, 0x8e, 0x31,
        0x5d, 0x62, 0xdc, 0x95, 0xa7, 0xf2, 0xf8, 0x25,
        0x48, 0x93, 0x0b, 0xc2, 0xfc, 0xc9, 0x86, 0xbf,
        0x74, 0x53, 0xbd,
    };

    int ret = setjmp(jump_buffer);
    assert_int_equal(ret, 0);

    send_transaction(raw_tx, sizeof(raw_tx));

    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    click_next();
    assert_string_equal(display_title, "Amount");
    assert_string_equal(display_text, "1234567890.12");

    click_next();
    assert_string_equal(display_title, "Amount");
    assert_string_equal(display_text, "3456789012345");

    click_next();
    assert_string_equal(display_title, "Amount");
    assert_string_equal(display_text, "678 AERGO");

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
    assert_string_equal(display_text, "smart_contrac");

    click_next();
    assert_string_equal(display_title, "Function");
    assert_string_equal(display_text, "t_function_na");

    click_next();
    assert_string_equal(display_title, "Function");
    assert_string_equal(display_text, "me");

// \"string parameter with spaces\",123,2.5,true,[11,\"22\",3.3],{\"one\":1,\"two\":2},{\"from\":\"AmMDEyc36FNXB3Fq1a61HeVJRT4yssMEP11NWWE9Qx8yhfRKexvq\",\"to\":\"AmP4AYWHKrxnPqvoUATyJhMwarzJAphWdkosz24AWgiD2sQ18si9\",\"hash\":\"0102030405060708090A0B0C0D0E0FFF\"}

    click_next();
    assert_string_equal(display_title, "Parameters");
    assert_string_equal(display_text, "\"string param");

    click_next();
    assert_string_equal(display_title, "Parameters");
    assert_string_equal(display_text, "eter with spa");

    click_next();
    assert_string_equal(display_title, "Parameters");
    assert_string_equal(display_text, "ces\",123,2.5,");

    click_next();
    assert_string_equal(display_title, "Parameters");
    assert_string_equal(display_text, "true,[11,\"22\"");

    click_next();
    assert_string_equal(display_title, "Parameters");
    assert_string_equal(display_text, ",3.3],{\"one\":");

    click_next();
    assert_string_equal(display_title, "Parameters");
    assert_string_equal(display_text, "1,\"two\":2},{\"");

    click_next();
    assert_string_equal(display_title, "Parameters");
    assert_string_equal(display_text, "from\":\"AmMDEy");

    click_next();
    assert_string_equal(display_title, "Parameters");
    assert_string_equal(display_text, "c36FNXB3Fq1a6");

    click_next();
    assert_string_equal(display_title, "Parameters");
    assert_string_equal(display_text, "1HeVJRT4yssME");

    click_next();
    assert_string_equal(display_title, "Parameters");
    assert_string_equal(display_text, "P11NWWE9Qx8yh");

    click_next();
    assert_string_equal(display_title, "Parameters");
    assert_string_equal(display_text, "fRKexvq\",\"to\"");

    click_next();
    assert_string_equal(display_title, "Parameters");
    assert_string_equal(display_text, ":\"AmP4AYWHKrx");

    click_next();
    assert_string_equal(display_title, "Parameters");
    assert_string_equal(display_text, "nPqvoUATyJhMw");

    click_next();
    assert_string_equal(display_title, "Parameters");
    assert_string_equal(display_text, "arzJAphWdkosz");

    click_next();
    assert_string_equal(display_title, "Parameters");
    assert_string_equal(display_text, "24AWgiD2sQ18s");

    click_next();
    assert_string_equal(display_title, "Parameters");
    assert_string_equal(display_text, "i9\",\"hash\":\"0");

    click_next();
    assert_string_equal(display_title, "Parameters");
    assert_string_equal(display_text, "1020304050607");

    click_next();
    assert_string_equal(display_title, "Parameters");
    assert_string_equal(display_text, "08090A0B0C0D0");

    click_next();
    assert_string_equal(display_title, "Parameters");
    assert_string_equal(display_text, "E0FFF\"}");

    click_next();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    // BACKWARDS

    click_prev();
    assert_string_equal(display_title, "Parameters");
    assert_string_equal(display_text, "E0FFF\"}");

    click_prev();
    assert_string_equal(display_title, "Parameters");
    assert_string_equal(display_text, "08090A0B0C0D0");

    click_prev();
    assert_string_equal(display_title, "Parameters");
    assert_string_equal(display_text, "1020304050607");

    click_prev();
    assert_string_equal(display_title, "Parameters");
    assert_string_equal(display_text, "i9\",\"hash\":\"0");

    click_prev();
    assert_string_equal(display_title, "Parameters");
    assert_string_equal(display_text, "24AWgiD2sQ18s");

    click_prev();
    assert_string_equal(display_title, "Parameters");
    assert_string_equal(display_text, "arzJAphWdkosz");

    click_prev();
    assert_string_equal(display_title, "Parameters");
    assert_string_equal(display_text, "nPqvoUATyJhMw");

    click_prev();
    assert_string_equal(display_title, "Parameters");
    assert_string_equal(display_text, ":\"AmP4AYWHKrx");

    click_prev();
    assert_string_equal(display_title, "Parameters");
    assert_string_equal(display_text, "fRKexvq\",\"to\"");

    click_prev();
    assert_string_equal(display_title, "Parameters");
    assert_string_equal(display_text, "P11NWWE9Qx8yh");

    click_prev();
    assert_string_equal(display_title, "Parameters");
    assert_string_equal(display_text, "1HeVJRT4yssME");

    click_prev();
    assert_string_equal(display_title, "Parameters");
    assert_string_equal(display_text, "c36FNXB3Fq1a6");

    click_prev();
    assert_string_equal(display_title, "Parameters");
    assert_string_equal(display_text, "from\":\"AmMDEy");

    click_prev();
    assert_string_equal(display_title, "Parameters");
    assert_string_equal(display_text, "1,\"two\":2},{\"");

    click_prev();
    assert_string_equal(display_title, "Parameters");
    assert_string_equal(display_text, ",3.3],{\"one\":");

    click_prev();
    assert_string_equal(display_title, "Parameters");
    assert_string_equal(display_text, "true,[11,\"22\"");

    click_prev();
    assert_string_equal(display_title, "Parameters");
    assert_string_equal(display_text, "ces\",123,2.5,");

    click_prev();
    assert_string_equal(display_title, "Parameters");
    assert_string_equal(display_text, "eter with spa");

    click_prev();
    assert_string_equal(display_title, "Parameters");
    assert_string_equal(display_text, "\"string param");

    click_prev();
    assert_string_equal(display_title, "Function");
    assert_string_equal(display_text, "me");

    click_prev();
    assert_string_equal(display_title, "Function");
    assert_string_equal(display_text, "t_function_na");

    click_prev();
    assert_string_equal(display_title, "Function");
    assert_string_equal(display_text, "smart_contrac");

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

    click_prev();
    assert_string_equal(display_title, "Amount");
    assert_string_equal(display_text, "678 AERGO");

    click_prev();
    assert_string_equal(display_title, "Amount");
    assert_string_equal(display_text, "3456789012345");

    click_prev();
    assert_string_equal(display_title, "Amount");
    assert_string_equal(display_text, "1234567890.12");

    click_prev();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    // again backwards 2 more times

    click_prev();
    assert_string_equal(display_title, "Parameters");
    assert_string_equal(display_text, "E0FFF\"}");

    click_prev();
    assert_string_equal(display_title, "Parameters");
    assert_string_equal(display_text, "08090A0B0C0D0");

    // then forward 4 times

    click_next();
    assert_string_equal(display_title, "Parameters");
    assert_string_equal(display_text, "E0FFF\"}");

    click_next();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    click_next();
    assert_string_equal(display_title, "Amount");
    assert_string_equal(display_text, "1234567890.12");

    click_next();
    assert_string_equal(display_title, "Amount");
    assert_string_equal(display_text, "3456789012345");
}


// FEE_DELEGATION CALL
static void test_tx_display_fee_delegation_call(void **state) {
    (void) state;

    // clang-format off
    uint8_t raw_tx[] = {
        // tx type
        0x03,
        // transaction
        0x08, 0xac, 0x02, 0x12, 0x21, 0x02, 0x9d, 0x02,
        0x05, 0x91, 0xe7, 0xfb, 0x7b, 0x09, 0x21, 0x53,
        0x68, 0x19, 0x95, 0xf8, 0x06, 0x09, 0xf0, 0xac,
        0x98, 0x8a, 0x4d, 0x93, 0x5e, 0x0e, 0xa6, 0x3c,
        0x06, 0x0f, 0x19, 0x54, 0xb0, 0x5f, 0x1a, 0x0c,
        0x62, 0x69, 0x67, 0x64, 0x65, 0x61, 0x6c, 0x70,
        0x6f, 0x69, 0x6e, 0x74, 0x22, 0x01, 0x00, 0x2a,
        0x81, 0x01, 0x7b, 0x22, 0x4e, 0x61, 0x6d, 0x65,
        0x22, 0x3a, 0x22, 0x73, 0x6d, 0x61, 0x72, 0x74,
        0x5f, 0x63, 0x6f, 0x6e, 0x74, 0x72, 0x61, 0x63,
        0x74, 0x5f, 0x66, 0x65, 0x65, 0x5f, 0x64, 0x65,
        0x6c, 0x65, 0x67, 0x61, 0x74, 0x69, 0x6f, 0x6e,
        0x5f, 0x66, 0x75, 0x6e, 0x63, 0x74, 0x69, 0x6f,
        0x6e, 0x22, 0x2c, 0x22, 0x41, 0x72, 0x67, 0x73,
        0x22, 0x3a, 0x5b, 0x22, 0x68, 0x65, 0x6c, 0x6c,
        0x6f, 0x20, 0x63, 0x6f, 0x6e, 0x74, 0x72, 0x61,
        0x63, 0x74, 0x21, 0x22, 0x2c, 0x31, 0x32, 0x33,
        0x2c, 0x32, 0x2e, 0x35, 0x2c, 0x74, 0x72, 0x75,
        0x65, 0x2c, 0x66, 0x61, 0x6c, 0x73, 0x65, 0x2c,
        0x5b, 0x31, 0x31, 0x2c, 0x22, 0x32, 0x32, 0x22,
        0x2c, 0x33, 0x2e, 0x33, 0x5d, 0x2c, 0x7b, 0x22,
        0x6f, 0x6e, 0x65, 0x22, 0x3a, 0x31, 0x31, 0x2c,
        0x22, 0x74, 0x77, 0x6f, 0x22, 0x3a, 0x32, 0x32,
        0x7d, 0x5d, 0x7d, 0x3a, 0x01, 0x00, 0x40, 0x03,
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
    assert_string_equal(display_title, "Contract");
    assert_string_equal(display_text, "bigdealpoint");

    click_next();
    assert_string_equal(display_title, "Function");
    assert_string_equal(display_text, "smart_contrac");

    click_next();
    assert_string_equal(display_title, "Function");
    assert_string_equal(display_text, "t_fee_delegat");

    click_next();
    assert_string_equal(display_title, "Function");
    assert_string_equal(display_text, "ion_function");

    click_next();
    assert_string_equal(display_title, "Parameters");
    assert_string_equal(display_text, "\"hello contra");

    click_next();
    assert_string_equal(display_title, "Parameters");
    assert_string_equal(display_text, "ct!\",123,2.5,");

    click_next();
    assert_string_equal(display_title, "Parameters");
    assert_string_equal(display_text, "true,false,[1");

    click_next();
    assert_string_equal(display_title, "Parameters");
    assert_string_equal(display_text, "1,\"22\",3.3],{");

    click_next();
    assert_string_equal(display_title, "Parameters");
    assert_string_equal(display_text, "\"one\":11,\"two");

    click_next();
    assert_string_equal(display_title, "Parameters");
    assert_string_equal(display_text, "\":22}");

    click_next();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    // BACKWARDS

    click_prev();
    assert_string_equal(display_title, "Parameters");
    assert_string_equal(display_text, "\":22}");

    click_prev();
    assert_string_equal(display_title, "Parameters");
    assert_string_equal(display_text, "\"one\":11,\"two");

    click_prev();
    assert_string_equal(display_title, "Parameters");
    assert_string_equal(display_text, "1,\"22\",3.3],{");

    click_prev();
    assert_string_equal(display_title, "Parameters");
    assert_string_equal(display_text, "true,false,[1");

    click_prev();
    assert_string_equal(display_title, "Parameters");
    assert_string_equal(display_text, "ct!\",123,2.5,");

    click_prev();
    assert_string_equal(display_title, "Parameters");
    assert_string_equal(display_text, "\"hello contra");

    click_prev();
    assert_string_equal(display_title, "Function");
    assert_string_equal(display_text, "ion_function");

    click_prev();
    assert_string_equal(display_title, "Function");
    assert_string_equal(display_text, "t_fee_delegat");

    click_prev();
    assert_string_equal(display_title, "Function");
    assert_string_equal(display_text, "smart_contrac");

    click_prev();
    assert_string_equal(display_title, "Contract");
    assert_string_equal(display_text, "bigdealpoint");

    click_prev();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    // again backwards 2 more times

    click_prev();
    assert_string_equal(display_title, "Parameters");
    assert_string_equal(display_text, "\":22}");

    click_prev();
    assert_string_equal(display_title, "Parameters");
    assert_string_equal(display_text, "\"one\":11,\"two");

    // then forward 4 times

    click_next();
    assert_string_equal(display_title, "Parameters");
    assert_string_equal(display_text, "\":22}");

    click_next();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    click_next();
    assert_string_equal(display_title, "Contract");
    assert_string_equal(display_text, "bigdealpoint");

    click_next();
    assert_string_equal(display_title, "Function");
    assert_string_equal(display_text, "smart_contrac");

}

// MULTICALL
static void test_tx_display_multicall_1(void **state) {
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

// MULTICALL
static void test_tx_display_multicall_2(void **state) {
    (void) state;

    // clang-format off
    uint8_t raw_tx[] = {
        // tx type
        0x07,
        // transaction
        0x08, 0xfa, 0x01, 0x12, 0x21, 0x02, 0x9d, 0x02,
        0x05, 0x91, 0xe7, 0xfb, 0x7b, 0x09, 0x21, 0x53,
        0x68, 0x19, 0x95, 0xf8, 0x06, 0x09, 0xf0, 0xac,
        0x98, 0x8a, 0x4d, 0x93, 0x5e, 0x0e, 0xa6, 0x3c,
        0x06, 0x0f, 0x19, 0x54, 0xb0, 0x5f, 0x22, 0x01,
        0x00, 0x2a, 0x93, 0x03, 0x5b, 0x5b, 0x22, 0x6c,
        0x65, 0x74, 0x22, 0x2c, 0x22, 0x74, 0x6f, 0x6b,
        0x65, 0x6e, 0x31, 0x22, 0x2c, 0x22, 0x41, 0x6d,
        0x68, 0x63, 0x63, 0x65, 0x6f, 0x70, 0x52, 0x69,
        0x55, 0x37, 0x72, 0x33, 0x47, 0x77, 0x79, 0x35,
        0x74, 0x6d, 0x74, 0x6b, 0x6b, 0x34, 0x5a, 0x33,
        0x50, 0x78, 0x35, 0x33, 0x53, 0x66, 0x73, 0x4b,
        0x42, 0x69, 0x66, 0x47, 0x4d, 0x76, 0x61, 0x53,
        0x53, 0x4e, 0x69, 0x79, 0x57, 0x72, 0x76, 0x4b,
        0x59, 0x65, 0x22, 0x5d, 0x2c, 0x5b, 0x22, 0x6c,
        0x65, 0x74, 0x22, 0x2c, 0x22, 0x74, 0x6f, 0x6b,
        0x65, 0x6e, 0x32, 0x22, 0x2c, 0x22, 0x41, 0x6d,
        0x50, 0x57, 0x77, 0x6d, 0x64, 0x67, 0x70, 0x76,
        0x50, 0x52, 0x50, 0x74, 0x79, 0x6b, 0x67, 0x43,
        0x43, 0x57, 0x76, 0x56, 0x64, 0x5a, 0x53, 0x36,
        0x68, 0x37, 0x62, 0x36, 0x77, 0x39, 0x55, 0x7a,
        0x63, 0x4c, 0x63, 0x73, 0x45, 0x64, 0x36, 0x34,
        0x6d, 0x7a, 0x4b, 0x4a, 0x39, 0x52, 0x43, 0x41,
        0x68, 0x70, 0x22, 0x5d, 0x2c, 0x5b, 0x22, 0x63,
        0x61, 0x6c, 0x6c, 0x22, 0x2c, 0x22, 0x25, 0x74,
        0x6f, 0x6b, 0x65, 0x6e, 0x32, 0x25, 0x22, 0x2c,
        0x22, 0x62, 0x61, 0x6c, 0x61, 0x6e, 0x63, 0x65,
        0x4f, 0x66, 0x22, 0x5d, 0x2c, 0x5b, 0x22, 0x73,
        0x74, 0x6f, 0x72, 0x65, 0x22, 0x2c, 0x22, 0x62,
        0x65, 0x66, 0x6f, 0x72, 0x65, 0x22, 0x5d, 0x2c,
        0x5b, 0x22, 0x63, 0x61, 0x6c, 0x6c, 0x22, 0x2c,
        0x22, 0x25, 0x74, 0x6f, 0x6b, 0x65, 0x6e, 0x31,
        0x25, 0x22, 0x2c, 0x22, 0x74, 0x72, 0x61, 0x6e,
        0x73, 0x66, 0x65, 0x72, 0x22, 0x2c, 0x22, 0x25,
        0x70, 0x61, 0x69, 0x72, 0x25, 0x22, 0x2c, 0x22,
        0x31, 0x30, 0x2e, 0x32, 0x35, 0x22, 0x2c, 0x22,
        0x73, 0x77, 0x61, 0x70, 0x22, 0x2c, 0x7b, 0x22,
        0x6d, 0x69, 0x6e, 0x5f, 0x6f, 0x75, 0x74, 0x70,
        0x75, 0x74, 0x22, 0x3a, 0x22, 0x31, 0x32, 0x2e,
        0x33, 0x34, 0x35, 0x22, 0x2c, 0x22, 0x75, 0x6e,
        0x77, 0x72, 0x61, 0x70, 0x5f, 0x61, 0x65, 0x72,
        0x67, 0x6f, 0x22, 0x3a, 0x74, 0x72, 0x75, 0x65,
        0x7d, 0x5d, 0x2c, 0x5b, 0x22, 0x63, 0x61, 0x6c,
        0x6c, 0x22, 0x2c, 0x22, 0x25, 0x74, 0x6f, 0x6b,
        0x65, 0x6e, 0x32, 0x25, 0x22, 0x2c, 0x22, 0x62,
        0x61, 0x6c, 0x61, 0x6e, 0x63, 0x65, 0x4f, 0x66,
        0x22, 0x5d, 0x2c, 0x5b, 0x22, 0x73, 0x75, 0x62,
        0x22, 0x2c, 0x22, 0x25, 0x6c, 0x61, 0x73, 0x74,
        0x5f, 0x72, 0x65, 0x73, 0x75, 0x6c, 0x74, 0x25,
        0x22, 0x2c, 0x22, 0x25, 0x62, 0x65, 0x66, 0x6f,
        0x72, 0x65, 0x25, 0x22, 0x5d, 0x2c, 0x5b, 0x22,
        0x61, 0x73, 0x73, 0x65, 0x72, 0x74, 0x22, 0x2c,
        0x22, 0x25, 0x6c, 0x61, 0x73, 0x74, 0x5f, 0x72,
        0x65, 0x73, 0x75, 0x6c, 0x74, 0x25, 0x22, 0x2c,
        0x22, 0x3e, 0x3d, 0x22, 0x2c, 0x22, 0x31, 0x30,
        0x30, 0x2e, 0x37, 0x35, 0x22, 0x5d, 0x5d, 0x3a,
        0x01, 0x00, 0x40, 0x07, 0x4a, 0x20, 0x52, 0x48,
        0x45, 0xc2, 0x4c, 0xd3, 0xe5, 0x3a, 0xec, 0xbc,
        0xda, 0x8e, 0x31, 0x5d, 0x62, 0xdc, 0x95, 0xa7,
        0xf2, 0xf8, 0x25, 0x48, 0x93, 0x0b, 0xc2, 0xfc,
        0xc9, 0x86, 0xbf, 0x74, 0x53, 0xbd,
    };

    int ret = setjmp(jump_buffer);
    assert_int_equal(ret, 0);

    send_transaction(raw_tx, sizeof(raw_tx));

    //generate_test_case();

    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "=> let");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "token1");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "AmhcceopRiU7r");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "3Gwy5tmtkk4Z3");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "Px53SfsKBifGM");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "vaSSNiyWrvKYe");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "=> let");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "token2");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "AmPWwmdgpvPRP");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "tykgCCWvVdZS6");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "h7b6w9UzcLcsE");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "d64mzKJ9RCAhp");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "=> call");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "%token2%");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "balanceOf");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "=> store");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "before");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "=> call");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "%token1%");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "transfer");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "%pair%");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "10.25");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "swap");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "{\"min_output\"");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, ":\"12.345\",\"un");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "wrap_aergo\":t");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "rue}");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "=> call");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "%token2%");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "balanceOf");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "=> sub");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "%last_result%");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "%before%");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "=> assert");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "%last_result%");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, ">=");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "100.75");

    click_next();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    // BACKWARDS
    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "100.75");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, ">=");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "%last_result%");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "=> assert");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "%before%");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "%last_result%");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "=> sub");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "balanceOf");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "%token2%");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "=> call");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "rue}");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "wrap_aergo\":t");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, ":\"12.345\",\"un");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "{\"min_output\"");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "swap");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "10.25");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "%pair%");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "transfer");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "%token1%");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "=> call");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "before");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "=> store");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "balanceOf");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "%token2%");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "=> call");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "d64mzKJ9RCAhp");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "h7b6w9UzcLcsE");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "tykgCCWvVdZS6");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "AmPWwmdgpvPRP");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "token2");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "=> let");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "vaSSNiyWrvKYe");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "Px53SfsKBifGM");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "3Gwy5tmtkk4Z3");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "AmhcceopRiU7r");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "token1");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "=> let");

    click_prev();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    // again backwards 2 more times

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "100.75");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, ">=");

    // then forward 4 times

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "100.75");

    click_next();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "=> let");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "token1");
}

// MULTICALL
static void test_tx_display_multicall_3(void **state) {
    (void) state;

    // clang-format off
    uint8_t raw_tx[] = {
        // tx type
        0x07,
        // transaction
        0x08, 0xfa, 0x01, 0x12, 0x21, 0x02, 0x9d, 0x02,
        0x05, 0x91, 0xe7, 0xfb, 0x7b, 0x09, 0x21, 0x53,
        0x68, 0x19, 0x95, 0xf8, 0x06, 0x09, 0xf0, 0xac,
        0x98, 0x8a, 0x4d, 0x93, 0x5e, 0x0e, 0xa6, 0x3c,
        0x06, 0x0f, 0x19, 0x54, 0xb0, 0x5f, 0x22, 0x01,
        0x00, 0x2a, 0xa5, 0x02, 0x5b, 0x5b, 0x22, 0x6c,
        0x65, 0x74, 0x22, 0x2c, 0x22, 0x76, 0x31, 0x22,
        0x2c, 0x32, 0x35, 0x30, 0x5d, 0x2c, 0x5b, 0x22,
        0x6c, 0x65, 0x74, 0x22, 0x2c, 0x22, 0x76, 0x32,
        0x22, 0x2c, 0x2d, 0x31, 0x32, 0x2e, 0x33, 0x34,
        0x35, 0x5d, 0x2c, 0x5b, 0x22, 0x6c, 0x65, 0x74,
        0x22, 0x2c, 0x22, 0x76, 0x33, 0x22, 0x2c, 0x74,
        0x72, 0x75, 0x65, 0x5d, 0x2c, 0x5b, 0x22, 0x6c,
        0x65, 0x74, 0x22, 0x2c, 0x22, 0x76, 0x34, 0x22,
        0x2c, 0x66, 0x61, 0x6c, 0x73, 0x65, 0x5d, 0x2c,
        0x5b, 0x22, 0x6c, 0x65, 0x74, 0x22, 0x2c, 0x22,
        0x76, 0x35, 0x22, 0x2c, 0x6e, 0x75, 0x6c, 0x6c,
        0x5d, 0x2c, 0x5b, 0x22, 0x6c, 0x65, 0x74, 0x22,
        0x2c, 0x22, 0x6c, 0x69, 0x73, 0x74, 0x22, 0x2c,
        0x5b, 0x32, 0x35, 0x30, 0x2c, 0x31, 0x32, 0x2e,
        0x33, 0x34, 0x35, 0x2c, 0x74, 0x72, 0x75, 0x65,
        0x5d, 0x5d, 0x2c, 0x5b, 0x22, 0x6c, 0x65, 0x74,
        0x22, 0x2c, 0x22, 0x6f, 0x62, 0x6a, 0x22, 0x2c,
        0x7b, 0x22, 0x69, 0x6e, 0x74, 0x22, 0x3a, 0x32,
        0x35, 0x30, 0x2c, 0x22, 0x66, 0x6c, 0x6f, 0x61,
        0x74, 0x22, 0x3a, 0x2d, 0x31, 0x32, 0x33, 0x34,
        0x35, 0x30, 0x30, 0x30, 0x2c, 0x22, 0x62, 0x6f,
        0x6f, 0x6c, 0x22, 0x3a, 0x74, 0x72, 0x75, 0x65,
        0x7d, 0x5d, 0x2c, 0x5b, 0x22, 0x63, 0x61, 0x6c,
        0x6c, 0x22, 0x2c, 0x22, 0x25, 0x63, 0x25, 0x22,
        0x2c, 0x22, 0x74, 0x65, 0x73, 0x74, 0x22, 0x2c,
        0x32, 0x35, 0x30, 0x2c, 0x31, 0x32, 0x2e, 0x33,
        0x34, 0x35, 0x2c, 0x2d, 0x32, 0x35, 0x30, 0x65,
        0x31, 0x30, 0x2c, 0x31, 0x32, 0x2e, 0x33, 0x34,
        0x35, 0x45, 0x2b, 0x35, 0x2c, 0x74, 0x72, 0x75,
        0x65, 0x2c, 0x66, 0x61, 0x6c, 0x73, 0x65, 0x2c,
        0x6e, 0x75, 0x6c, 0x6c, 0x5d, 0x2c, 0x5b, 0x22,
        0x61, 0x73, 0x73, 0x65, 0x72, 0x74, 0x22, 0x2c,
        0x2d, 0x31, 0x30, 0x30, 0x2e, 0x37, 0x35, 0x65,
        0x2b, 0x39, 0x2c, 0x22, 0x3e, 0x3d, 0x22, 0x2c,
        0x22, 0x25, 0x6c, 0x61, 0x73, 0x74, 0x5f, 0x72,
        0x65, 0x73, 0x75, 0x6c, 0x74, 0x25, 0x22, 0x5d,
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
    assert_string_equal(display_text, "v1");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "250");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "=> let");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "v2");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "-12.345");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "=> let");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "v3");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "true");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "=> let");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "v4");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "false");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "=> let");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "v5");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "null");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "=> let");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "list");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "[250,12.345,t");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "rue]");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "=> let");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "obj");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "{\"int\":250,\"f");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "loat\":-123450");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "00,\"bool\":tru");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "e}");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "=> call");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "%c%");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "test");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "250");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "12.345");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "-250e10");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "12.345E+5");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "true");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "false");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "null");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "=> assert");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "-100.75e+9");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, ">=");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "%last_result%");

    click_next();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    // BACKWARDS
    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "%last_result%");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, ">=");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "-100.75e+9");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "=> assert");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "null");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "false");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "true");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "12.345E+5");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "-250e10");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "12.345");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "250");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "test");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "%c%");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "=> call");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "e}");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "00,\"bool\":tru");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "loat\":-123450");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "{\"int\":250,\"f");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "obj");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "=> let");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "rue]");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "[250,12.345,t");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "list");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "=> let");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "null");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "v5");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "=> let");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "false");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "v4");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "=> let");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "true");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "v3");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "=> let");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "-12.345");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "v2");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "=> let");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "250");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "v1");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "=> let");

    click_prev();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    // again backwards 2 more times

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "%last_result%");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, ">=");

    // then forward 4 times

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "%last_result%");

    click_next();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "=> let");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "v1");
}

// MULTICALL
static void test_tx_display_multicall_4(void **state) {
    (void) state;

    // clang-format off
    uint8_t raw_tx[] = {
        // tx type
        0x07,
        // transaction
        0x08, 0xbc, 0x05, 0x12, 0x21, 0x02, 0x9d, 0x02,
        0x05, 0x91, 0xe7, 0xfb, 0x7b, 0x09, 0x21, 0x53,
        0x68, 0x19, 0x95, 0xf8, 0x06, 0x09, 0xf0, 0xac,
        0x98, 0x8a, 0x4d, 0x93, 0x5e, 0x0e, 0xa6, 0x3c,
        0x06, 0x0f, 0x19, 0x54, 0xb0, 0x5f, 0x22, 0x01,
        0x00, 0x2a, 0xae, 0x02, 0x5b, 0x5b, 0x22, 0x6c,
        0x65, 0x74, 0x22, 0x2c, 0x22, 0x70, 0x74, 0x22,
        0x2c, 0x22, 0x41, 0xc3, 0xa7, 0xc3, 0xa3, 0x6f,
        0x20, 0xc3, 0xa0, 0x20, 0x70, 0xc3, 0xa9, 0x22,
        0x5d, 0x2c, 0x5b, 0x22, 0x6c, 0x65, 0x74, 0x22,
        0x2c, 0x22, 0x6b, 0x72, 0x22, 0x2c, 0x22, 0xed,
        0x85, 0x8c, 0xec, 0x8a, 0xa4, 0xed, 0x8a, 0xb8,
        0xeb, 0x84, 0xb7, 0x22, 0x5d, 0x2c, 0x5b, 0x22,
        0x6c, 0x65, 0x74, 0x22, 0x2c, 0x22, 0x73, 0x70,
        0x65, 0x63, 0x69, 0x61, 0x6c, 0x5f, 0x63, 0x68,
        0x61, 0x72, 0x73, 0x22, 0x2c, 0x22, 0x61, 0x5c,
        0x6e, 0x62, 0x5c, 0x72, 0x63, 0x5c, 0x74, 0x64,
        0x5c, 0x75, 0x30, 0x30, 0x30, 0x35, 0x22, 0x5d,
        0x2c, 0x5b, 0x22, 0x6c, 0x65, 0x74, 0x22, 0x2c,
        0x22, 0x6c, 0x69, 0x73, 0x74, 0x31, 0x22, 0x2c,
        0x5b, 0x32, 0x35, 0x30, 0x2c, 0x5b, 0x31, 0x2c,
        0x32, 0x5d, 0x2c, 0x32, 0x35, 0x30, 0x5d, 0x5d,
        0x2c, 0x5b, 0x22, 0x6c, 0x65, 0x74, 0x22, 0x2c,
        0x22, 0x6c, 0x69, 0x73, 0x74, 0x32, 0x22, 0x2c,
        0x5b, 0x32, 0x35, 0x30, 0x2c, 0x7b, 0x22, 0x6f,
        0x6e, 0x65, 0x22, 0x3a, 0x31, 0x2c, 0x22, 0x74,
        0x77, 0x6f, 0x22, 0x3a, 0x32, 0x7d, 0x2c, 0x32,
        0x35, 0x30, 0x5d, 0x5d, 0x2c, 0x5b, 0x22, 0x6c,
        0x65, 0x74, 0x22, 0x2c, 0x22, 0x6f, 0x62, 0x6a,
        0x31, 0x22, 0x2c, 0x7b, 0x22, 0x6f, 0x62, 0x6a,
        0x22, 0x3a, 0x7b, 0x22, 0x6f, 0x6e, 0x65, 0x22,
        0x3a, 0x5b, 0x31, 0x5d, 0x2c, 0x22, 0x74, 0x77,
        0x6f, 0x22, 0x3a, 0x5b, 0x32, 0x5d, 0x7d, 0x2c,
        0x22, 0x6c, 0x69, 0x73, 0x74, 0x22, 0x3a, 0x5b,
        0x7b, 0x22, 0x6f, 0x6e, 0x65, 0x22, 0x3a, 0x31,
        0x7d, 0x2c, 0x7b, 0x22, 0x74, 0x77, 0x6f, 0x22,
        0x3a, 0x32, 0x7d, 0x5d, 0x2c, 0x22, 0x62, 0x6f,
        0x6f, 0x6c, 0x22, 0x3a, 0x74, 0x72, 0x75, 0x65,
        0x7d, 0x2c, 0x22, 0x68, 0x65, 0x6c, 0x6c, 0x6f,
        0x22, 0x5d, 0x2c, 0x5b, 0x22, 0x61, 0x73, 0x73,
        0x65, 0x72, 0x74, 0x22, 0x2c, 0x22, 0x74, 0x68,
        0x69, 0x73, 0x22, 0x2c, 0x22, 0x69, 0x73, 0x22,
        0x2c, 0x22, 0x73, 0x68, 0x6f, 0x77, 0x6e, 0x22,
        0x5d, 0x5d, 0x30, 0x20, 0x3a, 0x01, 0x01, 0x40,
        0x07, 0x4a, 0x20, 0x52, 0x48, 0x45, 0xc2, 0x4c,
        0xd3, 0xe5, 0x3a, 0xec, 0xbc, 0xda, 0x8e, 0x31,
        0x5d, 0x62, 0xdc, 0x95, 0xa7, 0xf2, 0xf8, 0x25,
        0x48, 0x93, 0x0b, 0xc2, 0xfc, 0xc9, 0x86, 0xbf,
        0x74, 0x53, 0xbd,
    };

    int ret = setjmp(jump_buffer);
    assert_int_equal(ret, 0);

    send_transaction(raw_tx, sizeof(raw_tx));

    //generate_test_case();

    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "=> let");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "pt");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "A\\uE7\\uE3o \\u");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "E0 p\\uE9");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "=> let");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "kr");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "\\uD14C\\uC2A4\\");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "uD2B8\\uB137");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "=> let");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "special_chars");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "a\\nb\\rc\\td\\u0");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "005");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "=> let");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "list1");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "[250,[1,2],25");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "0]");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "=> let");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "list2");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "[250,{\"one\":1");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, ",\"two\":2},250");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "]");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "=> let");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "obj1");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "{\"obj\":{\"one\"");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, ":[1],\"two\":[2");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "]},\"list\":[{\"");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "one\":1},{\"two");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "\":2}],\"bool\":");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "true}");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "hello");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "=> assert");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "this");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "is");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "shown");

    click_next();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    // BACKWARDS

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "shown");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "is");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "this");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "=> assert");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "hello");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "true}");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "\":2}],\"bool\":");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "one\":1},{\"two");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "]},\"list\":[{\"");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, ":[1],\"two\":[2");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "{\"obj\":{\"one\"");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "obj1");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "=> let");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "]");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, ",\"two\":2},250");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "[250,{\"one\":1");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "list2");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "=> let");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "0]");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "[250,[1,2],25");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "list1");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "=> let");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "005");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "a\\nb\\rc\\td\\u0");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "special_chars");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "=> let");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "uD2B8\\uB137");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "\\uD14C\\uC2A4\\");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "kr");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "=> let");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "E0 p\\uE9");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "A\\uE7\\uE3o \\u");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "pt");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "=> let");

    click_prev();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    // again backwards 2 more times

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "shown");

    click_prev();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "is");

    // then forward 4 times

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "shown");

    click_next();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "=> let");

    click_next();
    assert_string_equal(display_title, "MultiCall");
    assert_string_equal(display_text, "pt");

}

// DEPLOY
static void test_tx_display_deploy_1(void **state) {
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

// test also: deploy with parameters to constructor

// DEPLOY WITH AMOUNT
static void test_tx_display_deploy_2(void **state) {
    (void) state;

    // clang-format off
    uint8_t raw_tx[] = {
        // tx type
        0x06,
        // transaction
        0x08, 0x88, 0x27, 0x12, 0x21, 0x02, 0x9d, 0x02,
        0x05, 0x91, 0xe7, 0xfb, 0x7b, 0x09, 0x21, 0x53,
        0x68, 0x19, 0x95, 0xf8, 0x06, 0x09, 0xf0, 0xac,
        0x98, 0x8a, 0x4d, 0x93, 0x5e, 0x0e, 0xa6, 0x3c,
        0x06, 0x0f, 0x19, 0x54, 0xb0, 0x5f, 0x22, 0x0e,
        0x3c, 0xde, 0x6f, 0xff, 0x97, 0x32, 0xd5, 0x45,
        0xd5, 0x8f, 0xd9, 0x30, 0xf3, 0x4e, 0x2a, 0x81,
        0x08, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
        0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
        0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
        0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e,
        0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26,
        0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e,
        0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36,
        0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e,
        0x3f, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46,
        0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e,
        0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56,
        0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e,
        0x5f, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
        0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e,
        0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76,
        0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e,
        0x7f, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86,
        0x87, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e,
        0x8f, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96,
        0x97, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e,
        0x9f, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6,
        0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae,
        0xaf, 0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
        0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe,
        0xbf, 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6,
        0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce,
        0xcf, 0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6,
        0xd7, 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde,
        0xdf, 0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6,
        0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee,
        0xef, 0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6,
        0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe,
        0xff, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
        0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
        0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
        0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e,
        0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26,
        0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e,
        0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36,
        0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e,
        0x3f, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46,
        0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e,
        0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56,
        0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e,
        0x5f, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
        0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e,
        0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76,
        0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e,
        0x7f, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86,
        0x87, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e,
        0x8f, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96,
        0x97, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e,
        0x9f, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6,
        0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae,
        0xaf, 0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
        0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe,
        0xbf, 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6,
        0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce,
        0xcf, 0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6,
        0xd7, 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde,
        0xdf, 0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6,
        0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee,
        0xef, 0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6,
        0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe,
        0xff, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
        0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
        0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
        0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e,
        0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26,
        0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e,
        0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36,
        0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e,
        0x3f, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46,
        0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e,
        0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56,
        0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e,
        0x5f, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
        0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e,
        0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76,
        0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e,
        0x7f, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86,
        0x87, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e,
        0x8f, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96,
        0x97, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e,
        0x9f, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6,
        0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae,
        0xaf, 0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
        0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe,
        0xbf, 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6,
        0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce,
        0xcf, 0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6,
        0xd7, 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde,
        0xdf, 0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6,
        0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee,
        0xef, 0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6,
        0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe,
        0xff, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
        0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
        0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
        0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e,
        0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26,
        0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e,
        0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36,
        0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e,
        0x3f, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46,
        0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e,
        0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56,
        0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e,
        0x5f, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
        0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e,
        0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76,
        0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e,
        0x7f, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86,
        0x87, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e,
        0x8f, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96,
        0x97, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e,
        0x9f, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6,
        0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae,
        0xaf, 0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
        0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe,
        0xbf, 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6,
        0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce,
        0xcf, 0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6,
        0xd7, 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde,
        0xdf, 0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6,
        0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee,
        0xef, 0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6,
        0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe,
        0xff, 0x00, 0x3a, 0x01, 0x00, 0x40, 0x06, 0x4a,
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
    assert_string_equal(display_text, "1234567890123");

    click_next();
    assert_string_equal(display_title, "Amount");
    assert_string_equal(display_text, "456.123456789");

    click_next();
    assert_string_equal(display_title, "Amount");
    assert_string_equal(display_text, "012345678 AER");

    click_next();
    assert_string_equal(display_title, "Amount");
    assert_string_equal(display_text, "GO");

    click_next();
    assert_string_equal(display_title, "New Contract 1/6");
    assert_string_equal(display_text, "B3981D93EEB6");

    click_next();
    assert_string_equal(display_title, "New Contract 2/6");
    assert_string_equal(display_text, "4AA900F3E48C");

    click_next();
    assert_string_equal(display_title, "New Contract 3/6");
    assert_string_equal(display_text, "FCD48E9BBC89");

    click_next();
    assert_string_equal(display_title, "New Contract 4/6");
    assert_string_equal(display_text, "B77732C49EA2");

    click_next();
    assert_string_equal(display_title, "New Contract 5/6");
    assert_string_equal(display_text, "01C93656C62B");

    click_next();
    assert_string_equal(display_title, "New Contract 6/6");
    assert_string_equal(display_text, "6A09");

    click_next();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    // BACKWARDS

    click_prev();
    assert_string_equal(display_title, "New Contract 6/6");
    assert_string_equal(display_text, "6A09");

    click_prev();
    assert_string_equal(display_title, "New Contract 5/6");
    assert_string_equal(display_text, "01C93656C62B");

    click_prev();
    assert_string_equal(display_title, "New Contract 4/6");
    assert_string_equal(display_text, "B77732C49EA2");

    click_prev();
    assert_string_equal(display_title, "New Contract 3/6");
    assert_string_equal(display_text, "FCD48E9BBC89");

    click_prev();
    assert_string_equal(display_title, "New Contract 2/6");
    assert_string_equal(display_text, "4AA900F3E48C");

    click_prev();
    assert_string_equal(display_title, "New Contract 1/6");
    assert_string_equal(display_text, "B3981D93EEB6");

    click_prev();
    assert_string_equal(display_title, "Amount");
    assert_string_equal(display_text, "GO");

    click_prev();
    assert_string_equal(display_title, "Amount");
    assert_string_equal(display_text, "012345678 AER");

    click_prev();
    assert_string_equal(display_title, "Amount");
    assert_string_equal(display_text, "456.123456789");

    click_prev();
    assert_string_equal(display_title, "Amount");
    assert_string_equal(display_text, "1234567890123");

    click_prev();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    // again backwards 2 more times

    click_prev();
    assert_string_equal(display_title, "New Contract 6/6");
    assert_string_equal(display_text, "6A09");

    click_prev();
    assert_string_equal(display_title, "New Contract 5/6");
    assert_string_equal(display_text, "01C93656C62B");

    // then forward 4 times

    click_next();
    assert_string_equal(display_title, "New Contract 6/6");
    assert_string_equal(display_text, "6A09");

    click_next();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    click_next();
    assert_string_equal(display_title, "Amount");
    assert_string_equal(display_text, "1234567890123");

    click_next();
    assert_string_equal(display_title, "Amount");
    assert_string_equal(display_text, "456.123456789");

}

// REDEPLOY
static void test_tx_display_redeploy(void **state) {
    (void) state;

    // clang-format off
    uint8_t raw_tx[] = {
        // tx type
        0x02,
        // transaction
        0x08, 0x03, 0x12, 0x21, 0x02, 0x9d, 0x02, 0x05,
        0x91, 0xe7, 0xfb, 0x7b, 0x09, 0x21, 0x53, 0x68,
        0x19, 0x95, 0xf8, 0x06, 0x09, 0xf0, 0xac, 0x98,
        0x8a, 0x4d, 0x93, 0x5e, 0x0e, 0xa6, 0x3c, 0x06,
        0x0f, 0x19, 0x54, 0xb0, 0x5f, 0x1a, 0x21, 0x02,
        0x5d, 0x22, 0x30, 0xba, 0x75, 0x21, 0x7e, 0x60,
        0x37, 0x99, 0xe9, 0xa3, 0xd5, 0xb9, 0x1a, 0x63,
        0x61, 0x48, 0x3f, 0x9d, 0xa7, 0x37, 0x96, 0x41,
        0x0f, 0x6b, 0xc1, 0xce, 0x58, 0x01, 0xfd, 0xf2,
        0x22, 0x01, 0x00, 0x2a, 0x81, 0x04, 0x00, 0x01,
        0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
        0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11,
        0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19,
        0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21,
        0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29,
        0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31,
        0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
        0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41,
        0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
        0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51,
        0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
        0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, 0x60, 0x61,
        0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
        0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71,
        0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
        0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f, 0x80, 0x81,
        0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
        0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f, 0x90, 0x91,
        0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99,
        0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, 0xa0, 0xa1,
        0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9,
        0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb0, 0xb1,
        0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9,
        0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf, 0xc0, 0xc1,
        0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9,
        0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf, 0xd0, 0xd1,
        0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9,
        0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf, 0xe0, 0xe1,
        0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9,
        0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, 0xf0, 0xf1,
        0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9,
        0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff, 0x00, 0x01,
        0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
        0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11,
        0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19,
        0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21,
        0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29,
        0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31,
        0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
        0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41,
        0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
        0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51,
        0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
        0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, 0x60, 0x61,
        0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
        0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71,
        0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
        0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f, 0x80, 0x81,
        0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
        0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f, 0x90, 0x91,
        0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99,
        0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, 0xa0, 0xa1,
        0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9,
        0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb0, 0xb1,
        0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9,
        0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf, 0xc0, 0xc1,
        0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9,
        0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf, 0xd0, 0xd1,
        0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9,
        0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf, 0xe0, 0xe1,
        0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9,
        0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, 0xf0, 0xf1,
        0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9,
        0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff, 0x00, 0x3a,
        0x01, 0x00, 0x40, 0x02, 0x4a, 0x20, 0x52, 0x48,
        0x45, 0xc2, 0x4c, 0xd3, 0xe5, 0x3a, 0xec, 0xbc,
        0xda, 0x8e, 0x31, 0x5d, 0x62, 0xdc, 0x95, 0xa7,
        0xf2, 0xf8, 0x25, 0x48, 0x93, 0x0b, 0xc2, 0xfc,
        0xc9, 0x86, 0xbf, 0x74, 0x53, 0xbd,
    };

    int ret = setjmp(jump_buffer);
    assert_int_equal(ret, 0);

    send_transaction(raw_tx, sizeof(raw_tx));

    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    click_next();
    assert_string_equal(display_title, "Redeploy");
    assert_string_equal(display_text, "AmMDEyc36FNXB");

    click_next();
    assert_string_equal(display_title, "Redeploy");
    assert_string_equal(display_text, "3Fq1a61HeVJRT");

    click_next();
    assert_string_equal(display_title, "Redeploy");
    assert_string_equal(display_text, "4yssMEP11NWWE");

    click_next();
    assert_string_equal(display_title, "Redeploy");
    assert_string_equal(display_text, "9Qx8yhfRKexvq");

    click_next();
    assert_string_equal(display_title, "New Contract 1/6");
    assert_string_equal(display_text, "2AC6D9E2B50B");

    click_next();
    assert_string_equal(display_title, "New Contract 2/6");
    assert_string_equal(display_text, "DD65511FB4DC");

    click_next();
    assert_string_equal(display_title, "New Contract 3/6");
    assert_string_equal(display_text, "0F24B7F6A0C1");

    click_next();
    assert_string_equal(display_title, "New Contract 4/6");
    assert_string_equal(display_text, "B2E92D1CF690");

    click_next();
    assert_string_equal(display_title, "New Contract 5/6");
    assert_string_equal(display_text, "1DF565802E38");

    click_next();
    assert_string_equal(display_title, "New Contract 6/6");
    assert_string_equal(display_text, "0234");

    click_next();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    // BACKWARDS

    click_prev();
    assert_string_equal(display_title, "New Contract 6/6");
    assert_string_equal(display_text, "0234");

    click_prev();
    assert_string_equal(display_title, "New Contract 5/6");
    assert_string_equal(display_text, "1DF565802E38");

    click_prev();
    assert_string_equal(display_title, "New Contract 4/6");
    assert_string_equal(display_text, "B2E92D1CF690");

    click_prev();
    assert_string_equal(display_title, "New Contract 3/6");
    assert_string_equal(display_text, "0F24B7F6A0C1");

    click_prev();
    assert_string_equal(display_title, "New Contract 2/6");
    assert_string_equal(display_text, "DD65511FB4DC");

    click_prev();
    assert_string_equal(display_title, "New Contract 1/6");
    assert_string_equal(display_text, "2AC6D9E2B50B");

    click_prev();
    assert_string_equal(display_title, "Redeploy");
    assert_string_equal(display_text, "9Qx8yhfRKexvq");

    click_prev();
    assert_string_equal(display_title, "Redeploy");
    assert_string_equal(display_text, "4yssMEP11NWWE");

    click_prev();
    assert_string_equal(display_title, "Redeploy");
    assert_string_equal(display_text, "3Fq1a61HeVJRT");

    click_prev();
    assert_string_equal(display_title, "Redeploy");
    assert_string_equal(display_text, "AmMDEyc36FNXB");

    click_prev();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    // again backwards 2 more times

    click_prev();
    assert_string_equal(display_title, "New Contract 6/6");
    assert_string_equal(display_text, "0234");

    click_prev();
    assert_string_equal(display_title, "New Contract 5/6");
    assert_string_equal(display_text, "1DF565802E38");

    // then forward 4 times

    click_next();
    assert_string_equal(display_title, "New Contract 6/6");
    assert_string_equal(display_text, "0234");

    click_next();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    click_next();
    assert_string_equal(display_title, "Redeploy");
    assert_string_equal(display_text, "AmMDEyc36FNXB");

    click_next();
    assert_string_equal(display_title, "Redeploy");
    assert_string_equal(display_text, "3Fq1a61HeVJRT");

}

// GOVERNANCE
static void test_tx_display_governance_add_admin(void **state) {
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

static void test_tx_display_governance_remove_admin(void **state) {
    (void) state;

    // clang-format off
    uint8_t raw_tx[] = {
        // tx type
        0x01,
        // transaction
        0x08, 0x80, 0x10, 0x12, 0x21, 0x02, 0x9d, 0x02,
        0x05, 0x91, 0xe7, 0xfb, 0x7b, 0x09, 0x21, 0x53,
        0x68, 0x19, 0x95, 0xf8, 0x06, 0x09, 0xf0, 0xac,
        0x98, 0x8a, 0x4d, 0x93, 0x5e, 0x0e, 0xa6, 0x3c,
        0x06, 0x0f, 0x19, 0x54, 0xb0, 0x5f, 0x1a, 0x10,
        0x61, 0x65, 0x72, 0x67, 0x6f, 0x2e, 0x65, 0x6e,
        0x74, 0x65, 0x72, 0x70, 0x72, 0x69, 0x73, 0x65,
        0x22, 0x01, 0x00, 0x2a, 0x56, 0x7b, 0x22, 0x4e,
        0x61, 0x6d, 0x65, 0x22, 0x3a, 0x22, 0x72, 0x65,
        0x6d, 0x6f, 0x76, 0x65, 0x41, 0x64, 0x6d, 0x69,
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
    assert_string_equal(display_title, "Remove Admin");
    assert_string_equal(display_text, "\"AmMDEyc36FNX");

    click_next();
    assert_string_equal(display_title, "Remove Admin");
    assert_string_equal(display_text, "B3Fq1a61HeVJR");

    click_next();
    assert_string_equal(display_title, "Remove Admin");
    assert_string_equal(display_text, "T4yssMEP11NWW");

    click_next();
    assert_string_equal(display_title, "Remove Admin");
    assert_string_equal(display_text, "E9Qx8yhfRKexv");

    click_next();
    assert_string_equal(display_title, "Remove Admin");
    assert_string_equal(display_text, "q\"");

    click_next();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    // BACKWARDS

    click_prev();
    assert_string_equal(display_title, "Remove Admin");
    assert_string_equal(display_text, "q\"");

    click_prev();
    assert_string_equal(display_title, "Remove Admin");
    assert_string_equal(display_text, "E9Qx8yhfRKexv");

    click_prev();
    assert_string_equal(display_title, "Remove Admin");
    assert_string_equal(display_text, "T4yssMEP11NWW");

    click_prev();
    assert_string_equal(display_title, "Remove Admin");
    assert_string_equal(display_text, "B3Fq1a61HeVJR");

    click_prev();
    assert_string_equal(display_title, "Remove Admin");
    assert_string_equal(display_text, "\"AmMDEyc36FNX");

    click_prev();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    // again backwards 2 more times

    click_prev();
    assert_string_equal(display_title, "Remove Admin");
    assert_string_equal(display_text, "q\"");

    click_prev();
    assert_string_equal(display_title, "Remove Admin");
    assert_string_equal(display_text, "E9Qx8yhfRKexv");

    // then forward 4 times

    click_next();
    assert_string_equal(display_title, "Remove Admin");
    assert_string_equal(display_text, "q\"");

    click_next();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    click_next();
    assert_string_equal(display_title, "Remove Admin");
    assert_string_equal(display_text, "\"AmMDEyc36FNX");

    click_next();
    assert_string_equal(display_title, "Remove Admin");
    assert_string_equal(display_text, "B3Fq1a61HeVJR");

}

static void test_tx_display_governance_stake (void **state) {
    (void) state;

    // clang-format off
    uint8_t raw_tx[] = {
        // tx type
        0x01,
        // transaction
        0x08, 0x82, 0x20, 0x12, 0x21, 0x02, 0x9d, 0x02,
        0x05, 0x91, 0xe7, 0xfb, 0x7b, 0x09, 0x21, 0x53,
        0x68, 0x19, 0x95, 0xf8, 0x06, 0x09, 0xf0, 0xac,
        0x98, 0x8a, 0x4d, 0x93, 0x5e, 0x0e, 0xa6, 0x3c,
        0x06, 0x0f, 0x19, 0x54, 0xb0, 0x5f, 0x1a, 0x0c,
        0x61, 0x65, 0x72, 0x67, 0x6f, 0x2e, 0x73, 0x79,
        0x73, 0x74, 0x65, 0x6d, 0x22, 0x09, 0x06, 0xb1,
        0x4b, 0xd1, 0xe6, 0xee, 0xa0, 0x00, 0x00, 0x2a,
        0x12, 0x7b, 0x22, 0x4e, 0x61, 0x6d, 0x65, 0x22,
        0x3a, 0x22, 0x76, 0x31, 0x73, 0x74, 0x61, 0x6b,
        0x65, 0x22, 0x7d, 0x3a, 0x01, 0x00, 0x40, 0x01,
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
    assert_string_equal(display_title, "Stake");
    assert_string_equal(display_text, "123.456 AERGO");

    click_next();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    // BACKWARDS

    click_prev();
    assert_string_equal(display_title, "Stake");
    assert_string_equal(display_text, "123.456 AERGO");

    click_prev();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    // again backwards

    click_prev();
    assert_string_equal(display_title, "Stake");
    assert_string_equal(display_text, "123.456 AERGO");

    click_prev();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    // then forward

    click_next();
    assert_string_equal(display_title, "Stake");
    assert_string_equal(display_text, "123.456 AERGO");

    click_next();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

}

static void test_tx_display_governance_unstake (void **state) {
    (void) state;

    // clang-format off
    uint8_t raw_tx[] = {
        // tx type
        0x01,
        // transaction
        0x08, 0x82, 0x20, 0x12, 0x21, 0x02, 0x9d, 0x02,
        0x05, 0x91, 0xe7, 0xfb, 0x7b, 0x09, 0x21, 0x53,
        0x68, 0x19, 0x95, 0xf8, 0x06, 0x09, 0xf0, 0xac,
        0x98, 0x8a, 0x4d, 0x93, 0x5e, 0x0e, 0xa6, 0x3c,
        0x06, 0x0f, 0x19, 0x54, 0xb0, 0x5f, 0x1a, 0x0c,
        0x61, 0x65, 0x72, 0x67, 0x6f, 0x2e, 0x73, 0x79,
        0x73, 0x74, 0x65, 0x6d, 0x22, 0x0a, 0x1a, 0x24,
        0x91, 0xe2, 0x89, 0x5f, 0xc7, 0x30, 0xf3, 0x4e,
        0x2a, 0x14, 0x7b, 0x22, 0x4e, 0x61, 0x6d, 0x65,
        0x22, 0x3a, 0x22, 0x76, 0x31, 0x75, 0x6e, 0x73,
        0x74, 0x61, 0x6b, 0x65, 0x22, 0x7d, 0x3a, 0x01,
        0x00, 0x40, 0x01, 0x4a, 0x20, 0x52, 0x48, 0x45,
        0xc2, 0x4c, 0xd3, 0xe5, 0x3a, 0xec, 0xbc, 0xda,
        0x8e, 0x31, 0x5d, 0x62, 0xdc, 0x95, 0xa7, 0xf2,
        0xf8, 0x25, 0x48, 0x93, 0x0b, 0xc2, 0xfc, 0xc9,
        0x86, 0xbf, 0x74, 0x53, 0xbd,
    };

    int ret = setjmp(jump_buffer);
    assert_int_equal(ret, 0);

    send_transaction(raw_tx, sizeof(raw_tx));

    // generate_test_case();

    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    click_next();
    assert_string_equal(display_title, "Unstake");
    assert_string_equal(display_text, "123456.123456");

    click_next();
    assert_string_equal(display_title, "Unstake");
    assert_string_equal(display_text, "789012345678 ");

    click_next();
    assert_string_equal(display_title, "Unstake");
    assert_string_equal(display_text, "AERGO");

    click_next();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    // BACKWARDS

    click_prev();
    assert_string_equal(display_title, "Unstake");
    assert_string_equal(display_text, "AERGO");

    click_prev();
    assert_string_equal(display_title, "Unstake");
    assert_string_equal(display_text, "789012345678 ");

    click_prev();
    assert_string_equal(display_title, "Unstake");
    assert_string_equal(display_text, "123456.123456");

    click_prev();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    // again backwards 2 more times

    click_prev();
    assert_string_equal(display_title, "Unstake");
    assert_string_equal(display_text, "AERGO");

    click_prev();
    assert_string_equal(display_title, "Unstake");
    assert_string_equal(display_text, "789012345678 ");

    // then forward 4 times

    click_next();
    assert_string_equal(display_title, "Unstake");
    assert_string_equal(display_text, "AERGO");

    click_next();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    click_next();
    assert_string_equal(display_title, "Unstake");
    assert_string_equal(display_text, "123456.123456");

    click_next();
    assert_string_equal(display_title, "Unstake");
    assert_string_equal(display_text, "789012345678 ");

}

static void
test_tx_display_governance_bp_vote (void **state){
  (void) state;

    // clang-format off
    uint8_t raw_tx[] = {
        // tx type
        0x01,
        // transaction
        0x08, 0x80, 0x10, 0x12, 0x21, 0x02, 0x9d, 0x02,
        0x05, 0x91, 0xe7, 0xfb, 0x7b, 0x09, 0x21, 0x53,
        0x68, 0x19, 0x95, 0xf8, 0x06, 0x09, 0xf0, 0xac,
        0x98, 0x8a, 0x4d, 0x93, 0x5e, 0x0e, 0xa6, 0x3c,
        0x06, 0x0f, 0x19, 0x54, 0xb0, 0x5f, 0x1a, 0x0c,
        0x61, 0x65, 0x72, 0x67, 0x6f, 0x2e, 0x73, 0x79,
        0x73, 0x74, 0x65, 0x6d, 0x22, 0x01, 0x00, 0x2a,
        0x8a, 0x01, 0x7b, 0x22, 0x4e, 0x61, 0x6d, 0x65,
        0x22, 0x3a, 0x22, 0x76, 0x31, 0x76, 0x6f, 0x74,
        0x65, 0x42, 0x50, 0x22, 0x2c, 0x22, 0x41, 0x72,
        0x67, 0x73, 0x22, 0x3a, 0x5b, 0x22, 0x41, 0x6d,
        0x4d, 0x44, 0x45, 0x79, 0x63, 0x33, 0x36, 0x46,
        0x4e, 0x58, 0x42, 0x33, 0x46, 0x71, 0x31, 0x61,
        0x36, 0x31, 0x48, 0x65, 0x56, 0x4a, 0x52, 0x54,
        0x34, 0x79, 0x73, 0x73, 0x4d, 0x45, 0x50, 0x31,
        0x31, 0x4e, 0x57, 0x57, 0x45, 0x39, 0x51, 0x78,
        0x38, 0x79, 0x68, 0x66, 0x52, 0x4b, 0x65, 0x78,
        0x76, 0x71, 0x22, 0x2c, 0x22, 0x41, 0x6d, 0x4d,
        0x68, 0x4e, 0x5a, 0x56, 0x68, 0x69, 0x72, 0x64,
        0x56, 0x72, 0x67, 0x4c, 0x31, 0x31, 0x6b, 0x6f,
        0x55, 0x68, 0x31, 0x6a, 0x36, 0x54, 0x50, 0x6e,
        0x48, 0x31, 0x31, 0x38, 0x4b, 0x71, 0x78, 0x64,
        0x69, 0x68, 0x46, 0x44, 0x39, 0x59, 0x58, 0x48,
        0x44, 0x36, 0x33, 0x56, 0x70, 0x79, 0x46, 0x47,
        0x75, 0x22, 0x5d, 0x7d, 0x3a, 0x01, 0x00, 0x40,
        0x01, 0x4a, 0x20, 0x52, 0x48, 0x45, 0xc2, 0x4c,
        0xd3, 0xe5, 0x3a, 0xec, 0xbc, 0xda, 0x8e, 0x31,
        0x5d, 0x62, 0xdc, 0x95, 0xa7, 0xf2, 0xf8, 0x25,
        0x48, 0x93, 0x0b, 0xc2, 0xfc, 0xc9, 0x86, 0xbf,
        0x74, 0x53, 0xbd,
    };

    int ret = setjmp(jump_buffer);
    assert_int_equal(ret, 0);

    send_transaction(raw_tx, sizeof(raw_tx));

    // generate_test_case();

    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    click_next();
    assert_string_equal(display_title, "BP Vote");
    assert_string_equal(display_text, "\"AmMDEyc36FNX");

    click_next();
    assert_string_equal(display_title, "BP Vote");
    assert_string_equal(display_text, "B3Fq1a61HeVJR");

    click_next();
    assert_string_equal(display_title, "BP Vote");
    assert_string_equal(display_text, "T4yssMEP11NWW");

    click_next();
    assert_string_equal(display_title, "BP Vote");
    assert_string_equal(display_text, "E9Qx8yhfRKexv");

    click_next();
    assert_string_equal(display_title, "BP Vote");
    assert_string_equal(display_text, "q\",\"AmMhNZVhi");

    click_next();
    assert_string_equal(display_title, "BP Vote");
    assert_string_equal(display_text, "rdVrgL11koUh1");

    click_next();
    assert_string_equal(display_title, "BP Vote");
    assert_string_equal(display_text, "j6TPnH118Kqxd");

    click_next();
    assert_string_equal(display_title, "BP Vote");
    assert_string_equal(display_text, "ihFD9YXHD63Vp");

    click_next();
    assert_string_equal(display_title, "BP Vote");
    assert_string_equal(display_text, "yFGu\"");

    click_next();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    // BACKWARDS

    click_prev();
    assert_string_equal(display_title, "BP Vote");
    assert_string_equal(display_text, "yFGu\"");

    click_prev();
    assert_string_equal(display_title, "BP Vote");
    assert_string_equal(display_text, "ihFD9YXHD63Vp");

    click_prev();
    assert_string_equal(display_title, "BP Vote");
    assert_string_equal(display_text, "j6TPnH118Kqxd");

    click_prev();
    assert_string_equal(display_title, "BP Vote");
    assert_string_equal(display_text, "rdVrgL11koUh1");

    click_prev();
    assert_string_equal(display_title, "BP Vote");
    assert_string_equal(display_text, "q\",\"AmMhNZVhi");

    click_prev();
    assert_string_equal(display_title, "BP Vote");
    assert_string_equal(display_text, "E9Qx8yhfRKexv");

    click_prev();
    assert_string_equal(display_title, "BP Vote");
    assert_string_equal(display_text, "T4yssMEP11NWW");

    click_prev();
    assert_string_equal(display_title, "BP Vote");
    assert_string_equal(display_text, "B3Fq1a61HeVJR");

    click_prev();
    assert_string_equal(display_title, "BP Vote");
    assert_string_equal(display_text, "\"AmMDEyc36FNX");

    click_prev();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    // again backwards 2 more times

    click_prev();
    assert_string_equal(display_title, "BP Vote");
    assert_string_equal(display_text, "yFGu\"");

    click_prev();
    assert_string_equal(display_title, "BP Vote");
    assert_string_equal(display_text, "ihFD9YXHD63Vp");

    // then forward 4 times

    click_next();
    assert_string_equal(display_title, "BP Vote");
    assert_string_equal(display_text, "yFGu\"");

    click_next();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    click_next();
    assert_string_equal(display_title, "BP Vote");
    assert_string_equal(display_text, "\"AmMDEyc36FNX");

    click_next();
    assert_string_equal(display_title, "BP Vote");
    assert_string_equal(display_text, "B3Fq1a61HeVJR");

}

static void
test_tx_display_governance_dao_vote (void **state){
  (void) state;

    // clang-format off
    uint8_t raw_tx[] = {
        // tx type
        0x01,
        // transaction
        0x08, 0x80, 0x10, 0x12, 0x21, 0x02, 0x9d, 0x02,
        0x05, 0x91, 0xe7, 0xfb, 0x7b, 0x09, 0x21, 0x53,
        0x68, 0x19, 0x95, 0xf8, 0x06, 0x09, 0xf0, 0xac,
        0x98, 0x8a, 0x4d, 0x93, 0x5e, 0x0e, 0xa6, 0x3c,
        0x06, 0x0f, 0x19, 0x54, 0xb0, 0x5f, 0x1a, 0x0c,
        0x61, 0x65, 0x72, 0x67, 0x6f, 0x2e, 0x73, 0x79,
        0x73, 0x74, 0x65, 0x6d, 0x22, 0x01, 0x00, 0x2a,
        0x40, 0x7b, 0x22, 0x4e, 0x61, 0x6d, 0x65, 0x22,
        0x3a, 0x22, 0x76, 0x31, 0x76, 0x6f, 0x74, 0x65,
        0x44, 0x41, 0x4f, 0x22, 0x2c, 0x22, 0x41, 0x72,
        0x67, 0x73, 0x22, 0x3a, 0x5b, 0x22, 0x6e, 0x61,
        0x6d, 0x65, 0x70, 0x72, 0x69, 0x63, 0x65, 0x22,
        0x2c, 0x22, 0x32, 0x30, 0x30, 0x30, 0x30, 0x30,
        0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
        0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x22, 0x5d,
        0x7d, 0x3a, 0x01, 0x00, 0x40, 0x01, 0x4a, 0x20,
        0x52, 0x48, 0x45, 0xc2, 0x4c, 0xd3, 0xe5, 0x3a,
        0xec, 0xbc, 0xda, 0x8e, 0x31, 0x5d, 0x62, 0xdc,
        0x95, 0xa7, 0xf2, 0xf8, 0x25, 0x48, 0x93, 0x0b,
        0xc2, 0xfc, 0xc9, 0x86, 0xbf, 0x74, 0x53, 0xbd,
    };

    int ret = setjmp(jump_buffer);
    assert_int_equal(ret, 0);

    send_transaction(raw_tx, sizeof(raw_tx));

    // generate_test_case();

    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    click_next();
    assert_string_equal(display_title, "DAO Vote");
    assert_string_equal(display_text, "\"nameprice\",\"");

    click_next();
    assert_string_equal(display_title, "DAO Vote");
    assert_string_equal(display_text, "2000000000000");

    click_next();
    assert_string_equal(display_title, "DAO Vote");
    assert_string_equal(display_text, "0000000\"");

    click_next();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    // BACKWARDS

    click_prev();
    assert_string_equal(display_title, "DAO Vote");
    assert_string_equal(display_text, "0000000\"");

    click_prev();
    assert_string_equal(display_title, "DAO Vote");
    assert_string_equal(display_text, "2000000000000");

    click_prev();
    assert_string_equal(display_title, "DAO Vote");
    assert_string_equal(display_text, "\"nameprice\",\"");

    click_prev();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    // again backwards 2 more times

    click_prev();
    assert_string_equal(display_title, "DAO Vote");
    assert_string_equal(display_text, "0000000\"");

    click_prev();
    assert_string_equal(display_title, "DAO Vote");
    assert_string_equal(display_text, "2000000000000");

    // then forward 4 times

    click_next();
    assert_string_equal(display_title, "DAO Vote");
    assert_string_equal(display_text, "0000000\"");

    click_next();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    click_next();
    assert_string_equal(display_title, "DAO Vote");
    assert_string_equal(display_text, "\"nameprice\",\"");

    click_next();
    assert_string_equal(display_title, "DAO Vote");
    assert_string_equal(display_text, "2000000000000");

}

static void
test_tx_display_governance_create_name (void **state){
  (void) state;

    // clang-format off
    uint8_t raw_tx[] = {
        // tx type
        0x01,
        // transaction
        0x08, 0x80, 0x10, 0x12, 0x21, 0x02, 0x9d, 0x02,
        0x05, 0x91, 0xe7, 0xfb, 0x7b, 0x09, 0x21, 0x53,
        0x68, 0x19, 0x95, 0xf8, 0x06, 0x09, 0xf0, 0xac,
        0x98, 0x8a, 0x4d, 0x93, 0x5e, 0x0e, 0xa6, 0x3c,
        0x06, 0x0f, 0x19, 0x54, 0xb0, 0x5f, 0x1a, 0x0a,
        0x61, 0x65, 0x72, 0x67, 0x6f, 0x2e, 0x6e, 0x61,
        0x6d, 0x65, 0x22, 0x01, 0x00, 0x2a, 0x2d, 0x7b,
        0x22, 0x4e, 0x61, 0x6d, 0x65, 0x22, 0x3a, 0x22,
        0x76, 0x31, 0x63, 0x72, 0x65, 0x61, 0x74, 0x65,
        0x4e, 0x61, 0x6d, 0x65, 0x22, 0x2c, 0x22, 0x41,
        0x72, 0x67, 0x73, 0x22, 0x3a, 0x5b, 0x22, 0x63,
        0x72, 0x79, 0x70, 0x74, 0x6f, 0x62, 0x6f, 0x73,
        0x73, 0x22, 0x5d, 0x7d, 0x3a, 0x01, 0x00, 0x40,
        0x01, 0x4a, 0x20, 0x52, 0x48, 0x45, 0xc2, 0x4c,
        0xd3, 0xe5, 0x3a, 0xec, 0xbc, 0xda, 0x8e, 0x31,
        0x5d, 0x62, 0xdc, 0x95, 0xa7, 0xf2, 0xf8, 0x25,
        0x48, 0x93, 0x0b, 0xc2, 0xfc, 0xc9, 0x86, 0xbf,
        0x74, 0x53, 0xbd,
    };

    int ret = setjmp(jump_buffer);
    assert_int_equal(ret, 0);

    send_transaction(raw_tx, sizeof(raw_tx));

    //generate_test_case();

    click_next();
    assert_string_equal(display_title, "Create Name");
    assert_string_equal(display_text, "\"cryptoboss\"");

    click_next();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    // BACKWARDS

    click_prev();
    assert_string_equal(display_title, "Create Name");
    assert_string_equal(display_text, "\"cryptoboss\"");

    click_prev();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    // again backwards

    click_prev();
    assert_string_equal(display_title, "Create Name");
    assert_string_equal(display_text, "\"cryptoboss\"");

    click_prev();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    // again forward

    click_next();
    assert_string_equal(display_title, "Create Name");
    assert_string_equal(display_text, "\"cryptoboss\"");

    click_next();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

}

static void
test_tx_display_governance_update_name (void **state){
  (void) state;

    // clang-format off
    uint8_t raw_tx[] = {
        // tx type
        0x01,
        // transaction
        0x08, 0x80, 0x10, 0x12, 0x21, 0x02, 0x9d, 0x02,
        0x05, 0x91, 0xe7, 0xfb, 0x7b, 0x09, 0x21, 0x53,
        0x68, 0x19, 0x95, 0xf8, 0x06, 0x09, 0xf0, 0xac,
        0x98, 0x8a, 0x4d, 0x93, 0x5e, 0x0e, 0xa6, 0x3c,
        0x06, 0x0f, 0x19, 0x54, 0xb0, 0x5f, 0x1a, 0x0a,
        0x61, 0x65, 0x72, 0x67, 0x6f, 0x2e, 0x6e, 0x61,
        0x6d, 0x65, 0x22, 0x01, 0x00, 0x2a, 0x64, 0x7b,
        0x22, 0x4e, 0x61, 0x6d, 0x65, 0x22, 0x3a, 0x22,
        0x76, 0x31, 0x75, 0x70, 0x64, 0x61, 0x74, 0x65,
        0x4e, 0x61, 0x6d, 0x65, 0x22, 0x2c, 0x22, 0x41,
        0x72, 0x67, 0x73, 0x22, 0x3a, 0x5b, 0x22, 0x63,
        0x72, 0x79, 0x70, 0x74, 0x6f, 0x62, 0x6f, 0x73,
        0x73, 0x22, 0x2c, 0x22, 0x41, 0x6d, 0x4d, 0x44,
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

    // generate_test_case();

    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    click_next();
    assert_string_equal(display_title, "Update Name");
    assert_string_equal(display_text, "\"cryptoboss\",");

    click_next();
    assert_string_equal(display_title, "Update Name");
    assert_string_equal(display_text, "\"AmMDEyc36FNX");

    click_next();
    assert_string_equal(display_title, "Update Name");
    assert_string_equal(display_text, "B3Fq1a61HeVJR");

    click_next();
    assert_string_equal(display_title, "Update Name");
    assert_string_equal(display_text, "T4yssMEP11NWW");

    click_next();
    assert_string_equal(display_title, "Update Name");
    assert_string_equal(display_text, "E9Qx8yhfRKexv");

    click_next();
    assert_string_equal(display_title, "Update Name");
    assert_string_equal(display_text, "q\"");

    click_next();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    // BACKWARDS

    click_prev();
    assert_string_equal(display_title, "Update Name");
    assert_string_equal(display_text, "q\"");

    click_prev();
    assert_string_equal(display_title, "Update Name");
    assert_string_equal(display_text, "E9Qx8yhfRKexv");

    click_prev();
    assert_string_equal(display_title, "Update Name");
    assert_string_equal(display_text, "T4yssMEP11NWW");

    click_prev();
    assert_string_equal(display_title, "Update Name");
    assert_string_equal(display_text, "B3Fq1a61HeVJR");

    click_prev();
    assert_string_equal(display_title, "Update Name");
    assert_string_equal(display_text, "\"AmMDEyc36FNX");

    click_prev();
    assert_string_equal(display_title, "Update Name");
    assert_string_equal(display_text, "\"cryptoboss\",");

    click_prev();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    // again backwards 2 more times

    click_prev();
    assert_string_equal(display_title, "Update Name");
    assert_string_equal(display_text, "q\"");

    click_prev();
    assert_string_equal(display_title, "Update Name");
    assert_string_equal(display_text, "E9Qx8yhfRKexv");

    // then forward 4 times

    click_next();
    assert_string_equal(display_title, "Update Name");
    assert_string_equal(display_text, "q\"");

    click_next();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    click_next();
    assert_string_equal(display_title, "Update Name");
    assert_string_equal(display_text, "\"cryptoboss\",");

    click_next();
    assert_string_equal(display_title, "Update Name");
    assert_string_equal(display_text, "\"AmMDEyc36FNX");

}

static void
test_tx_display_governance_add_config (void **state){
  (void) state;

    // clang-format off
    uint8_t raw_tx[] = {
        // tx type
        0x01,
        // transaction
        0x08, 0x80, 0x10, 0x12, 0x21, 0x02, 0x9d, 0x02,
        0x05, 0x91, 0xe7, 0xfb, 0x7b, 0x09, 0x21, 0x53,
        0x68, 0x19, 0x95, 0xf8, 0x06, 0x09, 0xf0, 0xac,
        0x98, 0x8a, 0x4d, 0x93, 0x5e, 0x0e, 0xa6, 0x3c,
        0x06, 0x0f, 0x19, 0x54, 0xb0, 0x5f, 0x1a, 0x10,
        0x61, 0x65, 0x72, 0x67, 0x6f, 0x2e, 0x65, 0x6e,
        0x74, 0x65, 0x72, 0x70, 0x72, 0x69, 0x73, 0x65,
        0x22, 0x01, 0x00, 0x2a, 0x2e, 0x7b, 0x22, 0x4e,
        0x61, 0x6d, 0x65, 0x22, 0x3a, 0x22, 0x61, 0x70,
        0x70, 0x65, 0x6e, 0x64, 0x43, 0x6f, 0x6e, 0x66,
        0x22, 0x2c, 0x22, 0x41, 0x72, 0x67, 0x73, 0x22,
        0x3a, 0x5b, 0x22, 0x74, 0x69, 0x6d, 0x65, 0x6f,
        0x75, 0x74, 0x22, 0x2c, 0x31, 0x32, 0x33, 0x34,
        0x35, 0x5d, 0x7d, 0x3a, 0x01, 0x00, 0x40, 0x01,
        0x4a, 0x20, 0x52, 0x48, 0x45, 0xc2, 0x4c, 0xd3,
        0xe5, 0x3a, 0xec, 0xbc, 0xda, 0x8e, 0x31, 0x5d,
        0x62, 0xdc, 0x95, 0xa7, 0xf2, 0xf8, 0x25, 0x48,
        0x93, 0x0b, 0xc2, 0xfc, 0xc9, 0x86, 0xbf, 0x74,
        0x53, 0xbd,
    };

    int ret = setjmp(jump_buffer);
    assert_int_equal(ret, 0);

    send_transaction(raw_tx, sizeof(raw_tx));

    // generate_test_case();

    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    click_next();
    assert_string_equal(display_title, "Add Config");
    assert_string_equal(display_text, "\"timeout\",123");

    click_next();
    assert_string_equal(display_title, "Add Config");
    assert_string_equal(display_text, "45");

    click_next();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    // BACKWARDS

    click_prev();
    assert_string_equal(display_title, "Add Config");
    assert_string_equal(display_text, "45");

    click_prev();
    assert_string_equal(display_title, "Add Config");
    assert_string_equal(display_text, "\"timeout\",123");

    click_prev();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    // again backwards 2 more times

    click_prev();
    assert_string_equal(display_title, "Add Config");
    assert_string_equal(display_text, "45");

    click_prev();
    assert_string_equal(display_title, "Add Config");
    assert_string_equal(display_text, "\"timeout\",123");

    // then forward 4 times

    click_next();
    assert_string_equal(display_title, "Add Config");
    assert_string_equal(display_text, "45");

    click_next();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    click_next();
    assert_string_equal(display_title, "Add Config");
    assert_string_equal(display_text, "\"timeout\",123");

    click_next();
    assert_string_equal(display_title, "Add Config");
    assert_string_equal(display_text, "45");

}

static void
test_tx_display_governance_remove_config (void **state){
  (void) state;

    // clang-format off
    uint8_t raw_tx[] = {
        // tx type
        0x01,
        // transaction
        0x08, 0x80, 0x10, 0x12, 0x21, 0x02, 0x9d, 0x02,
        0x05, 0x91, 0xe7, 0xfb, 0x7b, 0x09, 0x21, 0x53,
        0x68, 0x19, 0x95, 0xf8, 0x06, 0x09, 0xf0, 0xac,
        0x98, 0x8a, 0x4d, 0x93, 0x5e, 0x0e, 0xa6, 0x3c,
        0x06, 0x0f, 0x19, 0x54, 0xb0, 0x5f, 0x1a, 0x10,
        0x61, 0x65, 0x72, 0x67, 0x6f, 0x2e, 0x65, 0x6e,
        0x74, 0x65, 0x72, 0x70, 0x72, 0x69, 0x73, 0x65,
        0x22, 0x01, 0x00, 0x2a, 0x2e, 0x7b, 0x22, 0x4e,
        0x61, 0x6d, 0x65, 0x22, 0x3a, 0x22, 0x72, 0x65,
        0x6d, 0x6f, 0x76, 0x65, 0x43, 0x6f, 0x6e, 0x66,
        0x22, 0x2c, 0x22, 0x41, 0x72, 0x67, 0x73, 0x22,
        0x3a, 0x5b, 0x22, 0x74, 0x69, 0x6d, 0x65, 0x6f,
        0x75, 0x74, 0x22, 0x2c, 0x31, 0x32, 0x33, 0x34,
        0x35, 0x5d, 0x7d, 0x3a, 0x01, 0x00, 0x40, 0x01,
        0x4a, 0x20, 0x52, 0x48, 0x45, 0xc2, 0x4c, 0xd3,
        0xe5, 0x3a, 0xec, 0xbc, 0xda, 0x8e, 0x31, 0x5d,
        0x62, 0xdc, 0x95, 0xa7, 0xf2, 0xf8, 0x25, 0x48,
        0x93, 0x0b, 0xc2, 0xfc, 0xc9, 0x86, 0xbf, 0x74,
        0x53, 0xbd,
    };

    int ret = setjmp(jump_buffer);
    assert_int_equal(ret, 0);

    send_transaction(raw_tx, sizeof(raw_tx));

    // generate_test_case();

    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    click_next();
    assert_string_equal(display_title, "Remove Config");
    assert_string_equal(display_text, "\"timeout\",123");

    click_next();
    assert_string_equal(display_title, "Remove Config");
    assert_string_equal(display_text, "45");

    click_next();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    // BACKWARDS

    click_prev();
    assert_string_equal(display_title, "Remove Config");
    assert_string_equal(display_text, "45");

    click_prev();
    assert_string_equal(display_title, "Remove Config");
    assert_string_equal(display_text, "\"timeout\",123");

    click_prev();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    // again backwards 2 more times

    click_prev();
    assert_string_equal(display_title, "Remove Config");
    assert_string_equal(display_text, "45");

    click_prev();
    assert_string_equal(display_title, "Remove Config");
    assert_string_equal(display_text, "\"timeout\",123");

    // then forward 4 times

    click_next();
    assert_string_equal(display_title, "Remove Config");
    assert_string_equal(display_text, "45");

    click_next();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    click_next();
    assert_string_equal(display_title, "Remove Config");
    assert_string_equal(display_text, "\"timeout\",123");

    click_next();
    assert_string_equal(display_title, "Remove Config");
    assert_string_equal(display_text, "45");

}

static void
test_tx_display_governance_enable_config (void **state){
  (void) state;

    // clang-format off
    uint8_t raw_tx[] = {
        // tx type
        0x01,
        // transaction
        0x08, 0x80, 0x10, 0x12, 0x21, 0x02, 0x9d, 0x02,
        0x05, 0x91, 0xe7, 0xfb, 0x7b, 0x09, 0x21, 0x53,
        0x68, 0x19, 0x95, 0xf8, 0x06, 0x09, 0xf0, 0xac,
        0x98, 0x8a, 0x4d, 0x93, 0x5e, 0x0e, 0xa6, 0x3c,
        0x06, 0x0f, 0x19, 0x54, 0xb0, 0x5f, 0x1a, 0x10,
        0x61, 0x65, 0x72, 0x67, 0x6f, 0x2e, 0x65, 0x6e,
        0x74, 0x65, 0x72, 0x70, 0x72, 0x69, 0x73, 0x65,
        0x22, 0x01, 0x00, 0x2a, 0x2e, 0x7b, 0x22, 0x4e,
        0x61, 0x6d, 0x65, 0x22, 0x3a, 0x22, 0x65, 0x6e,
        0x61, 0x62, 0x6c, 0x65, 0x43, 0x6f, 0x6e, 0x66,
        0x22, 0x2c, 0x22, 0x41, 0x72, 0x67, 0x73, 0x22,
        0x3a, 0x5b, 0x22, 0x74, 0x69, 0x6d, 0x65, 0x6f,
        0x75, 0x74, 0x22, 0x2c, 0x66, 0x61, 0x6c, 0x73,
        0x65, 0x5d, 0x7d, 0x3a, 0x01, 0x00, 0x40, 0x01,
        0x4a, 0x20, 0x52, 0x48, 0x45, 0xc2, 0x4c, 0xd3,
        0xe5, 0x3a, 0xec, 0xbc, 0xda, 0x8e, 0x31, 0x5d,
        0x62, 0xdc, 0x95, 0xa7, 0xf2, 0xf8, 0x25, 0x48,
        0x93, 0x0b, 0xc2, 0xfc, 0xc9, 0x86, 0xbf, 0x74,
        0x53, 0xbd,
    };

    int ret = setjmp(jump_buffer);
    assert_int_equal(ret, 0);

    send_transaction(raw_tx, sizeof(raw_tx));

    // generate_test_case();

    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    click_next();
    assert_string_equal(display_title, "Enable Config");
    assert_string_equal(display_text, "\"timeout\",fal");

    click_next();
    assert_string_equal(display_title, "Enable Config");
    assert_string_equal(display_text, "se");

    click_next();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    // BACKWARDS

    click_prev();
    assert_string_equal(display_title, "Enable Config");
    assert_string_equal(display_text, "se");

    click_prev();
    assert_string_equal(display_title, "Enable Config");
    assert_string_equal(display_text, "\"timeout\",fal");

    click_prev();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    // again backwards 2 more times

    click_prev();
    assert_string_equal(display_title, "Enable Config");
    assert_string_equal(display_text, "se");

    click_prev();
    assert_string_equal(display_title, "Enable Config");
    assert_string_equal(display_text, "\"timeout\",fal");

    // then forward 4 times

    click_next();
    assert_string_equal(display_title, "Enable Config");
    assert_string_equal(display_text, "se");

    click_next();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    click_next();
    assert_string_equal(display_title, "Enable Config");
    assert_string_equal(display_text, "\"timeout\",fal");

    click_next();
    assert_string_equal(display_title, "Enable Config");
    assert_string_equal(display_text, "se");
}

static void
test_tx_display_governance_change_cluster (void **state){
  (void) state;

    // clang-format off
    uint8_t raw_tx[] = {
        // tx type
        0x01,
        // transaction
        0x08, 0x80, 0x10, 0x12, 0x21, 0x02, 0x9d, 0x02,
        0x05, 0x91, 0xe7, 0xfb, 0x7b, 0x09, 0x21, 0x53,
        0x68, 0x19, 0x95, 0xf8, 0x06, 0x09, 0xf0, 0xac,
        0x98, 0x8a, 0x4d, 0x93, 0x5e, 0x0e, 0xa6, 0x3c,
        0x06, 0x0f, 0x19, 0x54, 0xb0, 0x5f, 0x1a, 0x10,
        0x61, 0x65, 0x72, 0x67, 0x6f, 0x2e, 0x65, 0x6e,
        0x74, 0x65, 0x72, 0x70, 0x72, 0x69, 0x73, 0x65,
        0x22, 0x01, 0x00, 0x2a, 0xb7, 0x01, 0x7b, 0x22,
        0x4e, 0x61, 0x6d, 0x65, 0x22, 0x3a, 0x22, 0x63,
        0x68, 0x61, 0x6e, 0x67, 0x65, 0x43, 0x6c, 0x75,
        0x73, 0x74, 0x65, 0x72, 0x22, 0x2c, 0x22, 0x41,
        0x72, 0x67, 0x73, 0x22, 0x3a, 0x5b, 0x7b, 0x22,
        0x63, 0x6f, 0x6d, 0x6d, 0x61, 0x6e, 0x64, 0x22,
        0x3a, 0x22, 0x61, 0x64, 0x64, 0x22, 0x2c, 0x22,
        0x6e, 0x61, 0x6d, 0x65, 0x22, 0x3a, 0x22, 0x73,
        0x6e, 0x61, 0x70, 0x73, 0x68, 0x6f, 0x74, 0x5f,
        0x6e, 0x6f, 0x64, 0x65, 0x34, 0x22, 0x2c, 0x22,
        0x61, 0x64, 0x64, 0x72, 0x65, 0x73, 0x73, 0x22,
        0x3a, 0x22, 0x2f, 0x64, 0x6e, 0x73, 0x2f, 0x61,
        0x6c, 0x70, 0x68, 0x61, 0x33, 0x2e, 0x61, 0x65,
        0x72, 0x67, 0x6f, 0x2e, 0x69, 0x6f, 0x2f, 0x74,
        0x63, 0x70, 0x2f, 0x31, 0x37, 0x38, 0x34, 0x36,
        0x22, 0x2c, 0x22, 0x70, 0x65, 0x65, 0x72, 0x69,
        0x64, 0x22, 0x3a, 0x22, 0x31, 0x36, 0x55, 0x69,
        0x75, 0x32, 0x48, 0x41, 0x6d, 0x53, 0x31, 0x51,
        0x51, 0x50, 0x48, 0x66, 0x62, 0x73, 0x6a, 0x64,
        0x6e, 0x35, 0x76, 0x72, 0x51, 0x43, 0x4c, 0x6d,
        0x4a, 0x42, 0x56, 0x5a, 0x34, 0x76, 0x36, 0x75,
        0x36, 0x44, 0x53, 0x34, 0x37, 0x64, 0x53, 0x53,
        0x38, 0x4e, 0x6d, 0x68, 0x44, 0x67, 0x46, 0x7a,
        0x38, 0x22, 0x7d, 0x5d, 0x7d, 0x3a, 0x01, 0x00,
        0x40, 0x01, 0x4a, 0x20, 0x52, 0x48, 0x45, 0xc2,
        0x4c, 0xd3, 0xe5, 0x3a, 0xec, 0xbc, 0xda, 0x8e,
        0x31, 0x5d, 0x62, 0xdc, 0x95, 0xa7, 0xf2, 0xf8,
        0x25, 0x48, 0x93, 0x0b, 0xc2, 0xfc, 0xc9, 0x86,
        0xbf, 0x74, 0x53, 0xbd,
    };

    int ret = setjmp(jump_buffer);
    assert_int_equal(ret, 0);

    send_transaction(raw_tx, sizeof(raw_tx));

    //generate_test_case();

    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    click_next();
    assert_string_equal(display_title, "Change Cluster");
    assert_string_equal(display_text, "{\"command\":\"a");

    click_next();
    assert_string_equal(display_title, "Change Cluster");
    assert_string_equal(display_text, "dd\",\"name\":\"s");

    click_next();
    assert_string_equal(display_title, "Change Cluster");
    assert_string_equal(display_text, "napshot_node4");

    click_next();
    assert_string_equal(display_title, "Change Cluster");
    assert_string_equal(display_text, "\",\"address\":\"");

    click_next();
    assert_string_equal(display_title, "Change Cluster");
    assert_string_equal(display_text, "/dns/alpha3.a");

    click_next();
    assert_string_equal(display_title, "Change Cluster");
    assert_string_equal(display_text, "ergo.io/tcp/1");

    click_next();
    assert_string_equal(display_title, "Change Cluster");
    assert_string_equal(display_text, "7846\",\"peerid");

    click_next();
    assert_string_equal(display_title, "Change Cluster");
    assert_string_equal(display_text, "\":\"16Uiu2HAmS");

    click_next();
    assert_string_equal(display_title, "Change Cluster");
    assert_string_equal(display_text, "1QQPHfbsjdn5v");

    click_next();
    assert_string_equal(display_title, "Change Cluster");
    assert_string_equal(display_text, "rQCLmJBVZ4v6u");

    click_next();
    assert_string_equal(display_title, "Change Cluster");
    assert_string_equal(display_text, "6DS47dSS8NmhD");

    click_next();
    assert_string_equal(display_title, "Change Cluster");
    assert_string_equal(display_text, "gFz8\"}");

    click_next();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    // BACKWARDS

    click_prev();
    assert_string_equal(display_title, "Change Cluster");
    assert_string_equal(display_text, "gFz8\"}");

    click_prev();
    assert_string_equal(display_title, "Change Cluster");
    assert_string_equal(display_text, "6DS47dSS8NmhD");

    click_prev();
    assert_string_equal(display_title, "Change Cluster");
    assert_string_equal(display_text, "rQCLmJBVZ4v6u");

    click_prev();
    assert_string_equal(display_title, "Change Cluster");
    assert_string_equal(display_text, "1QQPHfbsjdn5v");

    click_prev();
    assert_string_equal(display_title, "Change Cluster");
    assert_string_equal(display_text, "\":\"16Uiu2HAmS");

    click_prev();
    assert_string_equal(display_title, "Change Cluster");
    assert_string_equal(display_text, "7846\",\"peerid");

    click_prev();
    assert_string_equal(display_title, "Change Cluster");
    assert_string_equal(display_text, "ergo.io/tcp/1");

    click_prev();
    assert_string_equal(display_title, "Change Cluster");
    assert_string_equal(display_text, "/dns/alpha3.a");

    click_prev();
    assert_string_equal(display_title, "Change Cluster");
    assert_string_equal(display_text, "\",\"address\":\"");

    click_prev();
    assert_string_equal(display_title, "Change Cluster");
    assert_string_equal(display_text, "napshot_node4");

    click_prev();
    assert_string_equal(display_title, "Change Cluster");
    assert_string_equal(display_text, "dd\",\"name\":\"s");

    click_prev();
    assert_string_equal(display_title, "Change Cluster");
    assert_string_equal(display_text, "{\"command\":\"a");

    click_prev();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    // again backwards 2 more times

    click_prev();
    assert_string_equal(display_title, "Change Cluster");
    assert_string_equal(display_text, "gFz8\"}");

    click_prev();
    assert_string_equal(display_title, "Change Cluster");
    assert_string_equal(display_text, "6DS47dSS8NmhD");

    // then forward 4 times

    click_next();
    assert_string_equal(display_title, "Change Cluster");
    assert_string_equal(display_text, "gFz8\"}");

    click_next();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    click_next();
    assert_string_equal(display_title, "Change Cluster");
    assert_string_equal(display_text, "{\"command\":\"a");

    click_next();
    assert_string_equal(display_title, "Change Cluster");
    assert_string_equal(display_text, "dd\",\"name\":\"s");

}

// MESSAGE
static void test_display_message(void **state) {
    (void) state;

    unsigned char message[] = "Hello World! This message must be signed";

    int ret = setjmp(jump_buffer);
    assert_int_equal(ret, 0);

    on_new_message(message, strlen((char*)message), false);

    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    click_next();
    assert_string_equal(display_title, "Message");
    assert_string_equal(display_text, "Hello World! ");

    click_next();
    assert_string_equal(display_title, "Message");
    assert_string_equal(display_text, "This message ");

    click_next();
    assert_string_equal(display_title, "Message");
    assert_string_equal(display_text, "must be signe");

    click_next();
    assert_string_equal(display_title, "Message");
    assert_string_equal(display_text, "d");

    click_next();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    // BACKWARDS

    click_prev();
    assert_string_equal(display_title, "Message");
    assert_string_equal(display_text, "d");

    click_prev();
    assert_string_equal(display_title, "Message");
    assert_string_equal(display_text, "must be signe");

    click_prev();
    assert_string_equal(display_title, "Message");
    assert_string_equal(display_text, "This message ");

    click_prev();
    assert_string_equal(display_title, "Message");
    assert_string_equal(display_text, "Hello World! ");

    click_prev();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    // again backwards 2 more times

    click_prev();
    assert_string_equal(display_title, "Message");
    assert_string_equal(display_text, "d");

    click_prev();
    assert_string_equal(display_title, "Message");
    assert_string_equal(display_text, "must be signe");

    // then forward 4 times

    click_next();
    assert_string_equal(display_title, "Message");
    assert_string_equal(display_text, "d");

    click_next();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    click_next();
    assert_string_equal(display_title, "Message");
    assert_string_equal(display_text, "Hello World! ");

    click_next();
    assert_string_equal(display_title, "Message");
    assert_string_equal(display_text, "This message ");
}

// Message ADDRESS
static void test_display_account(void **state) {
    (void) state;

    // clang-format off
    uint8_t public_key_bytes[] = {
        0x01, 0x02,
        0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a,
        0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12,
        0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a,
        0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20
    };

    int ret = setjmp(jump_buffer);
    assert_int_equal(ret, 0);

    on_display_account(public_key_bytes, sizeof(public_key_bytes));

    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    click_next();
    assert_string_equal(display_title, "Account");
    assert_string_equal(display_text, "3DJaPUm8nu9hx");

    click_next();
    assert_string_equal(display_title, "Account");
    assert_string_equal(display_text, "YPGZfFDzEBx2r");

    click_next();
    assert_string_equal(display_title, "Account");
    assert_string_equal(display_text, "PbTzVnou5nPP9");

    click_next();
    assert_string_equal(display_title, "Account");
    assert_string_equal(display_text, "3ytgqbFmGzhv");

    click_next();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    // BACKWARDS

    click_prev();
    assert_string_equal(display_title, "Account");
    assert_string_equal(display_text, "3ytgqbFmGzhv");

    click_prev();
    assert_string_equal(display_title, "Account");
    assert_string_equal(display_text, "PbTzVnou5nPP9");

    click_prev();
    assert_string_equal(display_title, "Account");
    assert_string_equal(display_text, "YPGZfFDzEBx2r");

    click_prev();
    assert_string_equal(display_title, "Account");
    assert_string_equal(display_text, "3DJaPUm8nu9hx");

    click_prev();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    // again backwards 2 more times

    click_prev();
    assert_string_equal(display_title, "Account");
    assert_string_equal(display_text, "3ytgqbFmGzhv");

    click_prev();
    assert_string_equal(display_title, "Account");
    assert_string_equal(display_text, "PbTzVnou5nPP9");

    // then forward 4 times

    click_next();
    assert_string_equal(display_title, "Account");
    assert_string_equal(display_text, "3ytgqbFmGzhv");

    click_next();
    assert_string_equal(display_title, "Review");
    assert_string_equal(display_text, "Transaction");

    click_next();
    assert_string_equal(display_title, "Account");
    assert_string_equal(display_text, "3DJaPUm8nu9hx");

    click_next();
    assert_string_equal(display_title, "Account");
    assert_string_equal(display_text, "YPGZfFDzEBx2r");
}

void debug_it() {

    // clang-format off
    uint8_t raw_tx[] = {
0x6,0x8,0x80,0x4,0x12,0x21,0x2,0x9d,0x2,0x5,0x91,0xe7,0xfb,0x7b,0x9,0x21,0x53,0x68,0x19,0x95,0xf8,0x6,0x9,0xf0,0xac,0x98,0x8a,0x4d,0x93,0x5e,0xe,0xa6,0x3c,0x6,0xf,0x19,0x54,0xb0,0x5f,0x1a,0x21,0x3,0x8c,0xb9,0x2c,0xde,0xbf,0x39,0x98,0x69,0x9,0x3c,0xac,0x47,0xe3,0x70,0x28,0x56,0x5,0xaf,0xe8,0xcf,0xbd,0xd3,0xf9,0xad,0x1a,0xa9,0x5,0x8c,0xa,0x6,0xa9,0x8b,0x22,0xc,0x3,0xfd,0x35,0xeb,0x6d,0x79,0x7a,0x91,0xbe,0x38,0xf3,0x4e,0x2a,0xa2,0x2,0x7b,0x22,0x4e,0x61,0x6d,0x65,0x22,0x3a,0x22,0x0,0x0,0x0,0x0,0x0,0x0,0x9,0x0,0x9,0x9,0x0,0x3d,0x9,0x9,0x9,0x9,0x13,0x9,0x9,0x9,0x9,0x9,0x9,0x9,0x9,0x9,0x9,0x9,0x7b,0x22,0x9,0x9,0x9,0x9,0x9,0x9,0x9,0x9,0x9,0x9,0x9,0x9,0x9,0x9,0x9,0x9,0x9,0x9,0x9,0x9,0x9,0x9,0x9,0x9,0x9,0x9,0x9,0x9,0x9,0x9,0x9,0x9,0x9,0x9,0x9,0x9,0x9,0x9,0x0,0x0,0x0,0x0,0x0,0x2a,0xa2,0x2,0x7b,0x22,0x4e,0x61,0x6d,0x65,0x22,0x3a,0x22,0x0,0x0,0x0,0x4,0x0,0x0,0x0,0x0,0x9,0x0,0x9,0x9,0x0,0x3d,0x9,0x9,0x9,0x9,0x9,0x9,0x9,0x9,0x9,0x13,0x9,0x9,0x9,0x9,0x9,0x9,0x9,0x9,0x9,0x9,0x9,0x9,0x9,0x9,0xf7,0xf6,0xf6,0xf6,0xf6,0xf6,0xf6,0xf2,0x9,0xff,0xff,0xff,0xff,0x9,0x9,0x9,0x9,0x9,0x9,0xff,0x0,
    };

    int ret = setjmp(jump_buffer);
    assert_int_equal(ret, 0);

    send_transaction(raw_tx, sizeof(raw_tx));

    if (current_state != STATIC_SCREEN) goto loc_exit;
    click_next();

    while (current_state == DYNAMIC_SCREEN) {
      click_next();
    }

    if (current_state != STATIC_SCREEN) goto loc_exit;
    click_prev();

    while (current_state == DYNAMIC_SCREEN) {
      click_prev();
    }

loc_exit:
    puts("exiting...");
}

int main() {
    const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_tx_display_normal),
      cmocka_unit_test(test_tx_display_normal_long_payload),
      cmocka_unit_test(test_tx_display_transfer_1),
      cmocka_unit_test(test_tx_display_transfer_2),
      cmocka_unit_test(test_tx_display_call_1),
      cmocka_unit_test(test_tx_display_call_2),
      cmocka_unit_test(test_tx_display_call_big),
      cmocka_unit_test(test_tx_display_fee_delegation_call),
      cmocka_unit_test(test_tx_display_multicall_1),
      cmocka_unit_test(test_tx_display_multicall_2),
      cmocka_unit_test(test_tx_display_multicall_3),
      cmocka_unit_test(test_tx_display_multicall_4),
      cmocka_unit_test(test_tx_display_deploy_1),
      cmocka_unit_test(test_tx_display_deploy_2),
      cmocka_unit_test(test_tx_display_redeploy),
      // governance
      cmocka_unit_test(test_tx_display_governance_stake),
      cmocka_unit_test(test_tx_display_governance_unstake),
      cmocka_unit_test(test_tx_display_governance_bp_vote),
      cmocka_unit_test(test_tx_display_governance_dao_vote),
      cmocka_unit_test(test_tx_display_governance_create_name),
      cmocka_unit_test(test_tx_display_governance_update_name),
      cmocka_unit_test(test_tx_display_governance_add_admin),
      cmocka_unit_test(test_tx_display_governance_remove_admin),
      cmocka_unit_test(test_tx_display_governance_add_config),
      cmocka_unit_test(test_tx_display_governance_remove_config),
      cmocka_unit_test(test_tx_display_governance_enable_config),
      cmocka_unit_test(test_tx_display_governance_change_cluster),
      // message
      cmocka_unit_test(test_display_message),
      // account address
      cmocka_unit_test(test_display_account),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
