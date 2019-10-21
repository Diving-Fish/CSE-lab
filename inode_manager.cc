#include "inode_manager.h"
#include <ctime>

// disk layer -----------------------------------------

disk::disk()
{
  bzero(blocks, sizeof(blocks));
}

void
disk::read_block(blockid_t id, char *buf)
{
  if (id < 0 || id >= BLOCK_NUM || buf == NULL)
    return;
  memcpy(buf, blocks[id], BLOCK_SIZE);
}

void
disk::write_block(blockid_t id, const char *buf, int size)
{
  printf("\t  bm: writing block %u\n", id);
  if (id < 0 || id >= BLOCK_NUM || buf == NULL) {
    printf("error occured.\n");
    return;
  }
  memset(blocks[id], 0, size);
  memcpy(blocks[id], buf, size);
  printf("\t  bm: written block %u\n", id);
}

// block layer -----------------------------------------

// Allocate a free disk block.
blockid_t
block_manager::alloc_block()
{
  /*
   * your code goes here.
   * note: you should mark the corresponding bit in block bitmap when alloc.
   * you need to think about which block you can start to be allocated.
   */
  blockid_t start = IBLOCK(INODE_NUM, sb.nblocks) + 1;
  for (blockid_t off = next_block; off < BLOCK_NUM; off++) {
    if (using_blocks[start + off] == 0) {
      using_blocks[start + off] = 1;
      next_block = off + 1;
      return start + off;
    }
  }
  for (blockid_t off = 0; off < next_block; off++) {
    if (using_blocks[start + off] == 0) {
      using_blocks[start + off] = 1;
      next_block = off + 1;
      return start + off;
    }
  }
  printf("bm: blocks are ran out. exit.");
  exit(0);
  return 0;
}

void
block_manager::free_block(uint32_t id)
{
  /* 
   * your code goes here.
   * note: you should unmark the corresponding bit in the block bitmap when free.
   */
  using_blocks[id] = 0;
  return;
}

// The layout of disk should be like this:
// |<-sb->|<-free block bitmap->|<-inode table->|<-data->|
block_manager::block_manager()
{
  d = new disk();
  next_block = IBLOCK(INODE_NUM, sb.nblocks) + 1;
  // format the disk
  sb.size = BLOCK_SIZE * BLOCK_NUM;
  sb.nblocks = BLOCK_NUM;
  sb.ninodes = INODE_NUM;

}

void
block_manager::read_block(uint32_t id, char *buf)
{
  d->read_block(id, buf);
}

void
block_manager::write_block(uint32_t id, const char *buf, int size = BLOCK_SIZE)
{
  d->write_block(id, buf, size);
}

// inode layer -----------------------------------------

inode_manager::inode_manager()
{
  bm = new block_manager();
  next_inum = 1;
  uint32_t root_dir = alloc_inode(extent_protocol::T_DIR);
  if (root_dir != 1) {
    printf("\tim: error! alloc first inode %d, should be 1\n", root_dir);
    exit(0);
  }
}

/* Create a new file.
 * Return its inum. */
uint32_t
inode_manager::alloc_inode(uint32_t type)
{
  /* 
   * your code goes here.
   * note: the normal inode block should begin from the 2nd inode block.
   * the 1st is used for root_dir, see inode_manager::inode_manager().
   */
  for (uint32_t id = next_inum; id < INODE_NUM; id++) {
    inode_t *node = get_inode(id);
    if (node == NULL || node->type == 0) {
      node = (inode_t *)malloc(sizeof(inode_t));
      node->type = type;
      node->size = 0;
      node->atime = (uint) time(0);
      node->mtime = (uint) time(0);
      node->ctime = (uint) time(0);
      put_inode(id, node);
      next_inum = id + 1;
      free(node);
      return id;
    } else {
      free(node);
    }
  }

  for (uint32_t id = 1; id < next_inum; id++) {
    inode_t *node = get_inode(id);
    if (node == NULL || node->type == 0) {
      node = (inode_t *)malloc(sizeof(inode_t));
      node->type = type;
      node->size = 0;
      node->atime = (uint) time(0);
      node->mtime = (uint) time(0);
      node->ctime = (uint) time(0);
      put_inode(id, node);
      next_inum = id + 1;
      free(node);
      return id;
    } else {
      free(node);
    }
  }
  return 0;
}

void
inode_manager::free_inode(uint32_t inum)
{
  /* 
   * your code goes here.
   * note: you need to check if the inode is already a freed one;
   * if not, clear it, and remember to write back to disk.
   */
  inode_t *node = get_inode(inum);
  if (node != NULL) {
    node->type = 0;
    put_inode(inum, node);
  }
  free(node);
  return;
}


/* Return an inode structure by inum, NULL otherwise.
 * Caller should release the memory. */
struct inode* 
inode_manager::get_inode(uint32_t inum)
{
  struct inode *ino, *ino_disk;
  char buf[BLOCK_SIZE];

  printf("\tim: get_inode %d\n", inum);

  if (inum < 0 || inum >= INODE_NUM) {
    printf("\tim: inum out of range\n");
    return NULL;
  }
  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  // printf("%s:%d\n", __FILE__, __LINE__);
  ino_disk = (struct inode*)buf + inum%IPB;
  if (ino_disk->type == 0) {
    printf("\tim: inode not exist\n");
    return NULL;
  }
  ino = (struct inode*)malloc(sizeof(struct inode));
  *ino = *ino_disk;
  return ino;
}

void
inode_manager::put_inode(uint32_t inum, struct inode *ino)
{
  char buf[BLOCK_SIZE];
  struct inode *ino_disk;
  printf("\tim: put_inode %d\n", inum);
  if (ino == NULL)
    return;
  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  ino_disk = (struct inode *) buf + inum % IPB;
  *ino_disk = *ino;
  bm->write_block(IBLOCK(inum, bm->sb.nblocks), buf);
}

#define MIN(a,b) ((a)<(b) ? (a) : (b))

/* Get all the data of a file by inum. 
 * Return alloced data, should be freed by caller. */
void
inode_manager::read_file(uint32_t inum, char **buf_out, int *size)
{
  /*
   * your code goes here.
   * note: read blocks related to inode number inum,
   * and copy them to buf_Out
   */
  printf("  im: readfile %u\n", inum);
  inode_t *node = get_inode(inum);
  int blocks = node->size == 0 ? 0 : NBLOCKS(node->size);
  char *buf = (char *) malloc(BLOCK_SIZE * blocks);
  for (int i = 0; i < blocks; i++) {
    char block_buf[BLOCK_SIZE];
    bm->read_block(get_block_id(node, i), block_buf);
    int size = (i == blocks - 1) ? node->size - i * BLOCK_SIZE : BLOCK_SIZE;
    memcpy(buf + i * BLOCK_SIZE, block_buf, size);
  }
  *buf_out = buf;
  *size = node->size;
  node->atime = (uint) time(0);
  put_inode(inum, node);
  free(node);
  return;
}

/* alloc/free blocks if needed */
void
inode_manager::write_file(uint32_t inum, const char *buf, int size)
{
  /*
   * your code goes here.
   * note: write buf to blocks of inode inum.
   * you need to consider the situation when the size of buf 
   * is larger or smaller than the size of original inode
   */
  printf("  im: writefile %u\n", inum);
  inode_t *node = get_inode(inum);
  printf("1\n");
  int old_blocks = node->size == 0 ? 0 : NBLOCKS(node->size);
  int new_blocks = size == 0 ? 0 : NBLOCKS(size);
  node->size = size;
  printf("2\n");
  if (old_blocks < new_blocks) {
    for (int index = 0; index < new_blocks - old_blocks; index++) {
      blockid_t id = bm->alloc_block();
      set_block_id(node, old_blocks + index, id);
    }
  } else if (old_blocks > new_blocks) {
    for (int index = 0; index < old_blocks - new_blocks; index++) {
      bm->free_block(get_block_id(node, new_blocks + index));
    }
  }
  printf("3\n");
  for (int index = 0; index < new_blocks; index++) {
    int bsize = (index == new_blocks - 1) ? (size - index * BLOCK_SIZE) : BLOCK_SIZE;
    bm->write_block(get_block_id(node, index), buf + BLOCK_SIZE * index, bsize);
  }
  printf("4\n");
  node->atime = (uint) time(0);
  node->mtime = (uint) time(0);
  node->ctime = (uint) time(0);
  printf("5\n");
  put_inode(inum, node);
  printf("6\n");
  free(node);
  printf("7\n");
  return;
}

void
inode_manager::getattr(uint32_t inum, extent_protocol::attr &a)
{
  /*
   * your code goes here.
   * note: get the attributes of inode inum.
   * you can refer to "struct attr" in extent_protocol.h
   */
  inode_t *node = get_inode(inum);
  if (!node) return;
  a.type = node->type;
  a.atime = node->atime;
  a.mtime = node->mtime;
  a.ctime = node->ctime;
  a.size = node->size;
  free(node);
  return;
}

void
inode_manager::remove_file(uint32_t inum)
{
  /*
   * your code goes here
   * note: you need to consider about both the data block and inode of the file
   */
  inode_t *node = get_inode(inum);
  int blocks = node->size == 0 ? 0 : NBLOCKS(node->size);
  for (int i = 0; i < blocks; i++) {
    bm->free_block(get_block_id(node, i));
  }
  free_inode(inum);
  free(node);
  return;
}

blockid_t 
inode_manager::get_block_id(inode_t *node, int index) {
  if (index >= NDIRECT) {
    blockid_t id = node->blocks[NDIRECT];
    char block_buf[BLOCK_SIZE];
    bm->read_block(id, block_buf);
    blockid_t *block_ptr = (blockid_t *)block_buf;
    block_ptr += index - NDIRECT;
    return *block_ptr;
  }
  return node->blocks[index];
}

void 
inode_manager::set_block_id(inode_t *node, int index, blockid_t id) {
  if (index >= NDIRECT) {
    if (node->blocks[NDIRECT] == 0) {
      node->blocks[NDIRECT] = bm->alloc_block();
    }
    char block_buf[BLOCK_SIZE];
    bm->read_block(node->blocks[NDIRECT], block_buf);
    blockid_t *block_ptr = (blockid_t *) (block_buf + sizeof(blockid_t) * (index - NDIRECT));
    *block_ptr = id;
    bm->write_block(node->blocks[NDIRECT], block_buf);
  }
  node->blocks[index] = id;
}
