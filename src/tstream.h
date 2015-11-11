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

#ifndef __TSTREAM_H__
#define __TSTREAM_H__

/* structures and functions for a tar read & write stream */

#include <zlib.h>

#include "portability.h"

typedef struct _t_stream {
  /* real file descriptor to read/write from */
  int fd;
  /* buffers for zlib work */
  uLong bufsz;
  Bytef *inbuf;
  Bytef *outbuf;
  /* zlib stream */
  z_streamp zsp;
  /* stream mode, one of TS_READ or TS_WRITE */
  int mode;
  /* return code from the last zlib call */
  int zlib_err;
  /* crc32 of input data processed so far */
  unsigned long crc32;
  /* number of raw data bytes processed so far */
  off64_t raw_bytes;
  /* number of zlib stream bytes processed so far (includes headers) */
  off64_t zlib_bytes;
#if HAVE_MTIO_H
  /* boolean for using magnetic tape interface or not */
  int usemt;
#endif
  /* block size for magnetic tape operations */
  int blksz;
  /* base offset of archive start */
  off64_t baseoffset;
} t_stream;

typedef struct _t_stream *t_streamp;

/* constants for the mode member of t_stream */
#define TS_CLOSED 0;
#define TS_READ 1
#define TS_WRITE 2

/* default buffer size for the tar stream
 * 10240 corresponds to a tar blocking factor of 20
 */
#define TS_BUFSZ 10240

/* error constants */
#define TS_ERR_ZLIB -2
#define TS_ERR_BADMODE -3

/* Create/init a tar write stream.
 * If in is not null, it will be used, otherwise a new item will be allocated.
 * If zlib_level > 0, zlib will be initialized, otherwise it will be
 * a straight write through.
 * The fd argument is the file descriptor to do the real writes to.
 * The initialized t_streamp will be returned.
 * If zlib is requested, the caller should check tsp->zlib_error.
 * If zlib initialization failed, the caller must call ts_close on the stream
 * and then discard it.
 */
t_streamp init_tws(t_streamp tsp, int fd, int usemt, int blksz,
  int zlib_level);

/* Create/init a tar read stream.
 * If in is not null, it will be used, otherwise a new item will be allocated.
 * If zlib_level > 0, zlib will be initialized, otherwise it will be
 * a straight read through.
 * The fd argument is the file descriptor to do the real reads from.
 * The initialized t_streamp will be returned.
 * If the magic is wrong, zlib_err will be set to Z_VERSION_ERROR.
 */
t_streamp init_trs(t_streamp tsp, int fd, int usemt, int blksz,
  int zlib_level);

/* Write bytes to the stream.  If zlib is enabled, data may be buffered.
 * Returns the number of bytes processed, which is not necessarily the
 * number of bytes written to the fd in the case of zlib.
 * If the stream is not a write stream, TS_ERR_BADMODE will be returned.
 * If a zlib error ocurrs, TS_ERR_ZLIB will be returned, and the zlib
 * code can be found in tsp->zlib_err.
 * If a real write error ocurrs, -1 will be returned, as with write(2).
 * The function will make a best effort to write as many bytes as
 * requested.
 */
int ts_write(t_streamp tsp, void *buf, int len);

/* Read bytes from the stream.
 * Returns the number of bytes written into buf.
 * If the stream is not a read stream, TS_ERR_BADMODE will be returned.
 * If a zlib error ocurrs, TS_ERR_ZLIB will be returned, and the zlib
 * code can be found in tsp->zlib_err.
 * If a real read error ocurrs, -1 will be returned, as with read(2).
 */
int ts_read(t_streamp tsp, void *buf, int len);

/* Checkpoint a write stream.  Only meaningful for zlib streams, but calling
 * it on a non-zlib stream is harmless (equivalent to a no-op).
 * For a zlib stream, this will flush the zlib stream to make a seekable
 * point in the compressed stream.
 * Return value will be the real byte position in the output of the sync
 * point, -1 on write errors, TS_ERR_ZLIB on zlib errors, and TS_ERR_BADMODE
 * if a read stream is passed in.
 */
off64_t ts_checkpoint(t_streamp tsp);

/* Seek in an input stream, similar to lseek(2).  Always acts in the whence
 * = SEEK_SET mode.  Note that for zlib streams, the offset MUST be the
 * actual offset in the zlib stream, not the logical offset in the
 * uncompressed stream.  Returns 0 on success, -1 on i/o error,
 * TS_ERR_BADMODE if the stream is not a read stream, and TS_ERR_ZLIB if
 * there is a zlib error.
 */
int ts_seek(t_streamp tsp, off64_t offset);

/* Close and free a stream (read or write).  Returns 0 on success, -1 on i/o
 * errors, TS_ERR_ZLIB on zlib errors, or TS_ERR_BADMODE if the stream
 * is in an invalid state.
 * Since the caller provided the file descriptor on init, it must deal with
 * closing it after closing the tar stream.
 * If dofree != 0, it will also deallocate the struct itself.  The caller
 * must match up usage of the dofree parameter with how the stream was
 * allocated at init_tws or init_trs.
 */
int ts_close(t_streamp tsp, int dofree);

/* Util function like perror, but handles multistate errors from tstream
 * functions
 */
void ptserror(const char *msg, off64_t rv, t_streamp tsp);

#endif /* __TSTREAM_H__ */
