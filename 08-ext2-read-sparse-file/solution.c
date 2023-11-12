#include <solution.h>
#include <ext2fs/ext2fs.h>
#include <unistd.h>
#include <sys/stat.h>

#define EXT2_SB_OFFSET 1024

int block_Size(struct ext2_super_block *SB)
{
	return 1024 << SB->s_log_block_size;
}

int block_Read(int img, int out, size_t blk, size_t blk_size, size_t _part)
{

	char buffer[_part];

	int status = pread(img, buffer, _part, blk * blk_size);

	if (status < 0)
	{
		return -errno;
	}
	status = write(out, buffer, _part);

	if (status < 0)
	{
		return -errno;
	}

	return 0;
}

int inode_Read(int img, struct ext2_inode *inode, int inode_number, struct ext2_super_block *SB)
{

	size_t inode_index = (inode_number - 1) % SB->s_inodes_per_group;
	size_t desc_index = (inode_number - 1) / SB->s_inodes_per_group;

	struct ext2_group_desc group_desc;

	int blk_size = EXT2_BLOCK_SIZE(SB);

	size_t offset = (SB->s_first_data_block + 1) * blk_size + sizeof(struct ext2_group_desc) * desc_index;

	int status = pread(img, &group_desc, sizeof(struct ext2_group_desc), offset);

	if (status < 0)
	{
		return -errno;
	}

	int pos = group_desc.bg_inode_table * blk_size + inode_index * SB->s_inode_size;

	int inode_size = sizeof(struct ext2_inode);
	status = pread(img, inode, inode_size, pos);

	if (status)
	{
		return -errno;
	}
	return 0;
}

int copy_file(int img, int out, struct ext2_super_block *SB, int inode_number)
{

	size_t blk_size = EXT2_BLOCK_SIZE(SB);
	struct ext2_inode inode;
	int status = inode_Read(img, &inode, inode_number, SB);

	if (status < 0)
	{
		return -errno;
	}

	size_t read = inode.i_size;

	size_t _part = 0;

	for (size_t i = 0; i < EXT2_NDIR_BLOCKS; ++i)
	{

		if (read > blk_size)
		{
			_part = blk_size;
		}
		else
		{
			_part = read;
		}
		status = block_Read(img, out, inode.i_block[i], blk_size, _part);
		if (status < 0)
		{
			return -errno;
		}

		if (!(read -= _part))
		{
			return 0;
		}
	}

	_part = 0;
	uint32_t blk[blk_size];
	status = pread(img, blk, blk_size, blk_size * inode.i_block[EXT2_IND_BLOCK]);
	if (status < 0)
	{
		return -errno;
	}

	for (uint i = 0; i < blk_size / sizeof(int); ++i)
	{
		_part = read > blk_size ? blk_size : read;

		if (block_Read(img, out, blk[i], blk_size, _part) < 0)
		{
			return -errno;
		}

		if (!(read -= _part))
		{
			return 0;
		}
	}

	_part = 0;
	size_t num = blk_size / sizeof(int);
	uint32_t *double_blk = malloc(2 * blk_size);
	uint32_t *blk_index = double_blk + num;
	status = pread(img, double_blk, blk_size, inode.i_block[EXT2_DIND_BLOCK] * blk_size);
	if (status < 0)
	{
		free(double_blk);
		return -errno;
	}

	for (size_t i = 0; i < num; ++i)
	{
		status = pread(img, blk_index, blk_size, double_blk[i] * blk_size);
		if (status < 0)
		{
			free(double_blk);
			return -errno;
		}
		for (size_t j = 0; j < num; ++j)
		{

			if (read > blk_size)
			{
				_part = blk_size;
			}
			else
			{
				_part = read;
			}
			status = block_Read(img, out, blk_index[j], blk_size, _part);
			if (status < 0)
			{
				free(double_blk);
				return -errno;
			}

			if (!(read -= _part))
			{
				free(double_blk);
				return 0;
			}
		}
	}

	free(double_blk);

	return 0;
}

int dump_file(int img, int inode_nr, int out)
{
	struct ext2_super_block SB;
	struct ext2_inode inode;
	int status;
	size_t SB_size = sizeof(struct ext2_super_block);
	status = pread(img, &SB, SB_size, EXT2_SB_OFFSET);
	if (status < 0)
	{
		return -errno;
	}
	else if (inode_Read(img, &inode, inode_nr, &SB) < 0)
	{
		return -errno;
	}
	else if (copy_file(img, out, &SB, inode_nr) < 0)
	{
		return -errno;
	}

	return 0;
}
