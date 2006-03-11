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

/* TODO: pass filename for inclusion in gzip header */

t_streamp init_tws(t_streamp tsp, int fd, int zlib_level) {
  
  /* allocate the stream data */
  if (tsp == NULL)
    tsp = (t_streamp)malloc(sizeof(t_stream));
  
  tsp->fd = fd;
  tsp->bufsz = TS_BUFSZ;
  tsp->inbuf = (Bytef*)malloc(tsp->bufsz);
  tsp->outbuf = (Bytef*)malloc(tsp->bufsz);
  tsp->mode = TS_WRITE;
  tsp->zlib_err = Z_OK;
  tsp->zsp = NULL;
  tsp->crc32 = 0;
  tsp->total_bytes = 0;
  
  if (zlib_level > 0) {
    /* create the zlib stream */
    tsp->zsp = (z_streamp)malloc(sizeof(z_stream));
    tsp->zsp->zalloc = NULL;
    tsp->zsp->zfree = NULL;
    tsp->zsp->opaque = NULL;
    
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

/****** TODO: init_trs ***********/

/************ TODO: ts_write **********/

/************ TODO: ts_read **********/

/************ TODO: ts_checkpoint **********/

/************ TODO: ts_seek **********/

int ts_close(t_streamp tsp, int dofree) {
  
  /* check state */
  if (tsp == NULL)
    return 0;
  if (tsp->mode == TS_READ) {
    /* TODO: close read stream */
    return TS_ERR_BADMODE;
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
    
    if (dofree)
      free(tsp);
    
    return ret;
  } else {
    return TS_ERR_BADMODE;
  }
}
