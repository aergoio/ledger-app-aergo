#include <stdbool.h>


#include <setjmp.h>
jmp_buf jump_buffer;
#define THROW(x) longjmp(jump_buffer, x)


#define MAX_TX_PART 200


#include "sha256.h"
typedef int cx_sha256_t;
#define sha256_init(...)
#define sha256_add(...)
#define sha256_finish(...)


#define strlcpy strncpy
