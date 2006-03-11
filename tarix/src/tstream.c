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

#include "tstream.h"
#include "ts_util.h"

#include <stdlib.h>
#include <string.h>
/*DEBUG*/
#include <stdio.h>

/* TODO: pass filename for inclusion in gzip header */

/* common tsp init */
static t_streamp init_ts(t_streamp tsp, int fd, int zlib_level) {
  /* allocate the stream data */
  if (tsp == NULL)
    tsp = (t_streamp)malloc(sizeof(t_stream));

  /* initialize fields */
  tsp->fd = fd;
  tsp->bufsz = zlib_level > 0 ? TS_BUFSZ : 0;
  tsp->inbuf = zlib_level > 0 ? (Bytef*)malloc(tsp->bufsz) : NULL;
  tsp->outbuf = zlib_level > 0 ? (Bytef*)malloc(tsp->bufsz) : NULL;
  tsp->zlib_err = Z_OK;
  tsp->zsp = NULL;
  tsp->crc32 = 0;
  tsp->raw_bytes = 0;
  tsp->zlib_bytes = 0;
  
  if (zlib_level > 0) {
    /* create the zlib stream */
    tsp->zsp = (z_streamp)malloc(sizeof(z_stream));
    tsp->zsp->zalloc = NULL;
    tsp->zsp->zfree = NULL;
    tsp->zsp->opaque = NULL;
  }
  
  return tsp;
}

t_streamp init_tws(t_streamp tsp, int fd, int zlib_level) {
  
  /* common init, includes part of zlib */
  tsp = init_ts(tsp, fd, zlib_level);
  tsp->mode = TS_WRITE;

  /* if zlib asked for, set it up */
  if (zlib_level > 0) {
  
    /* negative window bits suppress zlib wrapper */
    tsp->zlib_err = deflateInit2(tsp->zsp, zlib_level, Z_DEFLATED, -MAX_WBITS,
      MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY);
    if (tsp->zlib_err != Z_OK)
      return tsp;
    
    /* put our buffer info into it */
    tsp->zsp->next_in = tsp->inbuf;
    tsp->zsp->avail_in = 0;
    tsp->zsp->next_out = tsp->outbuf;
    tsp->zsp->avail_out = tsp->bufsz;
    
    /* add the gzip header to the stream */
    put_gz_header(tsp);
  }
  
  return tsp;
}

t_streamp init_trs(t_streamp tsp, int fd, int zlib_level) {
  
  /* common init, includes part of zlib */
  tsp = init_ts(tsp, fd, zlib_level);
  tsp->mode = TS_READ;

  /* if zlib asked for, set it up */
  if (zlib_level > 0) {
  
    /* negative window bits suppress zlib wrapper */
    tsp->zlib_err = inflateInit2(tsp->zsp, -MAX_WBITS);
    if (tsp->zlib_err != Z_OK)
      return tsp;
    
    /* put our buffer info into it */
    tsp->zsp->next_in = tsp->inbuf;
    tsp->zsp->avail_in = 0;
    tsp->zsp->next_out = tsp->outbuf;
    tsp->zsp->avail_out = tsp->bufsz;
    
    /* read the gzip header from the stream */
    int gzhrv = read_gz_header(tsp);
    if (gzhrv != 0)
      tsp->zlib_err = Z_VERSION_ERROR;
  }
  
  return tsp;
}

int ts_write(t_streamp tsp, void *buf, int len) {
  
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
  
  int left = len;
  Bytef *cur = buf;
  z_streamp zsp = tsp->zsp;
  
  while (left > 0) {
    /* if there is room in the input buffer, put some data in */
    if (zsp->avail_in < tsp->bufsz) {
      /* how many bytes to stick in the input buffer */
      int toadd = tsp->bufsz - zsp->avail_in;
      if (toadd > left)
        toadd = left;
      
      memcpy(zsp->next_in, cur, toadd);
      zsp->avail_in += toadd;
      cur += toadd;
      left -= toadd;
    }
    
    /* run a deflate cycle */
    int nwrite = process_deflate(tsp, Z_NO_FLUSH);
    if (nwrite < 0)
      return nwrite;
    if (tsp->zlib_err != Z_OK)
      return TS_ERR_ZLIB;
  } /* while left < len */
  
  return len;
}

int ts_read(t_streamp tsp, void *buf, int len) {
  if (tsp == NULL || tsp->mode != TS_READ)
    return TS_ERR_BADMODE;
  
  z_streamp zsp = tsp->zsp;
  
  /* non-zlib: straight read */
  if (zsp == NULL)
    return read(tsp->fd, buf, len);
  
  /* zlib read */
  int left = len;
  void *cur = buf;
  
  while (left > 0) {
    /* run an inflate cycle */
    int nread = process_inflate(tsp, Z_NO_FLUSH);
    if (nread < 0)
      return nread;
    if (tsp->zlib_err != Z_OK && tsp->zlib_err != Z_STREAM_END)
      return TS_ERR_ZLIB;
    
    /* pull data from the zlib output buffer into our buffer */
    if (zsp->avail_out < tsp->bufsz) {
      int toadd = tsp->bufsz - zsp->avail_out;
      if (toadd > left)
        toadd = left;
      
      memcpy(cur, tsp->outbuf, toadd);
      zsp->avail_out += toadd;
      zsp->next_out -= toadd;
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
  
  if (tsp == NULL || tsp->mode != TS_WRITE)
    return TS_ERR_BADMODE;
  
  z_streamp zsp = tsp->zsp;
  
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

/************ TODO: ts_seek **********/

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
      
      int ngf = put_gz_footer(tsp);
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
