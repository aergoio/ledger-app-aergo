
bool sha256(void *hash, const void *data, size_t len) {
  static cx_sha256_t ctx;
  cx_sha256_init(&ctx);
  cx_hash(&ctx.header, 0, (unsigned char*) data, len, NULL, 0);
  cx_hash(&ctx.header, CX_LAST, NULL, 0, hash, 32);
  return true;
}

#define sha256_init(ctx) cx_sha256_init(&ctx)
#define sha256_add(ctx,ptr,len) cx_hash(&ctx.header, 0, (unsigned char*)ptr, len, NULL, 0)
#define sha256_finish(ctx,hash) cx_hash(&ctx.header, CX_LAST, NULL, 0, hash, 32)
