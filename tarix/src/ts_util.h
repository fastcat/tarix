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

/* Stick the gzip header into the stream's output buffers.
 * Format as defined in RFc-1952
 */
void put_gz_header(t_streamp tsp);

/* Write the gzip footer direct to the output fd */
int put_gz_footer(t_streamp tsp);

/* Read the gzip header, check for magic.  Returns 0 on sucess, -1 on i/o
 * error, 1 on magic problems */
int read_gz_header(t_streamp tsp);

#define GZ_FOOTER_LEN 8

/* Internal function to handle an iteration of calling deflate on the zlib
 * stream.  Will write the output buffer to the file descriptor and reset it
 * if it fills up or if flush != Z_NO_FLUSH.  Also keeps track of crc32 and
 * byte counts.
 */
int process_deflate(t_streamp tsp, int flush);

/* Internal function to handle an iteration of calling inflate on the zlib
 * stream.  Will read the input buffer from the file descriptor as needed. 
 * Also keeps track of crc32 and byte counts.
 */
int process_inflate(t_streamp tsp, int flush);
