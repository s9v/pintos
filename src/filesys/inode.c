#include "devices/disk.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include <stdbool.h>
#include "filesys/cache.h"
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "filesys/inode.h"

/* Identifies an inode. */
#define INODE_MAGIC   0x494e4f44
#define READDIR_MAX_LEN   14

/* On-disk inode.
   Must be exactly DISK_SECTOR_SIZE bytes long. */
// struct inode_disk
//   {
//     disk_sector_t start;                /* First data sector. */
//     off_t length;                       /* File size in bytes. */
//     unsigned magic;                     /* Magic number. */
//     uint32_t unused[125];               /* Not used. */
//   };

#define BLOCKS1_LEN   ((int) (400 / sizeof (disk_sector_t)))
#define BLOCKS1_SIZE  ((int) (400))
#define BLOCKS2_LEN   ((int) (DISK_SECTOR_SIZE / sizeof (disk_sector_t)))   // number of items
#define BLOCKS2_SIZE  ((int) (DISK_SECTOR_SIZE))                            // total bytes

#define SIZEOF_INODE_DISK (sizeof(off_t) +                           \
                           sizeof(bool) +                            \
                           sizeof(char) * (READDIR_MAX_LEN + 1) +    \
                           sizeof(disk_sector_t) * BLOCKS1_LEN +     \
                           sizeof(disk_sector_t) +                   \
                           sizeof(disk_sector_t) +                   \
                           sizeof(unsigned))

struct inode_disk {
  off_t length;                         /* File size in bytes. */
  bool is_dir;
  char name[READDIR_MAX_LEN + 1];
  disk_sector_t blocks1[BLOCKS1_LEN];

  disk_sector_t self;    // .
  disk_sector_t parent;  // ..

  unsigned magic;                       /* Magic number. */
  uint8_t unused[DISK_SECTOR_SIZE - SIZEOF_INODE_DISK];
};

struct blocks1 {
  disk_sector_t blocks2[BLOCKS2_LEN];
};


/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
// static inline size_t
// bytes_to_sectors (off_t size)
// {
//   return DIV_ROUND_UP (size, DISK_SECTOR_SIZE);
// }

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    disk_sector_t sector;               /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
  };

/* Returns the disk sector that contains byte offset POS within
   INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
// static disk_sector_t
// byte_to_sector (const struct inode *inode, off_t pos) 
// {
//   ASSERT (inode != NULL);
//   if (pos < inode->data.length)
//     return inode->data.start + pos / DISK_SECTOR_SIZE;
//   else
//     return -1;
// }


/* HELPERS */


void inode_release_blocks (struct inode *inode);


/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);

  ASSERT (sizeof(struct inode_disk) == DISK_SECTOR_SIZE);
  ASSERT (sizeof(struct blocks1) == DISK_SECTOR_SIZE);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   disk.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (disk_sector_t sector, off_t initial_size, bool is_dir)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == DISK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL) {
    /* Initialize disk_inode */
    
    // inode name
    //~ size_t name_len = strlen (name_);
    //~ ASSERT (name_len <= READDIR_MAX_LEN);
    //~ memcpy (disk_inode->name, name_, name_len + 1);

    disk_inode->length = initial_size;
    disk_inode->is_dir = is_dir;
    memset (disk_inode->blocks1, -1, BLOCKS1_SIZE); // TODO: check this resets items to "invalid" (disk_sector_t) -1
    disk_inode->magic = INODE_MAGIC;

    /* Write to disk */
    //~ disk_sector_t sector;
    //~ success = free_map_allocate (1, &sector);
    //~ if (success)
    success = true;
    cache_write (filesys_disk, sector, disk_inode);

    free (disk_inode);
  }

  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (disk_sector_t sector) 
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
  // disk_read (filesys_disk, inode->sector, &inode->data);
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
disk_sector_t
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
        inode_release_blocks (inode);

      free (inode);
    }
}

/* Reads the on-disk inode and releases all data blocks */
void
inode_release_blocks (struct inode *inode) {
  // Read on-disk inode
  struct inode_disk *disk_inode = (struct inode_disk *) malloc (sizeof (struct inode_disk));
  cache_read (filesys_disk, inode->sector, disk_inode);
  
  // Memory for level 2 blocks
  struct blocks1 *blocks1 = (struct blocks1 *) malloc (sizeof (struct blocks1));

  int i;
  off_t length;
  for (i = 0, length = 0; length < disk_inode->length && i < BLOCKS1_LEN; i++) {
    disk_sector_t sector1 = disk_inode->blocks1[i];

    if (sector1 == (disk_sector_t) -1)
      continue;

    // Read level 2 blocks
    cache_read (filesys_disk, sector1, blocks1);

    int j;
    for (j = 0; length < disk_inode->length && j < BLOCKS2_LEN; j++) {
      disk_sector_t sector2 = blocks1->blocks2[j];
      
      if (sector2 != (disk_sector_t) -1)
        free_map_release (sector2, 1);

      length += DISK_SECTOR_SIZE;
    }

    free_map_release (sector1, 1);
  }
  
  free_map_release (inode->sector, 1);
  free (disk_inode);
  free (blocks1);
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

  // Read on-disk inode
  struct inode_disk *disk_inode = (struct inode_disk *) malloc (sizeof (struct inode_disk));
  cache_read (filesys_disk, inode->sector, disk_inode);

  if (offset >= disk_inode->length)
    return 0;

  if (offset + size > disk_inode->length)
    size = disk_inode->length - offset;
  
  // Memory for level 2 blocks
  struct blocks1 *blocks1 = (struct blocks1 *) malloc (sizeof (struct blocks1));
  uint8_t *data_block = (uint8_t *) malloc (DISK_SECTOR_SIZE);

 /* Start reading */

  int i = offset / (BLOCKS2_LEN * DISK_SECTOR_SIZE);
  for (; size > 0 && i < BLOCKS1_LEN; i++) {
    disk_sector_t sector1 = disk_inode->blocks1[i];

    if (sector1 == (disk_sector_t) -1) {
      int blocks1_ofs = offset % (BLOCKS2_LEN * DISK_SECTOR_SIZE);
      int blocks1_left = BLOCKS2_LEN * DISK_SECTOR_SIZE - blocks1_ofs;
      int chunk_size = blocks1_left < size ? blocks1_left : size;

      memset (buffer + bytes_read, 0, chunk_size);

      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    } else {
      cache_read (filesys_disk, sector1, blocks1);

      int blocks1_ofs = offset % (BLOCKS2_LEN * DISK_SECTOR_SIZE);
      int j = blocks1_ofs / DISK_SECTOR_SIZE;
      for (; size > 0 && j < BLOCKS2_LEN; j++) {
        disk_sector_t sector2 = blocks1->blocks2[j];

        int sector_ofs = offset % DISK_SECTOR_SIZE;
        int sector_left = DISK_SECTOR_SIZE - sector_ofs;
        int chunk_size = sector_left < size ? sector_left : size;
        
        if (sector2 == (disk_sector_t) -1) {
          memset (data_block, 0, DISK_SECTOR_SIZE);
        } else {
          cache_read (filesys_disk, sector2, data_block);
        }

        memcpy (buffer + bytes_read, data_block + sector_ofs, chunk_size);

        size -= chunk_size;
        offset += chunk_size;
        bytes_read += chunk_size;
      }
    }
  }

  free (disk_inode);
  free (blocks1);
  free (data_block);

  return bytes_read;

  /* ------------- */

//  while (size > 0) 
//    {
//      /* Disk sector to read, starting byte offset within sector. */
//      disk_sector_t sector_idx = byte_to_sector (inode, offset);
//      int sector_ofs = offset % DISK_SECTOR_SIZE;
//
//      /* Bytes left in inode, bytes left in sector, lesser of the two. */
//      off_t inode_left = inode_length (inode) - offset;
//      int sector_left = DISK_SECTOR_SIZE - sector_ofs;
//      int min_left = inode_left < sector_left ? inode_left : sector_left;
//
//      /* Number of bytes to actually copy out of this sector. */
//      int chunk_size = size < min_left ? size : min_left;
//      if (chunk_size <= 0)
//        break;
//
//      if (sector_ofs == 0 && chunk_size == DISK_SECTOR_SIZE) 
//        {
//          /* Read full sector directly into caller's buffer. */
//          disk_read (filesys_disk, sector_idx, buffer + bytes_read); 
//        }
//      else 
//        {
//          /* Read sector into bounce buffer, then partially copy
//             into caller's buffer. */
//          if (bounce == NULL) 
//            {
//              bounce = malloc (DISK_SECTOR_SIZE);
//              if (bounce == NULL)
//                break;
//            }
//          disk_read (filesys_disk, sector_idx, bounce);
//          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
//        }
//      
//      /* Advance. */
//      size -= chunk_size;
//      offset += chunk_size;
//      bytes_read += chunk_size;
//    }
//  free (bounce);
//
//  return bytes_read;
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

  if (inode->deny_write_cnt)
    return 0;

  // Read on-disk inode
  struct inode_disk *disk_inode = (struct inode_disk *) malloc (sizeof (struct inode_disk));
  cache_read (filesys_disk, inode->sector, disk_inode);

  if (disk_inode->length < offset + size)
    disk_inode->length = offset + size;
  
  // Memory for level 2 blocks
  struct blocks1 *blocks1 = (struct blocks1 *) malloc (sizeof (struct blocks1));
  uint8_t *data_block = (uint8_t *) malloc (DISK_SECTOR_SIZE);

  /* Start writing */

  int i = offset / (BLOCKS2_LEN * DISK_SECTOR_SIZE);
  for (; size > 0 && i < BLOCKS1_LEN; i++) {
    disk_sector_t sector1 = disk_inode->blocks1[i];

    if (sector1 == (disk_sector_t) -1) {
      if (!free_map_allocate (1, &sector1)) {
        // size = 0;
        // break;
        PANIC ("can't free_map_allocate (sector1)");
      }

      disk_inode->blocks1[i] = sector1;
      memset (blocks1->blocks2, -1, DISK_SECTOR_SIZE);
      cache_write (filesys_disk, sector1, blocks1);
    } else {
      cache_read (filesys_disk, sector1, blocks1);
    }

    int blocks1_ofs = offset % (BLOCKS2_LEN * DISK_SECTOR_SIZE);
    int j = blocks1_ofs / DISK_SECTOR_SIZE;
    for (; size > 0 && j < BLOCKS2_LEN; j++) {
      disk_sector_t sector2 = blocks1->blocks2[j];

      if (sector2 == (disk_sector_t) -1) {
        if (!free_map_allocate (1, &sector2)) {
          // size = 0;
          // break;
          PANIC ("can't free_map_allocate (sector2)");
        }

        blocks1->blocks2[j] = sector2;
        memset (data_block, 0, DISK_SECTOR_SIZE);
        cache_write (filesys_disk, sector2, data_block);
      } else {
        cache_read (filesys_disk, sector2, data_block);
      }

      int sector_ofs = offset % DISK_SECTOR_SIZE;
      int sector_left = DISK_SECTOR_SIZE - sector_ofs;
      int chunk_size = size < sector_left ? size : sector_left;

      memcpy (data_block + sector_ofs, buffer + bytes_written, chunk_size);
      cache_write (filesys_disk, sector2, data_block);

      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }

    cache_write (filesys_disk, sector1, blocks1);
  }

  cache_write (filesys_disk, inode->sector, disk_inode);
  free (disk_inode);
  free (blocks1);
  free (data_block);

  return bytes_written;
}
  /* -------------- */

//  uint8_t *bounce = NULL;
//
//  if (inode->deny_write_cnt)
//    return 0;
//
//  while (size > 0) 
//    {
//      /* Sector to write, starting byte offset within sector. */
//      disk_sector_t sector_idx = byte_to_sector (inode, offset);
//      int sector_ofs = offset % DISK_SECTOR_SIZE;
//
//      /* Bytes left in inode, bytes left in sector, lesser of the two. */
//      off_t inode_left = inode_length (inode) - offset;
//      int sector_left = DISK_SECTOR_SIZE - sector_ofs;
//      int min_left = inode_left < sector_left ? inode_left : sector_left;
//
//      /* Number of bytes to actually write into this sector. */
//      int chunk_size = size < min_left ? size : min_left;
//      if (chunk_size <= 0)
//        break;
//
//      if (sector_ofs == 0 && chunk_size == DISK_SECTOR_SIZE) 
//        {
//          /* Write full sector directly to disk. */
//          disk_write (filesys_disk, sector_idx, buffer + bytes_written); 
//        }
//      else 
//        {
//          /* We need a bounce buffer. */
//          if (bounce == NULL) 
//            {
//              bounce = malloc (DISK_SECTOR_SIZE);
//              if (bounce == NULL)
//                break;
//            }
//
//          /* If the sector contains data before or after the chunk
//             we're writing, then we need to read in the sector
//             first.  Otherwise we start with a sector of all zeros. */
//          if (sector_ofs > 0 || chunk_size < sector_left) 
//            disk_read (filesys_disk, sector_idx, bounce);
//          else
//            memset (bounce, 0, DISK_SECTOR_SIZE);
//          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
//          disk_write (filesys_disk, sector_idx, bounce); 
//        }
//
//      /* Advance. */
//      size -= chunk_size;
//      offset += chunk_size;
//      bytes_written += chunk_size;
//    }
//  free (bounce);
//
//  return bytes_written;


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
  struct inode_disk *disk_inode = (struct inode_disk *) malloc (sizeof (struct inode_disk));
  cache_read (filesys_disk, inode->sector, disk_inode);
  ASSERT (disk_inode->magic == INODE_MAGIC);
  return disk_inode->length;
}
