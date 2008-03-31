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

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "debug.h"
#include "index_parser.h"
#include "lineloop.h"
#include "portability.h"
#include "tar.h"
#include "tarix.h"
#include "tstream.h"

struct extract_files_state {
  int gotheader;
  struct index_parser_state ipstate;
  int argc;
  char **argv;
  int *arglens;
  int debug_messages;
  /* curpos always tracks block offsets */
  off64_t curpos;
  int zlib_level;
  t_streamp tsp;
  int outfd;
};

int extract_files_lineloop_processor(char *line, void *data) {
  struct extract_files_state *state = (struct extract_files_state*)data;
  
  int n;
  /* for the DMSG macro */
  int debug_messages = state->debug_messages;
  off64_t destoff;
  
  if (!state->gotheader) {
    if (init_index_parser(&state->ipstate, line) != 0)
      return 1;
    state->ipstate.allocate_filename = 0;
    state->gotheader = 1;
  } else {
    int extract = 0;
    struct index_entry entry;
    memset(&entry, 0, sizeof(entry));
    
    if (parse_index_line(&state->ipstate, line, &entry) != 0)
      return 1;

    /* take action on the line */
    for (n = 0; n < state->argc; ++n) {
      /* does the start of the item match an extract arg? */
      if (strncmp(state->argv[n], entry.filename, state->arglens[n]) == 0) {
        extract = 1;
        break;
      }
    }
    if (extract) {
      char passbuf[TARBLKSZ];
      
      DMSG("extracting %s\n", entry.filename);
      /* seek to the record start and then pass the record through */
      /* don't actually seek if we're already there */
      if (state->curpos != entry.blocknum) {
        destoff = state->zlib_level ? entry.offset : entry.blocknum * TARBLKSZ;
        DMSG("seeking to %lld\n", (long long)destoff);
        if (ts_seek(state->tsp, destoff) != 0) {
          fprintf(stderr, "seek error\n");
          return 1;
        }
        state->curpos = entry.blocknum;
      }
      DMSG("reading %ld records\n", entry.blocklength);
      for (unsigned long bnum = 0; bnum < entry.blocklength; ++bnum) {
        if ((n = ts_read(state->tsp, passbuf, TARBLKSZ)) < TARBLKSZ) {
          if (n >= 0)
            perror("partial tarfile read");
          else
            ptserror("read tarfile", n, state->tsp);
          return 2;
        }
        DMSG("read a rec, now at %lld, %ld left\n",
          (long long)state->curpos, entry.blocklength - bnum - 1);
        ++state->curpos;
        if ((n = write(state->outfd, passbuf, TARBLKSZ)) < TARBLKSZ) {
          perror((n > 0) ? "partial tarfile write" : "write tarfile");
          return 2;
        }
        DMSG("wrote rec\n");
      }
    } /* if extract */
  } /* if gotheader */
  
  return 0;
}

int extract_files(const char *indexfile, const char *tarfile, int use_mt,
    int zlib_level, int debug_messages, int argc, char *argv[],
    int firstarg) {
  int n;
  int index, tar;
  struct extract_files_state state;
  
  memset(&state, 0, sizeof(state));
  
  /* the basic idea:
   * read the index an entry at a time
   * scan the index entry against each arg and pass through the file if
   * it matches
   */
  
  /* open the index file */
  if ((index = open(indexfile, O_RDONLY, 0666)) < 0) {
    perror("open indexfile");
    return 1;
  }
  /* maybe warn user about no largefile on stdin? */
  if (tarfile == NULL) {
    /* stdin */
    tar = 0;
  } else {
    if ((tar = open(tarfile, O_RDONLY|P_O_LARGEFILE)) < 0) {
      perror("open tarfile");
      return 1;
    }
  }
  
  /* tstream handles base offset */
  state.tsp = init_trs(NULL, tar, use_mt, TARBLKSZ, zlib_level);
  if (state.tsp->zlib_err != Z_OK) {
    fprintf(stderr, "zlib init error: %d\n", state.tsp->zlib_err);
    return 1;
  }
  
  /* prep step: calculate string lengths of args only once */
  state.arglens = calloc(argc - firstarg, sizeof(int));
  state.argc = argc - firstarg;
  state.argv = argv + firstarg;
  for (n = firstarg; n < argc; ++n)
    state.arglens[n-firstarg] = strlen(argv[n]);
  
  state.debug_messages = debug_messages;
  state.zlib_level = zlib_level;
  state.outfd = 1;
  
  lineloop(index, extract_files_lineloop_processor, (void*)&state);
  
  return 0;
}
