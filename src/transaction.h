
#define AddressLength         33
#define EncodedAddressLength  52

#define AddressVersion      0x42

#include "common/account.h"
#include "common/currency.h"

#define TXN_NORMAL         0
#define TXN_GOVERNANCE     1
#define TXN_REDEPLOY       2
#define TXN_FEEDELEGATION  3
#define TXN_TRANSFER       4
#define TXN_CALL           5
#define TXN_DEPLOY         6
#define TXN_MULTICALL      7

struct txn {
  uint64_t nonce;
  unsigned char *account;       // public key - 33 bytes
  unsigned char *recipient;     // public key - 33 bytes
  unsigned char *amount;        // variable-length big integer
           char *payload;
  unsigned int   payload_len;
  unsigned int   payload_part_offset;
  unsigned int   payload_part_len;
  uint64_t gasLimit;
  unsigned char *gasPrice;      // variable-length big integer
  uint32_t type;
  unsigned char *chainId;       // hash value of chain identifier - 32 bytes
  bool is_name;
  bool is_system;
  bool is_enterprise;
};

struct txn txn;

char recipient_address[EncodedAddressLength+1]; // encoded account address

char amount_str[48];

unsigned char txn_type;

char last_part[128];
int last_part_len;
int last_part_pos;

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
#define sha256_add_payload(ptr,len) cx_hash(&hash2.header,0,(unsigned char*)ptr,len,NULL,0)
#define sha256_add_message(ptr,len) cx_hash(&hash3.header,0,(unsigned char*)ptr,len,NULL,0)

static bool parse_payload_part(unsigned char *ptr, unsigned int len);
static bool parse_last_part(unsigned char *ptr, unsigned int len);

static bool parse_first_part(unsigned char *ptr, unsigned int len){
  unsigned int size;
  uint64_t str_len;
  unsigned int pos = 0;

  memset(&txn, 0, sizeof(struct txn));

  txn_is_complete = false;
  has_partial_payload = false;
  last_part_len = 0;
  last_part_pos = 0;

  // initialize hash
  memset(txn_hash, 0, sizeof txn_hash);
  memset(payload_hash, 0, sizeof payload_hash);
  cx_sha256_init(&hash);
  cx_sha256_init(&hash2);

  // transaction type
  if (len < 1) goto loc_incomplete;
  txn_type = *ptr;
  ptr++; len--;

  pos = 1;

  // nonce
  if (len < 1) goto loc_incomplete;
  if (*ptr != 0x08) goto loc_invalid;
  ptr++; len--;
  size = decode_varint(ptr, len, &txn.nonce);
  if (size == 0) goto loc_incomplete2;
  ptr += size;
  len -= size;

  sha256_add(&txn.nonce, 8);

  pos = 2;

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

  pos = 3;

  // recipient
  if (len < 1) goto loc_incomplete;
  if (*ptr == 0x1A) {
    ptr++; len--;
    size = decode_varint(ptr, len, &str_len);
    if (size == 0 || len - size < str_len) goto loc_incomplete2;
    ptr += size;
    len -= size;
    if (str_len != 33) {
      if (str_len==12 && strncmp((char*)ptr,"aergo.system",12) == 0) {
        txn.is_system = true;
      } else if (str_len==10 && strncmp((char*)ptr,"aergo.name",10) == 0) {
        txn.is_name = true;
      } else if (str_len==16 && strncmp((char*)ptr,"aergo.enterprise",16) == 0) {
        txn.is_enterprise = true;
      } else if (str_len > 30) {  /* name system accounts limited to 30 characters */
        goto loc_invalid;
      }
    }
    txn.recipient = ptr;
    ptr += str_len;
    len -= str_len;

    sha256_add(txn.recipient, str_len);

    if (str_len == 33) {
      encode_account(txn.recipient, str_len, recipient_address, sizeof recipient_address);
    } else {
      memmove(recipient_address, txn.recipient, str_len);
      recipient_address[str_len] = 0;
    }
  } else {
    recipient_address[0] = 0;
  }

  pos = 4;

  // amount
  if (len < 1) goto loc_incomplete;
  if (*ptr == 0x22) {
    ptr++; len--;
    size = decode_varint(ptr, len, &str_len);
    if (size == 0 || len - size < str_len) goto loc_incomplete2;
    ptr += size;
    len -= size;
    if (str_len > 15) goto loc_invalid;
    txn.amount = ptr;
    ptr += str_len;
    len -= str_len;
  } else {
    txn.amount = (unsigned char *) "\0";
    str_len = 1;
  }

  sha256_add(txn.amount, str_len);

  encode_amount(txn.amount, str_len, amount_str, sizeof amount_str);

  pos = 5;

  // payload
  if (len < 1) goto loc_incomplete;
  if (*ptr == 0x2A) {
    ptr++; len--;
    size = decode_varint(ptr, len, &str_len);
    if (size == 0) goto loc_incomplete2;
    ptr += size;
    len -= size;
    if (str_len > 0x7FFFFFFF) goto loc_invalid;
    txn.payload = (char*) ptr;
    txn.payload_len = str_len;
    txn.payload_part_offset = 0;
    if (len < str_len) {
      has_partial_payload = true;
      txn.payload_part_len = len;
    } else {
      txn.payload_part_len = str_len;
    }
    ptr += txn.payload_part_len;
    len -= txn.payload_part_len;

    sha256_add(txn.payload, txn.payload_part_len);
    sha256_add_payload(txn.payload, txn.payload_part_len);
  }


  if (has_partial_payload) {
    return true;
  } else {
    return parse_last_part(ptr, len);
  }


loc_incomplete2:
  ptr--;
  len++;
loc_incomplete:
  THROW(0x6955);  // the transaction is incomplete

loc_invalid:
  THROW(0x6720 + pos);  // invalid data

}

static bool parse_payload_part(unsigned char *ptr, unsigned int len){

  txn.payload = (char*) ptr;

  txn.payload_part_offset += txn.payload_part_len;

  if (txn.payload_part_offset + len < txn.payload_len) {
    has_partial_payload = true;
    txn.payload_part_len = len;
  } else {
    has_partial_payload = false;
    txn.payload_part_len = txn.payload_len - txn.payload_part_offset;
  }

  sha256_add(txn.payload, txn.payload_part_len);
  sha256_add_payload(txn.payload, txn.payload_part_len);

  ptr += txn.payload_part_len;
  len -= txn.payload_part_len;

  if (has_partial_payload) {
    return true;
  } else {
    return parse_last_part(ptr, len);
  }

}

static bool parse_last_part(unsigned char *ptr, unsigned int len){
  unsigned int size;
  uint64_t str_len, val64;
  unsigned int pos;

  if (last_part_len + len > sizeof(last_part)) {
    THROW(0x6720 + 10);
  }

  if (last_part_pos == 0) {
    last_part_pos = 6;
  } else {
    memcpy(last_part+last_part_len, ptr, len);
    ptr = (unsigned char*) last_part;
    len += last_part_len;
  }

  switch (last_part_pos) {

  case 6:
  pos = 6;

  // gasLimit
  if (len < 1) goto loc_incomplete;
  if (*ptr == 0x30) {
    ptr++; len--;
    size = decode_varint(ptr, len, &txn.gasLimit);
    if (size == 0) goto loc_incomplete2;
    ptr += size;
    len -= size;
  } else {
    txn.gasLimit = 0;
  }

  sha256_add(&txn.gasLimit, 8);

  case 7:
  pos = 7;

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

  case 8:
  pos = 8;

  // type
  if (len < 1) goto loc_incomplete;
  if (*ptr == 0x40) {
    ptr++; len--;
    size = decode_varint(ptr, len, &val64);
    if (size == 0) goto loc_incomplete2;
    ptr += size;
    len -= size;
    txn.type = val64; /* convert it to 32-bit */
  } else {
    txn.type = 0;
  }
  if (txn.type != txn_type) goto loc_invalid;

  sha256_add(&txn.type, 4);

  case 9:
  pos = 9;

  // chain_id
  if (len < 1) goto loc_incomplete;
  if (*ptr != 0x4A) goto loc_invalid;
  ptr++; len--;
  size = decode_varint(ptr, len, &str_len);
  if (size == 0 || len - size < str_len) goto loc_incomplete2;
  ptr += size;
  //len -= size;
  if (str_len != 32) goto loc_invalid;
  txn.chainId = ptr;
  //ptr += str_len;
  //len -= str_len;

  sha256_add(txn.chainId, 32);

  }


  txn_is_complete = true;

  /* calculate the transaction hash */
  cx_hash(&hash.header, CX_LAST, NULL, 0, txn_hash, sizeof txn_hash);

  return true;

loc_incomplete2:
  ptr--;
  len++;
loc_incomplete:
  /* incomplete transaction */
  memcpy(last_part, ptr, len);
  last_part_len = len;
  last_part_pos = pos;
  return false;

loc_invalid:
  THROW(0x6720 + pos);  // invalid data

}

/*
** The JSON payload has these formats:
**
**  {"Name":"some_function","Args":[<parameters>]}
**  {"Name":"some_function"}
*/
static bool parse_payload(char **pfunction_name, char **pargs, unsigned int *pargs_len) {
  char *name, *args, last_char;
  unsigned int len, args_len;

  if (!txn.payload || txn.payload_len==0) goto loc_invalid;

  /* is the payload completely downloaded? */
  if (has_partial_payload) {
    len = txn.payload_part_len;
  } else {
    len = txn.payload_len;
  }

  txn.payload[len] = 0; /* null terminator used by strcmp and stripstr */

  if (strncmp(txn.payload, "{\"Name\":\"", 9) != 0) goto loc_invalid;
  name = txn.payload + 9;
  args = stripstr(name, "\",\"Args\":[");

  if (has_partial_payload) {
    args_len = txn.payload + len - args;
  } else {
    if (args) {
      last_char = ']';
    } else {
      last_char = '"';
    }
    if (txn.payload[txn.payload_len-1] != '}') goto loc_invalid;
    if (txn.payload[txn.payload_len-2] != last_char) goto loc_invalid;
    txn.payload[txn.payload_len-2] = 0;
    if (args) {
      args_len = strlen(args);
    } else {
      args_len = 0;
    }
  }

  *pfunction_name = name;
  *pargs = args;
  *pargs_len = args_len;

  return true;
loc_invalid:
  return false;
}

static void parse_transaction_part(unsigned char *buf, unsigned int len, bool is_first, bool is_last){

  /* check the minimum transaction size */
  if (is_first && len < 60) {
    THROW(0x6700);  // wrong length
  }

  if (!is_first && txn_is_complete) {
    THROW(0x6985);  // invalid state
  }

  is_first_part = is_first;
  is_last_part = is_last;

  if (is_first) {
    parse_first_part(buf, len);
  } else if (has_partial_payload) {
    parse_payload_part(buf, len);
  } else {
    parse_last_part(buf, len);
  }

  if (is_last && !txn_is_complete) {
    THROW(0x6740);  // invalid data
  }

}
