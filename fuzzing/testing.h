#include <stdbool.h>


#include <setjmp.h>
jmp_buf jump_buffer;
#define THROW(x) longjmp(jump_buffer, x)


#define MAX_TX_PART 200


#include "sha256.h"
typedef int cx_sha256_t;
//#define cx_sha256_t SHA256_CTX
#define cx_sha256_init(...)
#define cx_hash(...)
#define CX_LAST  111
