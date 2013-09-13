/*
 *  tarix - a GNU/POSIX tar indexer
 *  Copyright (C) 2013 Jean-Charles BERTIN
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
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include "config.h"

#include "files_list.h"

#define LISTBLKSZ (4 * 1024)


int read_listfile(const char *file, char sep, char **bufptr, size_t *lenptr)
{
  int fd;
  char *buf;
  size_t buflen;
  size_t bufavail;
  size_t rdoff;
  ssize_t nread;
  
  fd = open(file, O_RDONLY);
  if (fd < 0)
    return 1;
  
  rdoff = 0;
  buflen = bufavail = LISTBLKSZ;
  buf = (char*)malloc(buflen);
  while ((nread = read(fd, buf + rdoff, bufavail)) > 0)
  {
    rdoff += nread;
    bufavail -= nread;
    
    if (bufavail == 0)
    {
      bufavail = LISTBLKSZ;
      buflen += LISTBLKSZ;
      buf = (char*)realloc(buf, buflen);
    }
  }
  
  close(fd);
  if (nread < 0)
  {
    free(buf);
    return 1;
  }
  
  /* make sure that listfile buffer is ended with separator */
  if (rdoff == 0 || buf[rdoff - 1] != sep)
  {
    if (bufavail == 0)
    {
      buflen += 1;
      buf = (char*)realloc(buf, buflen);
    }
    buf[rdoff++] = sep;
  }
  
  *bufptr = buf;
  *lenptr = rdoff;
  return 0;
}

int append_args_to_files_list(struct files_list_state *files_list,
                              int argc, char *argv[], int firstarg)
{
  size_t argidx;
  ssize_t argvlen = argc - firstarg;
  
  if (argvlen <= 0)
    return 0;
  
  if (files_list->argc + argvlen > files_list->argvlen) {
    files_list->argvlen = files_list->argc + argvlen;
    files_list->argv = realloc(files_list->argv,
                               files_list->argvlen * sizeof(char*));
    files_list->arglens = realloc(files_list->arglens,
                                  files_list->argvlen * sizeof(size_t));
  }
  
  for (argidx = firstarg; argidx < argc; argidx ++) {
    const char* arg = argv[argidx];
    size_t arglen = strlen(arg);
    if (arglen > 0) {
      files_list->argv[files_list->argc] = arg;
      files_list->arglens[files_list->argc] = arglen;
      files_list->argc ++;
    }
  }
  
  return 0;
}

int append_listfile_to_files_list(struct files_list_state *files_list,
                                  char sep, char *buf, size_t buflen)
{
  if (sep == '\n')
  {
    char *linebuf = buf;
    while ((linebuf - buf) < buflen)
    {
      char *nlpos = strchr(linebuf, '\n');
      if (nlpos == NULL)
        break;
      
      size_t linelen = nlpos - linebuf;
      /* make the line a null-terminated string */
      *nlpos = 0;
      
      if (linelen > 0) {
        if (files_list->argc == files_list->argvlen) {
          files_list->argvlen += 64;
          files_list->argv = realloc(files_list->argv,
                                     files_list->argvlen * sizeof(char*));
          files_list->arglens = realloc(files_list->arglens,
                                        files_list->argvlen * sizeof(size_t));
        }
        
        files_list->argv[files_list->argc] = linebuf;
        files_list->arglens[files_list->argc] = linelen;
        files_list->argc ++;
      }
      
      linebuf = nlpos + 1;
    }
  }
  else
  {
    char *linebuf = buf;
    while ((linebuf - buf) < buflen)
    {
      size_t linelen = strlen(linebuf);
      
      if (linelen > 0) {
        if (files_list->argc == files_list->argvlen) {
          files_list->argvlen += 64;
          files_list->argv = realloc(files_list->argv,
                                     files_list->argvlen * sizeof(char*));
          files_list->arglens = realloc(files_list->arglens,
                                        files_list->argvlen * sizeof(size_t));
        }
        
        files_list->argv[files_list->argc] = linebuf;
        files_list->arglens[files_list->argc] = linelen;
        files_list->argc ++;
      }
      
      linebuf += linelen + 1;
    }
  }
  
  return 0;
}
