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

int extract_files(const char *indexfile, const char *tarfile,
		int argc, char *argv[], int firstarg)
{
	int *arglens;
	int n, nread;
	char *linebuf = (char*)malloc(TARBLKSZ);
	char *linebufrdpos = linebuf;
	int linebufsz = TARBLKSZ;
	int linebufavail = linebufsz;
	char *nlpos;
	int linelen;
	char *iparse;
	unsigned long ioffset, ilen;
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

	/* prep step: calculate string lengths of args only once */
	arglens = calloc(argc - firstarg, sizeof(int));
	for (n = firstarg; n < argc; ++n)
		arglens[n-firstarg] = strlen(argv[n]);
	
	/* our solution to line-based reading in a portable way:
	 * it would be nice to use the bsd fgetln func, but that's not portable
	 * so we just allocate a buffer and do sequential reads and memmoves
	 */
	
	/* linebufavail - 1: make sure there's room to drop a '\0' in */
	while ((nread = read(index, linebufrdpos, linebufavail - 1)) > 0)
	{
		linebufrdpos += nread;
		linebufavail -= nread;
		*linebufrdpos = 0; /* null terminate the input buffer */
		
		/* process any whole lines we've read */
		while ((nlpos = strchr(linebuf, '\n')) != NULL)
		{
			/* prep, parse the read line */
			linelen = nlpos - linebuf + 1;
			*nlpos = 0;
			if (gotheader) /* don't try and parse the header line */
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
						off64_t destoff = (off64_t)ioffset * 512;
						if (p_lseek64(tar, destoff, SEEK_SET) != destoff)
						{
							perror("lseek64 tarfile");
							return 1;
						}
						for (; ilen > 0; --ilen)
						{
							if ((n = read(tar, passbuf, TARBLKSZ)) < TARBLKSZ)
							{
								perror((n > 0) ? "partial tarfile read" : "read tarfile");
								return 2;
							}
							if ((n = write(1, passbuf, TARBLKSZ)) < TARBLKSZ)
							{
								perror((n > 0) ? "partial tarfile write" : "write tarfile");
								return 2;
							}
						}
					}
				}
			}
			else
				gotheader = 1;
			
			/* move the line out of the memory buffer, adjust variables */
			memmove(linebuf, nlpos+1, linelen + 1); /* move the null too! */
			linebufavail += linelen;
			linebufrdpos -= linelen;
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
