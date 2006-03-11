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

#ifndef __PORTABILITY_H__
#define __PORTABILITY_H__

#if defined(__FreeBSD__) /* on fbsd, all off_t are 64-bit */

#define P_O_LARGEFILE 0
#define p_lseek64 lseek
typedef off_t off64_t;

#else

#define P_O_LARGEFILE O_LARGEFILE
#define p_lseek64 lseek64

#endif

int p_mt_setblk(int fd, int blksz);
int p_mt_getpos(int fd, off64_t *offset);
int p_mt_setpos(int fd, off64_t offset);

#endif /* __PORTABILITY_H__ */
