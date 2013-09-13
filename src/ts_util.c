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

#include "config.h"

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
  time_t now;
  const char *fcomment = "TARIX COMPRESSED v" TARIX_FORMAT_STRING;
  int nbytes;
  
  /* magic */
  obuf[0] = 0x1f;
  obuf[1] = 0x8b;
  /* compression method = deflate */
  obuf[2] = 8;
  /* flags: FCOMMENT */
  /* note: we don't include a filename yet, see TODO above */
  obuf[3] = (1 << 4);
  /* mtime, force lsb */
  now = time(NULL);
  lsb_buf(obuf + 4, now);
  /* XFL: might need to revisit this in case user tweaked compression */
  obuf[8] = 0;
  /* OS: unix */
  obuf[9] = 3;
  /* magic comment -- strlen + 1 bytes */
  /* cast to remove signedness warning */
  strcpy((char*)obuf + 10, fcomment);
  /* update zsp */
  nbytes = 10 + strlen(fcomment) + 1;
  tsp->zsp->next_out += nbytes;
  tsp->zsp->avail_out -= nbytes;
}

int put_gz_footer(t_streamp tsp) {
  Bytef obuf[GZ_FOOTER_LEN];
  int wr;
  
  lsb_buf(obuf, tsp->crc32);
  lsb_buf(obuf + 4, (unsigned long)(tsp->raw_bytes & 0xffffffff));
  
  wr = write(tsp->fd, obuf, 8);
  if (wr > 0)
    tsp->zlib_bytes += wr;
  return wr;
}

int process_deflate(t_streamp tsp, int flush) {
  z_streamp zsp = tsp->zsp;
  int inbytes_used, ready2write;

  /* save old pointer for crc calcs */
  Bytef *old_ni = zsp->next_in;

  tsp->zlib_err = deflate(zsp, flush);
  
  inbytes_used = zsp->next_in - old_ni;
  
  if (inbytes_used > 0) {
    /* update crc32 and raw_bytes */
    tsp->crc32 = update_crc(tsp->crc32, old_ni, inbytes_used);
    tsp->raw_bytes += inbytes_used;
    /* reconfigure the input buffer */
    memmove(tsp->inbuf, zsp->next_in, zsp->avail_in);
    zsp->next_in = tsp->inbuf;
  }
  
  /* amount ready to write is the buffer size minus the amount unused */
  ready2write = tsp->bufsz - zsp->avail_out;
  
  if (ready2write == 0)
    return 0;
  
  /* flush buffer if it's near full, or it was requested */
  if (zsp->avail_out < tsp->blksz || flush != Z_NO_FLUSH) {
    int ewrite = ready2write;
    int nwrite;
    /* if no specific flush requested, make ewrite an even multiple of the
     * block size */
    if (flush == Z_NO_FLUSH)
      ewrite -= ewrite % tsp->blksz;
    nwrite = write(tsp->fd, tsp->outbuf, ewrite);
    if (nwrite != ewrite)
      perror(nwrite >= 0 ? "partial block write" : "write block");
    if (nwrite > 0) {
      /* shift out the now flushed part of the buffer.  under some common
       * circumstances, this will become a no-op, as the amount to move will
       * be zero.  it's important that the amounts be based on ready2write,
       * not ewrite, since we need to move all bytes that are used but not
       * written */
      memmove(tsp->outbuf, tsp->outbuf + nwrite, ready2write - nwrite);
      zsp->next_out = tsp->outbuf + ready2write - nwrite;
      zsp->avail_out += nwrite;
      tsp->zlib_bytes += nwrite;
    }
    return nwrite;
  } else {
    return 0;
  }
}

int process_inflate(t_streamp tsp, int flush) {
  z_streamp zsp = tsp->zsp;
  int nread = 0, inbytes_used, obytes_gend;
  Bytef *old_ni, *old_no;
  
  /* fill input buffer if it's empty */
  if (zsp->avail_in == 0) {
    /* how many bytes can we read? */
    int eread = tsp->bufsz - zsp->avail_in;
    /* make it a multiple of the block size */
    eread -= eread % tsp->blksz;
    nread = read(tsp->fd, zsp->next_in + zsp->avail_in, eread);
    if (nread < 0) {
      perror("read block");
      return nread;
    } else {
      zsp->avail_in += nread;
    }
  }

  /* save old pointer for crc calcs */
  old_ni = zsp->next_in;
  old_no = zsp->next_out;

  tsp->zlib_err = inflate(zsp, flush);
  
  inbytes_used = zsp->next_in - old_ni;
  obytes_gend = zsp->next_out - old_no;
  /* update crc32 and byte counters */
  tsp->crc32 = update_crc(tsp->crc32, old_no, obytes_gend);
  tsp->zlib_bytes += inbytes_used;
  tsp->raw_bytes += obytes_gend;
  
  /* reconfigure the input buffer */
  if (inbytes_used > 0) {
    memmove(tsp->inbuf, zsp->next_in, zsp->avail_in);
    zsp->next_in = tsp->inbuf;
  }
  
  return nread;
}

int read_gz_header(t_streamp tsp) {
  const char *signature_start = "TARIX COMPRESSED v";
  int p = 0;
  char c;
  Bytef buf[10];

  int nread = read(tsp->fd, buf, 10);
  if (nread < 0)
    return nread;
  if (nread != 10)
    return 1;
  
  /* check gzip magic */
  if (buf[0] != 0x1f || buf[1] != 0x8b)
    return 1;
  /* check gzip settings: deflate, FCOMMENT */
  if (buf[2] != 8 || buf[3] != (1 << 4))
    return 1;
  
  // check comment magic: start sequence
  while (p < strlen(signature_start)) {
    if (read(tsp->fd, &c, 1) != 1)
      return -1;
    if (c != signature_start[p])
      return 1;
    ++p;
  }
  // comment magic: version character (just one for now)
  if (read(tsp->fd, &c, 1) != 1)
    return -1;
  // zlib archives can have v1 through vCURRENT inclusive
  if (c - '0' < 1 || c - '0' > TARIX_FORMAT_VERSION)
    return 1;
  // comment magic: null terminator
  if (read(tsp->fd, &c, 1) != 1)
    return -1;
  if (c != 0)
    return 1;
  
  return 0;
}
