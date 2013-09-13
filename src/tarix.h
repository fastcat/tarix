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

#ifndef __TARIX_H__
#define __TARIX_H__

#include "files_list.h"

#define stringify(x) #x

#define TARIX_FORMAT_VERSION 2
#define TARIX_FORMAT_STRING "2"
#define TARIX_VERSION "1.0.7"
#define TARIX_DEF_OUTFILE "out.tarix"

int create_index(const char *indexfile, const char *tarfile,
  int pass_through, int zlib_level, int debug_messages);
int extract_files(const char *indexfile, const char *tarfile,
  const char *outfile, int use_mt, int zlib_level, int debug_messages,
  int glob_flags, int exclude_mode, int exact_match,
  const struct files_list_state *files_list);

#endif /* __TARIX_H__ */
