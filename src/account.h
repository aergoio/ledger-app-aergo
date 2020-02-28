
bool (*b58_sha256_impl)(void *, const void *, size_t) = NULL;

static bool double_sha256(void *hash, const void *data, size_t datasz) {
  uint8_t buf[0x20];
  return b58_sha256_impl(buf, data, datasz) && b58_sha256_impl(hash, buf, sizeof(buf));
}

static const char b58digits_ordered[] = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

bool base58_encode(char *b58, size_t *b58sz, const void *data, size_t binsz) {
  const uint8_t *bin = (const uint8_t *) data;
  int carry;
  unsigned int i, j, high, zcount = 0;
  size_t size;

  while (zcount < binsz && !bin[zcount])
    ++zcount;

  size = (binsz - zcount) * 138 / 100 + 1;
  uint8_t buf[size];
  memset(buf, 0, size);

  for (i = zcount, high = size - 1; i < binsz; ++i, high = j) {
    for (carry = bin[i], j = size - 1; (j > high) || carry; --j) {
      carry += 256 * buf[j];
      buf[j] = carry % 58;
      carry /= 58;
    }
  }

  for (j = 0; j < size && !buf[j]; ++j);

  if (*b58sz <= zcount + size - j) {
    *b58sz = zcount + size - j + 1;
    return false;
  }

  if (zcount)
    memset(b58, '1', zcount);
  for (i = zcount; j < size; ++i, ++j)
    b58[i] = b58digits_ordered[buf[j]];
  b58[i] = '\0';
  *b58sz = i + 1;

  return true;
}

bool base58check_encode(char *b58c, size_t *b58c_sz, uint8_t ver, const void *data, size_t datasz){
  uint8_t buf[1 + datasz + 0x20];
  uint8_t *hash = &buf[1 + datasz];

  buf[0] = ver;
  memcpy(&buf[1], data, datasz);
  if (!double_sha256(hash, buf, datasz + 1)) {
    *b58c_sz = 0;
    return false;
  }

  return base58_encode(b58c, b58c_sz, buf, 1 + datasz + 4);
}

/******************************************************************************/

bool sha256(void *hash, const void *data, size_t len) {
  static cx_sha256_t ctx;
  cx_sha256_init(&ctx);
  cx_hash(&ctx.header, 0, (unsigned char*) data, len, NULL, 0);
  cx_hash(&ctx.header, CX_LAST, NULL, 0, hash, 32);
  return true;
}

bool encode_account(const void *data, size_t datasize, char *out, size_t outsize){
  b58_sha256_impl = sha256;
  return base58check_encode(out, &outsize, AddressVersion, data, datasize);
}
