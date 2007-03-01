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

#ifndef __INDEX_PARSER_H__
#define __INDEX_PARSER_H__

#include <sys/types.h>

struct index_parser_state {
  int version;
  int allocate_filename;
  int last_num;
};

struct index_entry {
  int version;
  /* 0-based index of entry in the index */
  int num;
  unsigned long ioffset;
  off64_t zoffset;
  unsigned long ilen;
  char *filename;
  int filename_allocated;
};

int init_index_parser(struct index_parser_state *state, char *header);

int parse_index_line(struct index_parser_state *state, char *line, struct index_entry *entry);

#endif
