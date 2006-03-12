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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
                     
#include "tarix.h"

#define OPTSTR "himf:t:xz123456789"

int show_help() {
  fprintf(stderr,
    "Usage: tarix [-hizx] [-<n>] [-f index_file] [-t tarfile]\n"
    "  -h   Show this help\n"
    "  -i   Explicitly create index, don't pass tar data to stdout\n"
    "  -z   Enable zlib (de)compression (default off)\n"
    "  -x   Use index to extract tar file\n"
    "  -<n> Set zlib compression level (default 3, same meaning as gzip)\n"
    "  -f   Set index file to use (else $TARIX_OUTFILE or out.tarix)\n"
    "  -t   Set tar file to use (otherwise stdin)\n"
    "  -m   Use mt (magnetic tape) IOCTLs for seeking instead of lseek\n"
    "\n"
    "The environment variable TARIX will be examined for arguments in\n"
    "addition to the command line\n"
    "\n"
    "The default action is to create an index and pass the tar data through\n"
    "to stdout so that tarix can be used with tar's --use-compress-program\n"
    "option.\n"
    "\n"
    "An archive created with zlib must be extracted thus too.\n"
    "A zlib'd archive will be readable with gunzip, but an archive\n"
    "compressed with gzip will not be readable by tarix\n"
  );
  return 0;
}

enum tarix_action {
  CREATE_INDEX,
  SHOW_HELP,
  EXTRACT_FILES
};

static int envgetopt(char **evarp, char *optstr) {
  char *evar = *evarp;
  int i;
  int error = '?';
  while (*evar == ' ')
    ++evar;
  if (evar[0] != '-') {
    if (evar[0] != 0) {
      fprintf(stderr, "error in format for TARIX environ options\n");
      error = '?';
      optopt = evar[0];
    } else {
      error = 0;
      *evarp = NULL;
    }
    return error;
  }
  for (i = 0; i < strlen(optstr); ++i) {
    if (optstr[i] == ':')
      continue;
    if (evar[1] == optstr[i]) {
      if (optstr[i+1] == ':') {
        char *spos;
        if (evar[2] != ' ') {
          error = ':';
          break;
        }
        spos = strchr(evar + 3, ' ');
        if (spos == NULL) {
          if (strlen(evar + 3) == 0) {
            error = ':';
            break;
          }
          /* this is one short so incr below is correct */
          spos = evar + 3 + strlen(evar + 3) - 1;
        } else
          *spos = 0;
        optarg = evar + 3;
        *evarp = spos + 1;
      } else {
        *evarp = evar + 2;
      }
      optopt = evar[1];
      return optopt;
    }
  }
  /* fall through means error */
  *evarp = NULL;
  optarg = NULL;
  optopt = evar[1];
  return error;
}

int main(int argc, char *argv[]) {
  int action = CREATE_INDEX;
  int opt;
  char *indexfile = NULL;
  char *tarfile = NULL;
  int pass_through = 1;
  int use_mt = 0;
  int use_zlib = 0;
  int zlib_level = 3;
  char *tenv = getenv("TARIX");
  
  /* parse opts, do right thing */
  while (1) {
    if (tenv != NULL)
      opt = envgetopt(&tenv, OPTSTR);
    /* envgetopt may bail if there is nothing to parse */
    if (tenv == NULL)
      opt = getopt(argc, argv, OPTSTR);
    if (opt == -1)
      break; /* no more options */
    switch (opt) {
      case 'h':
        action = SHOW_HELP;
        break;
      case 'i':
        action = CREATE_INDEX;
        pass_through = 0;
        break;
      case 'm':
        use_mt = 1;
        break;
      case 'f':
        if (indexfile)
          free(indexfile);
        indexfile = (char*)malloc(strlen(optarg) + 1);
        strcpy(indexfile, optarg);
        break;
      case 't':
        if (tarfile)
          free(tarfile);
        tarfile = (char*)malloc(strlen(optarg) + 1);
        strcpy(tarfile, optarg);
        break;
      case 'x':
        action = EXTRACT_FILES;
        break;
      case 'z':
        use_zlib = 1;
        break;
      case '1': case '2': case '3': case '4': case '5': case '6': case '7':
      case '9':
        zlib_level = opt - '0';
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
  
  if (!indexfile)
    indexfile = getenv("TARIX_OUTFILE");
  if (!indexfile)
    indexfile = TARIX_DEF_OUTFILE;
  
  if (!use_zlib)
    zlib_level = 0;
  
  switch (action) {
    case CREATE_INDEX:
      return create_index(indexfile, tarfile, pass_through, zlib_level);
    case SHOW_HELP:
      return show_help();
    case EXTRACT_FILES:
      return extract_files(indexfile, tarfile, use_mt, zlib_level,
        argc, argv, optind);
    default:
      fprintf(stderr, "EEK! unknown action!\n");
      return 1;
  }
  
  
  return -1;
}
