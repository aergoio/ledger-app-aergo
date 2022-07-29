#include <stdbool.h>


#include <setjmp.h>
jmp_buf jump_buffer;
#define THROW(x) longjmp(jump_buffer, x)


#define MAX_TX_PART 200


int  cmd_type;
bool is_signing;
bool is_first_part;
bool is_last_part;
bool txn_is_complete;
bool has_partial_payload;


#include "sha256.h"
typedef int cx_sha256_t;
//#define cx_sha256_t SHA256_CTX
#define cx_sha256_init(...)
#define cx_hash(...)
#define CX_LAST  111

static cx_sha256_t hash;
static cx_sha256_t hash2;
static cx_sha256_t hash3;
unsigned char txn_hash[32];
unsigned char payload_hash[32];

