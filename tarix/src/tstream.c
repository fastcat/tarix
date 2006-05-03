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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "portability.h"
#include "tstream.h"
#include "ts_util.h"
#include "tar.h"

/* TODO: pass filename for inclusion in gzip header */

/* common tsp init */
static void init_ts_buffers(t_streamp tsp) {
  if (tsp->bufsz <= 0)
    tsp->bufsz = TS_BUFSZ;
  if (tsp->inbuf == NULL)
    tsp->inbuf = (Bytef*)malloc(tsp->bufsz);
  if (tsp->outbuf == NULL)
    tsp->outbuf = (Bytef*)malloc(tsp->bufsz);
    /* put our buffer info into it */
    tsp->zsp->next_in = tsp->inbuf;
    tsp->zsp->avail_in = 0;
    tsp->zsp->next_out = tsp->outbuf;
    tsp->zsp->avail_out = tsp->bufsz;
}

static int do_seek(t_streamp tsp, off64_t offset) {
  offset += tsp->baseoffset;
  if (tsp->usemt) {
    int tmp = p_mt_setpos(tsp->fd, offset / tsp->blksz);
    int remainder = offset % tsp->blksz;
    if (tmp < 0)
      return tmp;
    /* consume partial block if necessary */
    if (remainder != 0) {
      Bytef buf[TARBLKSZ];
      while (remainder > 0) {
        int nread = TARBLKSZ;
        if (nread > remainder)
          nread = remainder;
        if (read(tsp->fd, buf, nread) != nread)
          return -1;
        remainder -= nread;
      }
    }
    return 0;
  } else {
    off64_t lsret = p_lseek64(tsp->fd, offset, SEEK_SET);
    return lsret < 0 ? -1 : 0;
  }
}

static off64_t do_tell(t_streamp tsp) {
  off64_t pos;
  if (tsp->usemt) {
    if (p_mt_getpos(tsp->fd, &pos) != 0)
      return -1;
    return pos * tsp->blksz;
  } else {
    pos = p_lseek64(tsp->fd, 0, SEEK_CUR);
    return pos;
  }
}

/* common tsp init */
static t_streamp init_ts(t_streamp tsp, int fd, int usemt, int blksz,
    int zlib_level) {
  /* allocate the stream data */
  if (tsp == NULL)
    tsp = (t_streamp)malloc(sizeof(t_stream));

  /* initialize fields */
  tsp->fd = fd;
  tsp->bufsz = 0;
  tsp->inbuf = NULL;
  tsp->outbuf = NULL;
  tsp->zlib_err = Z_OK;
  tsp->zsp = NULL;
  tsp->crc32 = 0;
  tsp->raw_bytes = 0;
  tsp->zlib_bytes = 0;
  tsp->usemt = usemt;
  tsp->blksz = blksz <= 0 ? TARBLKSZ : blksz;
  
  if (tsp->usemt) {
    int tmp = p_mt_setblk(tsp->fd, tsp->blksz);
    if (tmp < 0) {
      tsp->zlib_err = Z_STREAM_ERROR;
      /* don't necessarily have zsp */
      /* tsp->zsp->msg = "setblk error"; */
      return tsp;
    }
    tsp->baseoffset = do_tell(tsp);
    if (tsp->baseoffset < 0) {
      tsp->zlib_err = Z_STREAM_ERROR;
      tsp->zsp->msg = "get base offset error";
    }
  } else {
    tsp->baseoffset = 0;
  }
  if (tsp->baseoffset < 0) {
    tsp->zlib_err = Z_STREAM_ERROR;
    return tsp;
  }
  
  if (zlib_level > 0) {
    /* create the zlib stream */
    tsp->zsp = (z_streamp)malloc(sizeof(z_stream));
    tsp->zsp->zalloc = NULL;
    tsp->zsp->zfree = NULL;
    tsp->zsp->opaque = NULL;
    
    init_ts_buffers(tsp);
  }
  
  return tsp;
}

t_streamp init_tws(t_streamp tsp, int fd, int usemt, int blksz,
    int zlib_level) {
  /* common init, includes part of zlib */
  tsp = init_ts(tsp, fd, usemt, blksz, zlib_level);
  tsp->mode = TS_WRITE;

  /* if zlib asked for, set it up */
  if (zlib_level > 0) {
  
    /* negative window bits suppress zlib wrapper */
    tsp->zlib_err = deflateInit2(tsp->zsp, zlib_level, Z_DEFLATED, -MAX_WBITS,
      MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY);
    if (tsp->zlib_err != Z_OK)
      return tsp;
    
    /* add the gzip header to the stream */
    put_gz_header(tsp);
  }
  
  return tsp;
}

t_streamp init_trs(t_streamp tsp, int fd, int usemt, int blksz,
    int zlib_level) {
  /* common init, includes part of zlib */
  tsp = init_ts(tsp, fd, usemt, blksz, zlib_level);
  tsp->mode = TS_READ;

  /* if zlib asked for, set it up */
  if (zlib_level > 0) {
     int gzhrv;
  
    /* negative window bits suppress zlib wrapper */
    tsp->zlib_err = inflateInit2(tsp->zsp, -MAX_WBITS);
    if (tsp->zlib_err != Z_OK)
      return tsp;
    
    /* read the gzip header from the stream */
    gzhrv = read_gz_header(tsp);
    if (gzhrv != 0)
      tsp->zlib_err = Z_VERSION_ERROR;
  }
  
  return tsp;
}

int ts_write(t_streamp tsp, void *buf, int len) {
  int left; /* how many bytes in buf left to process? */
  Bytef *cur; /* pointer to next byte to process in buf */
  z_streamp zsp;
  
  /* sanity check */
  if (tsp == NULL || tsp->mode != TS_WRITE)
    return TS_ERR_BADMODE;
  
  /* short circuit */
  if (len == 0)
    return 0;
  
  if (tsp->zsp == NULL) {
    /* non-zlib is just a simple write through */
    return write(tsp->fd, buf, len);
  }
  
  /* dump into zlib buffers */
  
  /* start write processing at the beginning of the buffer */
  left = len;
  cur = buf;
  zsp = tsp->zsp;
  
  while (left > 0) {
    int nwrite;
    
    /* if there is room in the input buffer, put some data in */
    if (zsp->avail_in < tsp->bufsz) {
      /* how many bytes to stick in the input buffer */
      int toadd = tsp->bufsz - zsp->avail_in;
      if (toadd > left)
        toadd = left;
      
      /* copy our bytes to the end of the input buffer */
      memcpy(zsp->next_in + zsp->avail_in, cur, toadd);
      zsp->avail_in += toadd;
      cur += toadd;
      left -= toadd;
    }
    
    /* run a deflate cycle */
    nwrite = process_deflate(tsp, Z_NO_FLUSH);
    if (nwrite < 0)
      return nwrite;
    if (tsp->zlib_err != Z_OK)
      return TS_ERR_ZLIB;
  } /* while left < len */
  
  return len;
}

int ts_read(t_streamp tsp, void *buf, int len) {
  z_streamp zsp;
  int left;
  void *cur;
  
  if (tsp == NULL || tsp->mode != TS_READ)
    return TS_ERR_BADMODE;
  
  zsp = tsp->zsp;
  
  /* non-zlib: straight read */
  if (zsp == NULL)
    return read(tsp->fd, buf, len);
  
  /* zlib read */
  left = len;
  cur = buf;
  
  while (left > 0) {
    /* run an inflate cycle, flush as much to output buffer as possible */
    int nread = process_inflate(tsp, Z_SYNC_FLUSH);
    if (nread < 0)
      return nread;
    if (tsp->zlib_err != Z_OK && tsp->zlib_err != Z_STREAM_END)
      return TS_ERR_ZLIB;
    
    /* if there is anything available in the zlib output buffer */
    if (zsp->avail_out < tsp->bufsz) {
      /* the available data is the buffer size minus the unused part */
      int bytesavail = tsp->bufsz - zsp->avail_out;
      int toadd = bytesavail;
      if (toadd > left)
        toadd = left;
      
      /* copy data to the requested buffer */
      memcpy(cur, tsp->outbuf, toadd);
      /* shift out the part we've used up */
      memmove(tsp->outbuf, tsp->outbuf + toadd, bytesavail - toadd);
      zsp->avail_out += toadd;
      zsp->next_out = tsp->outbuf + (tsp->bufsz - zsp->avail_out);
      left -= toadd;
    }
    
    if (tsp->zlib_err == Z_STREAM_END) {
      /* if we're at the end of the stream, bail */
      break;
    }
  } /* while tread < len */
  
  return len - left;
}

off64_t ts_checkpoint(t_streamp tsp) {
  z_streamp zsp;
  
  if (tsp == NULL || tsp->mode != TS_WRITE)
    return TS_ERR_BADMODE;
  
  zsp = tsp->zsp;
  
  /* non-zlib: return basic offset */
  if (zsp == NULL)
    return tsp->raw_bytes;
  
  /* do a zlib checkpoint */
  while (1) {
    int pdr = process_deflate(tsp, Z_FULL_FLUSH);
    if (pdr < 0)
      return -1;
    if (tsp->zlib_err == Z_BUF_ERROR) {
      /* no progress was possible, therefore we are done */
      break;
    } else if (tsp->zlib_err != Z_OK) {
      /* some error occurred */
      return TS_ERR_ZLIB;
    }
  }
  
  return tsp->zlib_bytes;
}

int ts_seek(t_streamp tsp, off64_t offset) {
  z_streamp zsp;
  
  if (tsp == NULL || tsp->mode != TS_READ)
    return TS_ERR_BADMODE;
  
  /* we could use inflateSync to ensure a proper stream state, but
   * inspection of the zlib code for that shows that it just searches for a
   * sync point and then resets the stream.  Since we know where the sync
   * point is, we just need to reset the stream.
   */
  
  /* TODO: check for the sync point magic right before the offset? */
  
  zsp = tsp->zsp;
  if (zsp != NULL) {
    /* reset buffers */
    init_ts_buffers(tsp);
    /* reset zlib */
    tsp->zlib_err = inflateReset(zsp);
  }
  
  /* actual stream seek, both for zlib and non-zlib */
  if (do_seek(tsp, offset) != 0)
    return -1;
  
  /* next read will refill buffers */
  
  return 0;
}

int ts_close(t_streamp tsp, int dofree) {
  
  /* check state */
  if (tsp == NULL)
    return 0;
  
  if (tsp->mode == TS_READ) {
    if (tsp->zsp != NULL) {
      tsp->zlib_err = inflateEnd(tsp->zsp);
    }
    
    tsp->mode = TS_CLOSED;
    
    if (dofree)
      free(tsp);
    
    return 0;
  } else if (tsp->mode == TS_WRITE) {
    int ret = 0;
    int ngf;
    
    if (tsp->zsp != NULL) {
      /* flush zlib */
      while (1) {
        int nwrite = process_deflate(tsp, Z_FINISH);
        if (nwrite < 0) {
          ret = -1;
          break;
        }
        if (tsp->zlib_err == Z_STREAM_END) {
          /* all done flushing zlib */
          break;
        }
        /* otherwise loop again and flush some more */
      } /* while not flushed */
      
      tsp->zlib_err = deflateEnd(tsp->zsp);
      
      ngf = put_gz_footer(tsp);
      if (ngf != GZ_FOOTER_LEN)
        return -1;
      
    } /* if zsp != NULL */
    
    tsp->mode = TS_CLOSED;
    
    if (dofree)
      free(tsp);
    
    return ret;
  } else {
    return TS_ERR_BADMODE;
  }
}

void ptserror(const char *msg, off64_t rv, t_streamp tsp) {
  if (rv == TS_ERR_ZLIB) {
    fprintf(stderr, "%s: zlib error: %d: %s\n", msg, tsp->zlib_err,
      tsp->zsp->msg);
  } else if (rv == TS_ERR_BADMODE) {
    fprintf(stderr, "%s: invalid tstream mode\n", msg);
  } else if (rv == -1) {
    fprintf(stderr, "%s: zlib i/o: %s\n", msg, strerror(errno));
  } else {
    fprintf(stderr, "unknown error: %lld\n", (long long)rv);
  }
}
