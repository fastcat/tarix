/* crc32 code from RFC-1952 */

/* Make the table for a fast CRC. */
void make_crc_table(void);

/* Update a running crc with the bytes buf[0..len-1] and return the updated
 * crc. The crc should be initialized to zero. Pre- and post-conditioning
 * (one's complement) is performed within this function so it shouldn't be
 * done by the caller. Usage example:
 *
 * unsigned long crc = 0L;
 *
 * while (read_buffer(buffer, length) != EOF) {
 *   crc = update_crc(crc, buffer, length);
 * }
 *
 * if (crc != original_crc) error();
 */
unsigned long update_crc(unsigned long crc, unsigned char *buf, int len);

/* Return the CRC of the bytes buf[0..len-1]. */
unsigned long crc(unsigned char *buf, int len);
