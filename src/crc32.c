/* crc32 code from RFC-1952 */

/* Table of CRCs of all 8-bit messages. */
unsigned long crc_table[256];

/* Flag: has the table been computed? Initially false. */
int crc_table_computed = 0;

void make_crc_table(void) {
  unsigned long c;

  int n, k;
  for (n = 0; n < 256; n++) {
    c = (unsigned long) n;
    for (k = 0; k < 8; k++) {
      if (c & 1) {
        c = 0xedb88320L ^ (c >> 1);
      } else {
        c = c >> 1;
      }
    }
    crc_table[n] = c;
  }
  crc_table_computed = 1;
}

unsigned long update_crc(unsigned long crc, unsigned char *buf, int len) {
  unsigned long c = crc ^ 0xffffffffL;
  int n;

  if (!crc_table_computed)
    make_crc_table();

  for (n = 0; n < len; n++)
    c = crc_table[(c ^ buf[n]) & 0xff] ^ (c >> 8);

  return c ^ 0xffffffffL;
}

unsigned long crc(unsigned char *buf, int len) {
  return update_crc(0L, buf, len);
}
