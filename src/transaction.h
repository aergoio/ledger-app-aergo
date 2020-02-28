
#define AddressLength         33
#define EncodedAddressLength  52

#define AddressVersion      0x42

#include "account.h"
#include "currency.h"

#define TXN_NORMAL         0
#define TXN_GOVERNANCE     1
#define TXN_REDEPLOY       2
#define TXN_FEEDELEGATION  3
#define TXN_TRANSFER       4
#define TXN_CALL           5
#define TXN_DEPLOY         6

struct txn {
  uint64_t nonce;
  unsigned char *account;       // public key - 33 bytes
  unsigned char *recipient;     // public key - 33 bytes
  unsigned char *amount;        // variable-length big integer
           char *payload;
  unsigned int   payload_len;
  uint64_t gasLimit;
  unsigned char *gasPrice;      // variable-length big integer
  uint32_t type;
  unsigned char *chainId;       // hash value of chain identifier - 32 bytes
};

struct txn txn;

char recipient_address[EncodedAddressLength+1]; // encoded account address

char amount_str[32];

unsigned char txn_type;

static unsigned int decode_varint(unsigned char *buf, unsigned int max_len, uint64_t *dest){
  unsigned char byte;
  uint_fast8_t bitpos = 0;
  unsigned int len = 0;
  uint64_t result = 0;

  do {
    if (bitpos >= 64) {
      THROW(0x6984);  // invalid data
    }

    if (len >= max_len) return 0;
    byte = buf[len];
    len++;

    result |= (uint64_t)(byte & 0x7F) << bitpos;
    bitpos = (uint_fast8_t)(bitpos + 7);
  } while (byte & 0x80);

  *dest = result;
  return len;
}

static char * stripstr(char *mainstr, char *separator) {
  char *ptr;

  if (!mainstr || !separator) return NULL;

  ptr = strstr(mainstr, separator);
  if (ptr == 0) return NULL;

  ptr[0] = '\0';
  ptr += strlen(separator);
  return ptr;
}

#define sha256_add(ptr,len) cx_hash(&hash.header,0,(unsigned char*)ptr,len,NULL,0)

static bool parse_first_part(unsigned char *buf, unsigned int len){
  unsigned char *ptr;
  unsigned int size;
  uint64_t str_len, val64;

  memset(&txn, 0, sizeof(struct txn));

  ptr = buf;

  // nonce
  if (len < 1) goto loc_incomplete;
  if (*ptr != 0x08) goto loc_invalid;
  ptr++; len--;
  size = decode_varint(ptr, len, &txn.nonce);
  if (size == 0) goto loc_incomplete2;
  ptr += size;
  len -= size;

  sha256_add(&txn.nonce, 8);

  // account
  if (len < 1) goto loc_incomplete;
  if (*ptr != 0x12) goto loc_invalid;
  ptr++; len--;
  size = decode_varint(ptr, len, &str_len);
  if (size == 0 || len - size < str_len) goto loc_incomplete2;
  ptr += size;
  len -= size;
  if (str_len != 33) goto loc_invalid;
  txn.account = ptr;
  ptr += str_len;
  len -= str_len;

  sha256_add(txn.account, str_len);

  // recipient
  if (len < 1) goto loc_incomplete;
  if (*ptr == 0x1A) {
    ptr++; len--;
    size = decode_varint(ptr, len, &str_len);
    if (size == 0 || len - size < str_len) goto loc_incomplete2;
    ptr += size;
    len -= size;
    if (str_len != 33) goto loc_invalid;
    txn.recipient = ptr;
    ptr += str_len;
    len -= str_len;

    sha256_add(txn.recipient, str_len);

    encode_account(txn.recipient, str_len, recipient_address, sizeof recipient_address);
  } else {
    recipient_address[0] = 0;
  }

  // amount
  if (len < 1) goto loc_incomplete;
  if (*ptr != 0x22) goto loc_invalid;
  ptr++; len--;
  size = decode_varint(ptr, len, &str_len);
  if (size == 0 || len - size < str_len) goto loc_incomplete2;
  ptr += size;
  len -= size;
  if (str_len > 20) goto loc_invalid;
  txn.amount = ptr;
  ptr += str_len;
  len -= str_len;

  sha256_add(txn.amount, str_len);

  encode_amount(txn.amount, str_len, amount_str, sizeof amount_str);

  // payload
  if (len < 1) goto loc_incomplete;
  if (*ptr == 0x2A) {
    ptr++; len--;
    size = decode_varint(ptr, len, &str_len);
    if (size == 0) goto loc_incomplete2;
    ptr += size;
    len -= size;
    if (str_len > 140) goto loc_invalid;
    if (len < str_len) goto loc_incomplete;
    txn.payload = (char*) ptr;
    txn.payload_len = str_len;
    ptr += str_len;
    len -= str_len;

    sha256_add(txn.payload, str_len);
  }

  // gasLimit
  if (len < 1) goto loc_incomplete;
  if (*ptr != 0x30) goto loc_invalid;
  ptr++; len--;
  size = decode_varint(ptr, len, &txn.gasLimit);
  if (size == 0) goto loc_incomplete2;
  ptr += size;
  len -= size;

  sha256_add(&txn.gasLimit, 8);

  // gasPrice
  if (len < 1) goto loc_incomplete;
  if (*ptr == 0x3A) {
    ptr++; len--;
    size = decode_varint(ptr, len, &str_len);
    if (size == 0 || len - size < str_len) goto loc_incomplete2;
    ptr += size;
    len -= size;
    if (str_len > 22) goto loc_invalid;
    txn.gasPrice = ptr;
    ptr += str_len;
    len -= str_len;
  } else {
    txn.gasPrice = (unsigned char *) "\0";
    str_len = 1;
  }

  sha256_add(txn.gasPrice, str_len);

  // type
  if (len < 1) goto loc_incomplete;
  if (*ptr != 0x40) goto loc_invalid;
  ptr++; len--;
  size = decode_varint(ptr, len, &val64);
  if (size == 0) goto loc_incomplete2;
  ptr += size;
  len -= size;
  txn.type = val64; /* convert to 32-bit */
  txn_type = txn.type;

  sha256_add(&txn.type, 4);

  // chain_id
  if (len < 1) goto loc_incomplete;
  if (*ptr != 0x4A) goto loc_invalid;
  ptr++; len--;
  size = decode_varint(ptr, len, &str_len);
  if (size == 0 || len - size < str_len) goto loc_incomplete2;
  ptr += size;
  len -= size;
  if (str_len != 32) goto loc_invalid;
  txn.chainId = ptr;
  ptr += str_len;
  len -= str_len;

  sha256_add(txn.chainId, 32);





  return true;

loc_incomplete2:
  ptr--;
  len++;
loc_incomplete:

//  ...

  return false;

loc_invalid:

  THROW(0x6984);  // invalid data

}


static void on_new_transaction_part(unsigned char *buf, unsigned int len, bool is_first, bool is_last){


  if (is_first) {
    // check minimum size
    if (len < 60) {
      THROW(0x6700);  // wrong length
    }
    // initialize hash
    cx_sha256_init(&hash);
  }


  parse_first_part(buf, len);


  /* calculate the transaction hash */
  memset(txn_hash, 0, sizeof txn_hash);
  cx_hash(&hash.header, CX_LAST, NULL, 0, txn_hash, sizeof txn_hash);


  /* determine what to display according to the transaction type */
  switch (txn_type) {
  case TXN_TRANSFER: {
    char *ptr;
    unsigned int len, i;

    screens[0].title = "Amount";
    screens[0].value = amount_str;
    screens[0].vsize = strlen(amount_str);

    ptr = recipient_address;
    len = EncodedAddressLength;  // strlen(recipient_address);

    i = 0;
    while (len > 0) {
      i++;
      screens[i].title = "Recipient";
      screens[i].value = ptr;
      screens[i].vsize = len > MAX_CHARS_PER_LINE ? MAX_CHARS_PER_LINE : len;
      ptr += screens[i].vsize;
      len -= screens[i].vsize;
    }

    num_screens = i;

    break;
  }
  case TXN_CALL: {
    char *name, *args;

    /* parse the payload */
    /* {"Name":"some_function","Args":[<parameters>]} */

    if (strncmp(txn.payload, "{\"Name\":\"", 9) != 0) goto loc_invalid;
    name = txn.payload + 9;
    if (txn.payload[txn.payload_len-2] != ']' ||
        txn.payload[txn.payload_len-1] != '}' ) goto loc_invalid;
    txn.payload[txn.payload_len-2] = 0;
    args = stripstr(name, "\",\"Args\":[");

    /* set the 3 screens to be displayed */

    screens[0].title = "Contract";
    screens[0].value = recipient_address;

    screens[1].title = "Function";
    screens[1].value = name;

    screens[2].title = "Parameters";
    screens[2].value = args;

    num_screens = 3;

    break;
  }
  case TXN_GOVERNANCE:
    if (strcmp(txn.payload,"{\"Name\":\"v1stake\"}") == 0) {

      screens[0].title = "Stake";
      screens[0].value = amount_str;
      num_screens = 1;

    } else if (strcmp(txn.payload,"{\"Name\":\"v1unstake\"}") == 0) {

      screens[0].title = "Unstake";
      screens[0].value = amount_str;
      num_screens = 1;

    }
    break;
  //case TXN_NORMAL:
  //case TXN_DEPLOY:
  //case TXN_REDEPLOY:
  //case TXN_FEEDELEGATION:
  default:
    goto loc_invalid;
  }


  /* display the first screen */
  display_screen(0);


  return;

loc_invalid:

  THROW(0x6984);  // invalid data

}
