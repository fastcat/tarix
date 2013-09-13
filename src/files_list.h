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

#ifndef __FILES_LIST_H__
#define __FILES_LIST_H__

struct files_list_state
{
  size_t argc;
  size_t argvlen;
  const char **argv;
  size_t *arglens;
};

int read_listfile(const char *file, char sep, char **bufptr, size_t *lenptr);
int append_args_to_files_list(struct files_list_state *files_list,
                              int argc, char *argv[], int firstarg);
int append_listfile_to_files_list(struct files_list_state *files_list,
                                  char sep, char *buf, size_t buflen);

#endif /* __FILES_LIST_H__ */
