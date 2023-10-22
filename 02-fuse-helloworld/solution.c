#include <solution.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <fuse.h>
static int _getattr(const char *path, struct stat *st, struct fuse_file_info *info) 
{

	(void)info;

	if (strcmp(path, "/") == 0)
	{
		st->st_mode = S_IFDIR | 0755;
		st->st_nlink = 2;
	}
	 else if (strcmp(path, "/hello") == 0) 
	 {
		st->st_mode = S_IFREG | 0444;
		st->st_nlink = 1;
		st->st_size = 64;
	} 
	else 
	{
		return -ENOENT;
	}
	return 0;
}
static int _readdir(const char *path, void *data, fuse_fill_dir_t filler,
           off_t off, struct fuse_file_info *info, enum fuse_readdir_flags frf)
{
	(void)off; 
	(void)info; 
	(void)frf;
	// if (strcmp(path, "/") != 0)
	// 	return -ENOENT;
	filler(data, ".", NULL, 0, 0);
	filler(data, "..", NULL, 0, 0);
	filler(data, "hello", NULL, 0, 0);
	return 0;
}
static int _read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *info)
{
	(void)info;
	(void)path;
	(void)offset;

	char file_data[64];
	sprintf(file_data, "hello, %d\n", fuse_get_context()->pid);
	u_int64_t length = strlen(file_data);
	if ((u_int64_t)offset < length) {
		if (offset + size > length)
			size = length - offset;
		memcpy(buf, file_data + offset, size);
	} 
	else
		size = 0;
	return size;	
}
static int _write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *info) {
	(void)path; 
	(void)buf; 
	(void)size; 
	(void)offset; 
	(void)info;
	return -EROFS;
}
static int _open(const char *path, struct fuse_file_info *fi)
{
	(void)path;

	if (strcmp(path, "/hello")!=0)
	return -ENOENT;
		
	if ((fi->flags & O_ACCMODE) != O_RDONLY)
		return -EROFS;

	return 0;
}
static int _create(const char *path, mode_t mode, struct fuse_file_info *info)
{
	(void)path;
	(void)mode;
	(void)info;
	return -EROFS;
}

static const struct fuse_operations hellofs_ops = {
	.read = _read,
	.readdir = _readdir, 
	.write = _write,
	.getattr = _getattr,
	.open = _open,
	.create = _create,
};

int helloworld(const char *mntp)
{
	char *argv[] = {"exercise", "-f", (char *)mntp, NULL};
	return fuse_main(3, argv, &hellofs_ops, NULL);
}
