#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>

#include "testing.h"
#include "../src/globals.h"
#include "../src/transaction.h"

static int parse_transaction(const unsigned char *buf, unsigned int len){
  bool is_first, is_last;
  unsigned char *ptr = (unsigned char *) buf;
  unsigned int remaining = len;
  int ret;

  ret = setjmp(jump_buffer);
  //if (ret != 0) return ret;

  while (remaining > 0 && ret == 0) {
    unsigned int bytes_now = remaining;
    if (bytes_now > MAX_TX_PART) bytes_now = MAX_TX_PART;
    remaining -= bytes_now;
    is_first = (ptr == buf);
    is_last = (remaining == 0);
    parse_transaction_part(ptr, bytes_now, is_first, is_last);
    ptr += bytes_now;
  }

  return ret;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  parse_transaction(data, size);
  return 0;
}
