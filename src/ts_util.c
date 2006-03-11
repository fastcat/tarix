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

/* utility funcs for tstream */

#include <zlib.h>
#include <time.h>
#include <string.h>
#include <stdio.h>

#include "tarix.h"
#include "tstream.h"
#include "ts_util.h"
#include "crc32.h"

static void lsb_buf(Bytef *buf, unsigned long int32) {
  buf[0] = int32 & 0xff;
  buf[1] = (int32 >> 8) & 0xff;
  buf[2] = (int32 >> 16) & 0xff;
  buf[3] = (int32 >> 24) & 0xff;
}

void put_gz_header(t_streamp tsp) {
  Bytef *obuf = tsp->zsp->next_out;
  
  /* magic */
  obuf[0] = 0x1f;
  obuf[1] = 0x8b;
  /* compression method = deflate */
  obuf[2] = 8;
  /* flags: FCOMMENT */
  /* note: we don't include a filename yet, see TODO above */
  obuf[3] = (1 << 4);
  /* mtime, force lsb */
  time_t now = time(NULL);
  lsb_buf(obuf + 4, now);
  /* XFL: might need to revisit this in case user tweaked compression */
  obuf[8] = 0;
  /* OS: unix */
  obuf[9] = 3;
  /* magic comment -- strlen + 1 bytes */
  const char *fcomment = "TARIX COMPRESSED v" TARIX_FMT_VERSION_NEW;
  /* cast to remove signedness warning */
  strcpy((char*)obuf + 10, fcomment);
  /* update zsp */
  int nbytes = 10 + strlen(fcomment) + 1;
  tsp->zsp->next_out += nbytes;
  tsp->zsp->avail_out -= nbytes;
}

int put_gz_footer(t_streamp tsp) {
  Bytef obuf[GZ_FOOTER_LEN];
  lsb_buf(obuf, tsp->crc32);
  lsb_buf(obuf + 4, (unsigned long)(tsp->total_bytes & 0xffffffff));
  return write(tsp->fd, obuf, 8);
}

int process_deflate(t_streamp tsp, int flush) {
  z_streamp zsp = tsp->zsp;

  /* save old pointer for crc calcs */
  Bytef *old_ni = zsp->next_in;

  tsp->zlib_err = deflate(zsp, flush);
  
  /* update crc32 and total_bytes */
  int inbytes_used = zsp->next_in - old_ni;
  tsp->crc32 = update_crc(tsp->crc32, old_ni, inbytes_used);
  tsp->total_bytes += inbytes_used;
  
  if (zsp->avail_out == 0 || flush != Z_NO_FLUSH) {
    /* flush the buffer to output fd */
    int ewrite = tsp->bufsz - zsp->avail_out;
    int nwrite = write(tsp->fd, tsp->outbuf, ewrite);
    if (nwrite != ewrite)
      perror(nwrite >= 0 ? "partial block write" : "write block");
    if (nwrite > 0) {
      /* if nwrite == ewrite, this will no-op out and the next_out
       * assignment will become next_out = outbuf
       */
      memmove(tsp->outbuf, tsp->outbuf + nwrite, ewrite - nwrite);
      zsp->next_out = tsp->outbuf + ewrite - nwrite;
      zsp->avail_out += nwrite;
    }
    return nwrite;
  } else {
    return 0;
  }
}
