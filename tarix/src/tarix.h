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

#define TARIX_FMT_VERSION_OLD "0"
#define TARIX_FMT_VERSION_NEW "1"
#define TARIX_VERSION "1.0.1"
#define TARIX_DEF_OUTFILE "out.tarix"

int create_index(const char *indexfile, const char *tarfile,
	int pass_through, int zlib_level);
int extract_files(const char *indexfile, const char *tarfile, int use_mt,
	int zlib_level, int argc, char *argv[], int firstarg);

#endif /* __TARIX_H__ */
