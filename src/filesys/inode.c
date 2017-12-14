#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    //block_sector_t start;               /* First data sector. */
    /* Added */
    block_sector_t data_blocks[124];
    block_sector_t double_block;
    bool is_dir;
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
    //uint32_t unused[125];               /* Not used. */
  };

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode 
{
  struct list_elem elem;              /* Element in inode list. */
  block_sector_t sector;              /* Sector number of disk location. */
  int open_cnt;                       /* Number of openers. */
  bool removed;                       /* True if deleted, false otherwise. */
  int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
  struct inode_disk data;             /* Inode content. */
};

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);
  if (pos < inode->data.length)
  {
    /* original implemntation */
    //return inode->data.start + pos / BLOCK_SECTOR_SIZE;

    /* new implmentation */
    int index = pos / BLOCK_SECTOR_SIZE ; 
    if(index < 124)
    {
      return inode->data.data_blocks[index];
    }
    else
    {
      index = index - 124;
      int fst_index = index / 128;
      int snd_index = index % 128;

      block_sector_t fst_blocks[128];
      block_sector_t snd_blocks[128];

      block_read(fs_device, inode->data.double_block, &fst_blocks);
      
      block_read(fs_device, fst_blocks[fst_index], &snd_blocks);

      return snd_blocks[snd_index];

      //exit(-1);
    }
  }
  else
    return -1;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (bool is_dir,block_sector_t sector, off_t length)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      size_t sectors = bytes_to_sectors (length);
      disk_inode->is_dir = is_dir;
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
      /*
      if (free_map_allocate (sectors, &disk_inode->start)) 
        {
          block_write (fs_device, sector, disk_inode);
          if (sectors > 0) 
            {
              static char zeros[BLOCK_SECTOR_SIZE];
              size_t i;
              
              for (i = 0; i < sectors; i++) 
                block_write (fs_device, disk_inode->start + i, zeros);
            }
          success = true; 
        }*/
      if(1)
      {
        int num_data_blocks = sectors;
        if(num_data_blocks > 124)
          num_data_blocks = 124;
        int i=0;
        for(; i < num_data_blocks; i ++)
        {
          if(!free_map_allocate(1, & (disk_inode->data_blocks[i]) ))
          {
            free(disk_inode);
            return success;
          }
          static char zeros[BLOCK_SECTOR_SIZE];
          block_write (fs_device, disk_inode->data_blocks[i], zeros);
        }
       /* 
        i = 0;
        for(; i < sectors; i ++)
        {
          static char zeros[BLOCK_SECTOR_SIZE];
          block_write (fs_device, disk_inode->data_blocks[i], zeros);
        }*/
        if(sectors <= 124)
        {
          block_write(fs_device, sector, disk_inode);
          success = true;
          free(disk_inode);
          return success;
        }
      }
      if(sectors > 124)  // need double block 
      {
        int num_sectors = sectors - 124;
        int num_indirect_blocks = num_sectors / 128;
        int num_blocks_at_last = num_sectors % 128;
        
        if(num_blocks_at_last != 0)
          num_indirect_blocks ++;

        //printf("num_sectors : %d\n", num_sectors);
        //printf("num_indirect_blocks : %d\n", num_indirect_blocks);
        //printf("num_blocks_at_last : %d\n", num_blocks_at_last);
        //-----
        block_sector_t level_one[128];
        block_sector_t level_two[128];

        free_map_allocate(1, &disk_inode->double_block);
        //else
        //  block_read(fs_device, inode->blocks[inode->direct_index], &level_one);
            
        int lev_one_index = 0; 
        int lev_two_index = 0;
        static char zeros[BLOCK_SECTOR_SIZE];

        while (1)
        {
          if(num_indirect_blocks > 1)
          {
            free_map_allocate(1, &level_one[lev_one_index]);
            for( lev_two_index = 0; lev_two_index < 128; lev_two_index ++)
            {
              free_map_allocate(1, &level_two[lev_two_index]);
              block_write(fs_device, level_two[lev_two_index], zeros);
            }
            block_write(fs_device, level_one[lev_one_index++], &level_two);
            num_indirect_blocks --;
          }
          else if(num_indirect_blocks == 1)
          {
            free_map_allocate(1, &level_one[lev_one_index]);
            for( lev_two_index = 0; lev_two_index < num_blocks_at_last; lev_two_index ++)
            {
              free_map_allocate(1, &level_two[lev_two_index]);
              block_write(fs_device, level_two[lev_two_index], zeros);
            }
            block_write(fs_device, level_one[lev_one_index++], &level_two);
            num_indirect_blocks --;
          }
          else
          {
            block_write(fs_device, disk_inode->double_block, &level_one);
            break;
          }
        //----
        }
        block_write(fs_device, sector, disk_inode);
        success=true;
      }
      free (disk_inode);
    }
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  block_read (fs_device, inode->sector, &inode->data);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          free_map_release (inode->sector, 1);
          //free_map_release (inode->data.start,
          //                  bytes_to_sectors (inode->data.length)); 
          /* Added start free */
          int num_del_sectors = bytes_to_sectors(inode->data.length);
          if( num_del_sectors <= 124)
          { 
            int i=0;
            for( ; i < num_del_sectors ; i++)
            {
              free_map_release(inode->data.data_blocks[i], 1);
            }
          }
          else
          {
            int i=0;
            num_del_sectors = num_del_sectors - 124;
            int fst_num = num_del_sectors / 128;
            int snd_num = num_del_sectors % 128;
            for( ; i < 124 ; i++)
            {
              free_map_release(inode->data.data_blocks[i], 1);
            }

            free_map_release(inode->data.double_block, 1);
          }
        }

      free (inode); 
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Read full sector directly into caller's buffer. */
          block_read (fs_device, sector_idx, buffer + bytes_read);
        }
      else 
        {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          block_read (fs_device, sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);

  return bytes_read;
}
/* Added for project 4 */
bool
inode_grow(struct inode *inode, off_t size)
{
  struct inode_disk *disk_inode = &(inode->data);
  bool success = false;
  off_t cur_len = disk_inode -> length;
  ASSERT (size >= 0);
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);
  ASSERT (size + cur_len < 128*128*512 + 124 * 512);
  //disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
  {
    size_t sectors = bytes_to_sectors (cur_len);
    size_t sectors_needed = bytes_to_sectors (cur_len + size);
    disk_inode->length = cur_len + size;
    if(sectors_needed <= 124)
    {
      int num_data_blocks = sectors_needed;
      int i=sectors;
      for(; i < num_data_blocks; i ++)
      {
        if(disk_inode->data_blocks[i] == 0)
        {
        if(!free_map_allocate(1, & ( (&(inode->data))->data_blocks[i]) ))
        {
          free(disk_inode);
          return success;
        }
        static char zeros[BLOCK_SECTOR_SIZE];
        block_write (fs_device, disk_inode->data_blocks[i], zeros);
        }
      }
      //block_write(fs_device, inode->sector, disk_inode);
      success = true;
      //free(disk_inode);
      return success;
    }
    else  // need double block 
    {
      if(sectors <= 124)
      {

      }
      if(sectors > 124)
      {

      }
      /*
      int num_sectors = sectors - 124;
      int num_indirect_blocks = num_sectors / 128;
      int num_blocks_at_last = num_sectors % 128;
        
        if(num_blocks_at_last != 0)
          num_indirect_blocks ++;

        //printf("num_sectors : %d\n", num_sectors);
        //printf("num_indirect_blocks : %d\n", num_indirect_blocks);
        //printf("num_blocks_at_last : %d\n", num_blocks_at_last);
        //-----
        block_sector_t level_one[128];
        block_sector_t level_two[128];

        free_map_allocate(1, &disk_inode->double_block);
        //else
        //  block_read(fs_device, inode->blocks[inode->direct_index], &level_one);
            
        int lev_one_index = 0; 
        int lev_two_index = 0;
        static char zeros[BLOCK_SECTOR_SIZE];

        while (1)
        {
          if(num_indirect_blocks > 1)
          {
            free_map_allocate(1, &level_one[lev_one_index]);
            for( lev_two_index = 0; lev_two_index < 128; lev_two_index ++)
            {
              free_map_allocate(1, &level_two[lev_two_index]);
              block_write(fs_device, level_two[lev_two_index], zeros);
            }
            block_write(fs_device, level_one[lev_one_index++], &level_two);
            num_indirect_blocks --;
          }
          else if(num_indirect_blocks == 1)
          {
            free_map_allocate(1, &level_one[lev_one_index]);
            for( lev_two_index = 0; lev_two_index < num_blocks_at_last; lev_two_index ++)
            {
              free_map_allocate(1, &level_two[lev_two_index]);
              block_write(fs_device, level_two[lev_two_index], zeros);
            }
            block_write(fs_device, level_one[lev_one_index++], &level_two);
            num_indirect_blocks --;
          }
          else
          {
            block_write(fs_device, disk_inode->double_block, &level_one);
            break;
          }
        //----
        }
        block_write(fs_device, sector, disk_inode);
        success=true;
      }
      free (disk_inode);*/
    }
  }
  return success;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;

  /* If growth needed, then grow */
  if(inode_length(inode) - offset < size)
  {
    if( ! inode_grow(inode, size - (inode_length(inode) - offset)) )
    {
      return 0;
    }
  }
  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Write full sector directly to disk. */
          block_write (fs_device, sector_idx, buffer + bytes_written);
        }
      else 
        {
          /* We need a bounce buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
          if (sector_ofs > 0 || chunk_size < sector_left) 
            block_read (fs_device, sector_idx, bounce);
          else
            memset (bounce, 0, BLOCK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          block_write (fs_device, sector_idx, bounce);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  free (bounce);

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}

