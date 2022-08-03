#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>

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

unsigned char *txn_ptr = NULL;
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
  unsigned char *ptr;

  memset(&txn, 0, sizeof(struct txn));

  txn_ptr = malloc(len);
  if (txn_ptr) {
    memcpy(txn_ptr, buf, len);
  }
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
// TEST CASES
////////////////////////////////////////////////////////////////////////////////

static void process_transaction(const unsigned char *buf, unsigned int len){
  int ret;

  ret = setjmp(jump_buffer);
  if (ret != 0) goto loc_exit;

  send_transaction(buf, len);

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
  if (txn_ptr) { free(txn_ptr); txn_ptr = NULL; }

}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  process_transaction(data, size);
  return 0;
}
