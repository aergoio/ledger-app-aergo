
/*
** This lookup table is used to help decode the first byte of
** a multi-byte UTF8 character.
*/
static const unsigned char UTF8Trans1[] = {
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
  0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
  0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x00, 0x01, 0x02, 0x03, 0x00, 0x01, 0x00, 0x00,
};

#define READ_UTF8(zIn, zEnd, c)                            \
  c = *(zIn++);                                            \
  is_utf8 = (c >= 0xc0);                                   \
  if( is_utf8 ){                                           \
    c = UTF8Trans1[c-0xc0];                                \
    while( zIn!=zEnd && (*zIn & 0xc0)==0x80 ){             \
      c = (c<<6) + (0x3f & *(zIn++));                      \
    }                                                      \
    if( c<0x80                                             \
        || (c&0xFFFFF800)==0xD800                          \
        || (c&0xFFFFFFFE)==0xFFFE ){ c = 0xFFFD; }         \
  }

#define READ_REMAINING_UTF8(zIn, zEnd, c)                  \
  while( zIn!=zEnd && (*zIn & 0xc0)==0x80 ){               \
    c = (c<<6) + (0x3f & *(zIn++));                        \
  }                                                        \
  if( c<0x80                                               \
      || (c&0xFFFFF800)==0xD800                            \
      || (c&0xFFFFFFFE)==0xFFFE ){ c = 0xFFFD; }           \

/*
** Array for converting from half-bytes (nybbles) into ASCII hex
** digits.
*/
static const char hexdigits[] = {
  '0', '1', '2', '3', '4', '5', '6', '7',
  '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
};
