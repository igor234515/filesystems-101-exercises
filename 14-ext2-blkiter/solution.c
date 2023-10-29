#include <solution.h>
#include <fs_malloc.h>
#include <ext2fs/ext2fs.h>
#include <ext2fs/ext2_fs.h>
#include <errno.h>
#include <unistd.h>

struct ext2_fs
{
	int fd;
	int blkSize;
	struct ext2_super_block SB;
};
struct mem_blk
{
	int bid;
	int *b_pointer;
};
struct ext2_blkiter
{
	
	
	struct ext2_fs *fs;
	int inode_table;

	struct ext2_inode inode;
	int current;

	struct mem_blk direct;

	struct mem_blk reverse;
};
static int offset(struct ext2_fs *fs, int blk)
{
	return blk * fs->blkSize;
}
static void iniMemBlk(struct mem_blk *b)
{
	b->bid = 0;
	b->b_pointer = NULL;
}
static int GetMemBlk(struct mem_blk *b, int new_id, struct ext2_fs *fs)
{
	if (b->b_pointer && new_id == b->bid)
	{
		return 0;
	}

	if (!b->b_pointer)
	{
		b->b_pointer = fs_xmalloc(fs->blkSize);
	}

	int ret = pread(fs->fd, b->b_pointer, fs->blkSize, offset(fs, new_id));
	if (ret == -1)
	{
		return -1;
	}

	b->bid = new_id;
	return 1;
}
static int process(struct ext2_blkiter *i, int *blkno, int id)
{
	int ptr = i->inode.i_block[id];
	if (ptr == 0)
	{
		return 0;
	}
	*blkno = ptr;
	i->current++;

	return 1;
}
int ext2_fs_init(struct ext2_fs **fs, int fd)
{
	struct ext2_fs *file_system = fs_xmalloc(sizeof(struct ext2_fs));
	file_system->fd = fd;

	int atmpt = pread(file_system->fd, &file_system->SB, SUPERBLOCK_SIZE, SUPERBLOCK_OFFSET);
	if (atmpt == -1)
	{
		fs_xfree(file_system);
		return -errno;
	}
	file_system->blkSize = EXT2_BLOCK_SIZE(&file_system->SB);
	*fs = file_system;
	return 0;
}
void ext2_fs_free(struct ext2_fs *fs)
{
	fs_xfree(fs);
}
int ext2_blkiter_init(struct ext2_blkiter **i, struct ext2_fs *fs, int ino)
{
	int groupId = (ino - 1) / fs->SB.s_inodes_per_group;
	int inoId = (ino - 1) % fs->SB.s_inodes_per_group;

	int numBlkDesc = fs->blkSize / sizeof(struct ext2_group_desc);

	int blckno = fs->SB.s_first_data_block + 1 + groupId / numBlkDesc;
	int blkoff = (groupId % numBlkDesc) * sizeof(struct ext2_group_desc);

	struct ext2_group_desc group_desc;
	int ret = pread(fs->fd, &group_desc, sizeof(group_desc), offset(fs, blckno) + blkoff);
	
	if (ret == -1)
	{
		return -errno;
	}
	struct ext2_blkiter *next_iter = fs_xmalloc(sizeof(struct ext2_blkiter));
	next_iter->inode_table = group_desc.bg_inode_table;

	ret = pread(fs->fd, &next_iter->inode, sizeof(struct ext2_inode),
			offset(fs, next_iter->inode_table) + inoId * fs->SB.s_inode_size);
	
	if (ret == -1)
	{
		return -errno;
	}

	*i = next_iter;
	next_iter->current = 0;

	iniMemBlk(&next_iter->direct);
	iniMemBlk(&next_iter->reverse);
	
	next_iter->fs = fs;

	return 0;
}

int ext2_blkiter_next(struct ext2_blkiter *i, int *blkno)
{
	int numBlkPtrs = i->fs->blkSize / sizeof(int);

	int direct_end = EXT2_NDIR_BLOCKS;
	int reverse_start = direct_end;
	int reverse_end = direct_end + numBlkPtrs;
	int rreverse_start = reverse_end;
	int rreverse_end = reverse_end + numBlkPtrs * numBlkPtrs;

	if (i->current < direct_end)
	{
		return process(i, blkno, i->current);
	}

	if (i->current < reverse_end)
	{
		int updated = GetMemBlk(&i->direct, i->inode.i_block[EXT2_IND_BLOCK], i->fs);
		if (updated)
		{
			if (updated < 0)
			{
				return -errno;
			}

			*blkno = i->inode.i_block[EXT2_IND_BLOCK];
			return 1;
		}

		int rev_pos = i->current - reverse_start;
		int ptr = i->direct.b_pointer[rev_pos];

		if (ptr == 0)
		{
			return 0;
		}

		*blkno = ptr;
		i->current++;
		return 1;
	}
	if (i->current < rreverse_end)
	{
		int updated = GetMemBlk(&i->reverse, i->inode.i_block[EXT2_DIND_BLOCK], i->fs);
		if (updated)
		{
			if (updated < 0)
			{
				return -errno;
			}
			*blkno = i->inode.i_block[EXT2_DIND_BLOCK];
			return 1;
		}

		int rrev_pos = (i->current - rreverse_start) / numBlkPtrs;
		int rev_pos = (i->current - rreverse_start) % numBlkPtrs;

		updated = GetMemBlk(&i->direct, i->reverse.b_pointer[rrev_pos], i->fs);
		if (updated)
		{
			if (updated < 0)
			{
				return -errno;
			}
			*blkno = i->reverse.b_pointer[rrev_pos];
			return 1;
		}

		int ptr = i->direct.b_pointer[rev_pos];
		if (ptr == 0)
		{
			return 0;
		}
		
		*blkno = ptr;
		i->current++;
		return 1;
	}

	return 0;
}

void ext2_blkiter_free(struct ext2_blkiter *i)
{
	if (i != NULL)
	{
		fs_xfree(i->direct.b_pointer);
		fs_xfree(i->reverse.b_pointer);
		fs_xfree(i);
	}
	return;
}
