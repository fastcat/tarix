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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
                     
#include "tarix.h"

int show_help()
{
	fprintf(stderr,
		"Usage: tarix [-hix] [-f index_file]\n"
		"  -h   Show this help\n"
		"  -i   Create index from tar file on stdin\n"
		"  -x   Use index specified with -f to extract tar file on stdin\n"
		"  -f   Set index file to use\n"
		"With no options, will create an index of the tar file on stdin and\n"
		"write it to the file specified by the TARIX_OUTFILE environment \n"
		"variable, or out.tarix if that is not set; in this mode, the tar file\n"
		"will passed through to stdout, so that tarix can be used with tar's\n"
		"--use-compress-program option.  If the -i option is used, the tar file\n"
		"will not be written to stdout.\n");
	return 0;
}

enum tarix_action
{
	CREATE_INDEX,
	SHOW_HELP,
	EXTRACT_FILES
};

int main(int argc, char *argv[], char *env[])
{
	int action = CREATE_INDEX;
	int opt;
	char *argixfile = NULL;
	int pass_through = 1;
	
	/* parse opts, do right thing */
	while ((opt = getopt(argc, argv, "hif:x")) != -1)
	{
		switch (opt)
		{
			case 'h':
				action = SHOW_HELP;
				break;
			case 'i':
				action = CREATE_INDEX;
				pass_through = 0;
				break;
			case 'f':
				if (argixfile)
					free(argixfile);
				argixfile = (char*)malloc(strlen(optarg) + 1);
				strcpy(argixfile, optarg);
				break;
			case 'x':
				action = EXTRACT_FILES;
				break;
			case ':':
				fprintf(stderr, "Missing arg to '%c' option\n", optopt);
				show_help();
				return 1;
				break;
			case '?':
				fprintf(stderr, "Unrecognized option '%c'\n", optopt);
				show_help();
				return 1;
				break;
			default:
				fprintf(stderr, "EEK! getopt returned an unrecognized value\n");
				return 1;
				break;
		}
	}
	switch (action)
	{
		case CREATE_INDEX:
			return create_index(argixfile, pass_through);
		case SHOW_HELP:
			return show_help();
		case EXTRACT_FILES:
			fprintf(stderr, "Extract not yet implemented\n");
			return 1;
		default:
			return extract_files(argc, argv, optind);
	}
	return -1;
}
