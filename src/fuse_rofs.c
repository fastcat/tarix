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

// functions that always return EROFS

static int tarix_mknod(const char *path, mode_t mode, dev_t rdev) {
  return -EROFS;
}

// covers mkdir, chmod
static int tarix_rofs_pathmode(const char *path, mode_t mode) {
  return -EROFS;
}

// covers unlink, rmdir
static int tarix_rofs_path(const char *path) {
  return -EROFS;
}

// covers symlink, rename, link
static int tarix_rofs_2path(const char *path1, const char *path2) {
  return -EROFS;
}

static int tarix_chown(const char *path, uid_t uid, gid_t gid) {
  return -EROFS;
}

static int tarix_truncate(const char *path, off_t size) {
  return -EROFS;
}

static int tarix_write(const char *path, const char *buf, size_t size,
    off_t off, struct fuse_file_info *fi) {
  return -EROFS;
}

static int tarix_setxattr(const char *path, const char *name,
    const char *value, size_t size, int flags) {
  return -EROFS;
}

static int tarix_create(const char *path, mode_t mode,
    struct fuse_file_info *fi) {
  return -EROFS;
}

static int tarix_ftruncate(const char *path, off_t size,
    struct fuse_file_info *fi) {
  return -EROFS;
}

static int tarix_utimens(const char *path, const struct timespec tv[2]) {
  return -EROFS;
}
