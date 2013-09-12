/*
 *  tarix - a GNU/POSIX tar indexer
 *  Copyright (C) 2006 Matthew "Cheetah" Gabeler-Lee
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __CRC32_H__
#define __CRC32_H__

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

#endif /* __CRC32_H__ */
