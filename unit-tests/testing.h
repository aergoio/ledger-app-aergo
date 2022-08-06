#include <stdbool.h>


#include <setjmp.h>
jmp_buf jump_buffer;
#define THROW(x) longjmp(jump_buffer, x)


#define MAX_TX_PART 200


#include "sha256.h"
#define cx_sha256_t SHA256_CTX
#define sha256_init(ctx) sha256_initialize(&ctx)
#define sha256_add(ctx,ptr,len) sha256_update(&ctx,(const uchar*)ptr,len)
#define sha256_finish(ctx,hash) sha256_final(&ctx,hash)


#define strlcpy strncpy
