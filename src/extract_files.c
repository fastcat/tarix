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

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "tar.h"
#include "portability.h"

struct index_entry
{
	unsigned long start;
	unsigned long length;
	char *filename;
};

int extract_files(const char *indexfile, const char *tarfile, int use_mt,
		int argc, char *argv[], int firstarg)
{
	int *arglens;
	int n, nread;
	char *linebuf = (char*)malloc(TARBLKSZ);
	int rdoff = 0;
	int linebufsz = TARBLKSZ;
	int linebufavail = linebufsz;
	char *nlpos;
	int linelen;
	char *iparse;
	unsigned long ioffset, ilen;
	off64_t baseoffset = 0, destoff, curpos;
	int gotheader = 0;
	char passbuf[TARBLKSZ];
	int index, tar;
	
	/* the basic idea:
	 * read the index an entry at a time
	 * scan the index entry against each arg and pass through the file if
	 * it matches
	 */
	
	/* open the index file */
	if ((index = open(indexfile, O_RDONLY, 0666)) < 0)
	{
		perror("open indexfile");
		return 1;
	}
	/* maybe warn user about no largefile on stdin? */
	if (tarfile == NULL)
		tar = 0;
	else
	{
		if ((tar = open(tarfile, O_RDONLY|P_O_LARGEFILE)) < 0)
		{
			perror("open tarfile");
			return 1;
		}
	}
	
	/* On tape devices and such, a tar archive very likely will not start at
	 * offset 0, so we grab the base offset and use it for future seeks
	 */
	/* If we use the mt method, we store offsets & stuff as block numbers */
	if (use_mt)
	{
		/* for tapes there are 2 steps, first we set the block size, then we
		 * query the position
		 */
		if (p_mt_setblk(tar, TARBLKSZ) != 0)
		{
			perror("set tape block size = 512");
			return 1;
		}
		if (p_mt_getpos(tar, &baseoffset) != 0)
		{
			perror("tape get pos");
			return 1;
		}
	}
	else
	{
		if ((baseoffset = p_lseek64(tar, 0, SEEK_CUR)) < 0)
		{
			perror("lseek(tell) tarfile base offset");
			return 1;
		}
	}
	
	curpos = baseoffset;

	/* prep step: calculate string lengths of args only once */
	arglens = calloc(argc - firstarg, sizeof(int));
	for (n = firstarg; n < argc; ++n)
		arglens[n-firstarg] = strlen(argv[n]);
	
	/* our solution to line-based reading in a portable way:
	 * it would be nice to use the bsd fgetln func, but that's not portable
	 * so we just allocate a buffer and do sequential reads and memmoves
	 */
	
	/* linebufavail - 1: make sure there's room to drop a '\0' in */
	while ((nread = read(index, linebuf + rdoff, linebufavail - 1)) > 0)
	{
		rdoff += nread;
		linebufavail -= nread;
		linebuf[rdoff] = 0; /* null terminate the input buffer */
		
		/* process any whole lines we've read */
		while ((nlpos = strchr(linebuf, '\n')) != NULL)
		{
			/* prep, parse the read line */
			linelen = nlpos - linebuf + 1;
			*nlpos = 0;
			if (!gotheader) /* don't try and parse the header line */
				gotheader = 1;
			else
			{
				/* FIXME: this may segfault on an invalid index format */
				iparse = linebuf;
				ioffset = strtoul(iparse, &iparse, 10);
				++iparse;
				ilen = strtoul(iparse, &iparse, 10);
				++iparse;

				/* take action on the line */
				for (n = firstarg; n < argc; ++n)
				{
					if (strncmp(argv[n], iparse, arglens[n-firstarg]) == 0)
					{
						/* seek to the record start and then pass the record through */
						/* don't actually seek if we're already there */
						if (use_mt)
						{
							/* mt uses block based seeks! */
							if (curpos != ioffset + baseoffset) {
								if (p_mt_setpos(tar, ioffset + baseoffset) != 0) {
									perror("mt seek");
									return 1;
								}
								curpos = ioffset + baseoffset;
							}
						}
						else
						{
							destoff = (off64_t)ioffset * TARBLKSZ + baseoffset;
							if (curpos != destoff) {
								if (p_lseek64(tar, destoff, SEEK_SET) != destoff) {
									perror("lseek64 tarfile");
									return 1;
								}
								curpos = destoff;
							}
						}
						/* this destroys ilen, but that's ok since we're only gonna
						 * extract the file once
						 */
						for (; ilen > 0; --ilen)
						{
							if ((n = read(tar, passbuf, TARBLKSZ)) < TARBLKSZ)
							{
								perror((n > 0) ? "partial tarfile read" : "read tarfile");
								return 2;
							}
							curpos += use_mt ? 1 : TARBLKSZ;
							if ((n = write(1, passbuf, TARBLKSZ)) < TARBLKSZ)
							{
								perror((n > 0) ? "partial tarfile write" : "write tarfile");
								return 2;
							}
						}
						break; /* only extract file once, breaks out of for n argv */
					} /* if index item matches arg n */
				} /* for n over extract args */
			} /* if gotheader */
			
			/* move the line out of the memory buffer, adjust variables */
			/* this will move the null we put at the end of the buffer too */
			memmove(linebuf, nlpos+1, rdoff - linelen + 1);
			linebufavail += linelen;
			rdoff -= linelen;
		}
		
		/* if, after processing any lines, we don't have much space left in the
		 * buffer, we increase it by a block
		 */
		if (linebufavail < TARBLKSZ)
		{
			linebuf = realloc(linebuf, linebufsz + TARBLKSZ);
			linebufsz += TARBLKSZ;
			linebufavail += TARBLKSZ;
		}
	}
	return 0;
}
