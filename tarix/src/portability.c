/*
 *  tarix - a GNU/POSIX tar indexer
 *  Copyright (C) 2002 Matthew "Cheetah" Gabeler-Lee
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

#include <sys/ioctl.h>
#include <sys/mtio.h>

#include "portability.h"

/* linux defaults to hardware block addresses, so I guess we'll have
 * freebsd use them, since you have to be explicit with freebsd */

int p_mt_setblk(int fd, int blksz)
{
	struct mtop mt_op;
	
	#if defined(__FreeBSD__)
		mt_op.mt_op = MTSETBSIZ;
	#else
		mt_op.mt_op = MTSETBLK;
	#endif
	mt_op.mt_count = blksz;
	return ioctl(fd, MTIOCTOP, &mt_op);
}

int p_mt_getpos(int fd, off64_t *offset)
{
	int ioctr;
	#if defined(__FreeBSD__)
		u_int32_t mt_pos = *offset;
		ioctr = ioctl(fd, MTIOCRDHPOS, &mt_pos);
		*offset = mt_pos;
	#else
		struct mtpos mt_pos;
		mt_pos.mt_blkno = *offset;
		ioctr = ioctl(fd, &mt_pos);
		*offset = mt_pos.mt_blkno;
	#endif
	return ioctr;
}

/* mt uses block based seeks */
int p_mt_setpos(int fd, off64_t offset)
{
	#if defined(__FreeBSD__)
		u_int32_t mt_pos = offset;
		return ioctl(fd, MTIOCHLOCATE, &mt_pos);
	#else
		struct mtop mt_op;
		mt_op.mt_op = MTSEEK;
		mt_op.mt_count = offset;
		return ioctl(fd, MTIOCTOP, &mt_op);
	#endif
}
