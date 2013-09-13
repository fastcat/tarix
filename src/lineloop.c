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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"

#include "lineloop.h"
#include "tar.h"

/* our solution to line-based reading in a portable way:
 * it would be nice to use the bsd fgetln func, but that's not portable
 * so we just allocate a buffer and do sequential reads and memmoves
 */

int lineloop(int fd, lineprocessor_t lineprocessor, void *data) {
  int nread;
  char *linebuf = (char*)malloc(TARBLKSZ);
  int lpret = 0;
  int linebufsz = TARBLKSZ;
  int linebufavail = linebufsz;
  int linelen;
  int rdoff = 0;
  char *nlpos;
  
  /* linebufavail - 1: make sure there's room to drop a '\0' in */
  while ((nread = read(fd, linebuf + rdoff, linebufavail - 1)) > 0) {
    rdoff += nread;
    linebufavail -= nread;
    linebuf[rdoff] = 0; /* null terminate the input buffer */
    
    /* process any whole lines we've read */
    while ((nlpos = strchr(linebuf, '\n')) != NULL) {
      /* prep, parse the read line */
      linelen = nlpos - linebuf + 1;
      /* make the line a null-terminated string */
      *nlpos = 0;
      
      /* call the processor callback */
      lpret = lineprocessor(linebuf, data);
      if (lpret != 0)
        break;

      /* move the line out of the memory buffer, adjust variables */
      /* this will move the null we put at the end of the buffer too */
      memmove(linebuf, nlpos + 1, rdoff - linelen + 1);
      linebufavail += linelen;
      rdoff -= linelen;
    }

    /* if, after processing any lines, we don't have much space left in the
     * buffer, we increase it by a block
     */
    if (linebufavail < TARBLKSZ) {
      linebuf = realloc(linebuf, linebufsz + TARBLKSZ);
      linebufsz += TARBLKSZ;
      linebufavail += TARBLKSZ;
    }
  }
  
  free(linebuf);
  
  return lpret;
}
