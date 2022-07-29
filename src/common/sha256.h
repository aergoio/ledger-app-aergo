
bool sha256(void *hash, const void *data, size_t len) {
  static cx_sha256_t ctx;
  cx_sha256_init(&ctx);
  cx_hash(&ctx.header, 0, (unsigned char*) data, len, NULL, 0);
  cx_hash(&ctx.header, CX_LAST, NULL, 0, hash, 32);
  return true;
}
