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

int main (int argc, char **argv) {
  int fd;
  char *ofn = malloc(strlen(argv[0]) + strlen(".data") + 1);
  sprintf(ofn, "%s.data", argv[0]);
  if ((fd = open(ofn, O_WRONLY | O_CREAT | O_TRUNC, 0666)) < 0) {
    perror("open output");
    return 1;
  }
  
  t_streamp tsp = init_tws(NULL, fd, 3);
  
  if (tsp->zlib_err != Z_OK) {
    printf("zlib init error: %d\n", tsp->zlib_err);
    return 1;
  }
  
  int cr = ts_close(tsp, 1);
  if (cr == 0) {
    /* do nothing */
  } else if (cr == -1) {
    perror("ts_close");
    return 1;
  } else if (cr == TS_ERR_ZLIB) {
    printf("zlib end error\n");
    return 1;
  } else if (cr == TS_ERR_BADMODE) {
    printf("bad mode error\n");
    return 1;
  } else {
    printf("unknown return from ts_close: %d\n", cr);
  }
  
  return 0;
}
