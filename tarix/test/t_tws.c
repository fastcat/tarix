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

/* test case for simple tar write stream */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <zlib.h>

#include "tstream.h"

#define OFILE "bin/test/data.gz"

void ptserr(const char *msg, off64_t rv, t_streamp tsp) {
  if (rv == TS_ERR_ZLIB) {
    printf("zlib deflate error: %d\n", tsp->zlib_err);
  } else if (rv == TS_ERR_BADMODE) {
    printf("invalid mode\n");
  } else if (rv == -1) {
    perror("zlib write");
  } else {
    printf("unknown error: %lld\n", rv);
  }
}

int main (int argc, char **argv) {
  int fd, rv;
  t_streamp tsp;
  off64_t cpoff;
  
  if ((fd = open(OFILE, O_WRONLY | O_CREAT | O_TRUNC, 0666)) < 0) {
    perror("open output");
    return 1;
  }
  
  tsp = init_tws(NULL, fd, 0, 0, 3);
  if (tsp->zlib_err != Z_OK) {
    printf("zlib init error: %d\n", tsp->zlib_err);
    return 1;
  }
  
  rv = ts_write(tsp, "Hello World\n", 12);
  if (rv < 0) {
    ptserr("ts_write 1", rv, tsp);
    return 1;
  } else if (rv != 12) {
    printf("write returned %d\n", rv);
    return 1;
  }
  
  cpoff = ts_checkpoint(tsp);
  if (cpoff < 0) {
    ptserr("ts_checkpoint", cpoff, tsp);
    return 1;
  }
  printf("checkpointed at output byte 0x%llx\n", cpoff);
  
  rv = ts_write(tsp, "Hello World\n", 12);
  if (rv < 0) {
    ptserr("ts_write 2", rv, tsp);
    return 1;
  } else if (rv != 12) {
    printf("write returned %d\n", rv);
    return 1;
  }
  
  rv = ts_close(tsp, 1);
  if (rv == 0) {
    /* do nothing */
  } else if (rv == -1) {
    perror("ts_close");
    return 1;
  } else if (rv == TS_ERR_ZLIB) {
    printf("zlib end error\n");
    return 1;
  } else if (rv == TS_ERR_BADMODE) {
    printf("bad mode error\n");
    return 1;
  } else {
    printf("unknown return from ts_close: %d\n", rv);
  }
  
  return 0;
}
