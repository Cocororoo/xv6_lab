// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"
#define NBUCKETS 13

struct {
  struct spinlock lock[NBUCKETS];
  struct buf buf[NBUF];
  // Linked list of all buffers, through prev/next.
  // head.next is most recently used.
  //struct buf head;
  struct buf head[NBUCKETS]; //每个哈希队列一个linked list及一个lock
  struct spinlock global_lock;
} bcache;

unsigned int hash_function(unsigned int dev, unsigned int blockno) {
    unsigned int hash = 0;
    hash = 31 * hash + dev;
    hash = 31 * hash + blockno;
    return hash % NBUCKETS; // NBUCKETS 是哈希桶的数量
}

void
binit(void)
{
  struct buf *b;

  // printf("=============binit: start============\n");
  initlock(&bcache.global_lock, "bcache_global_lock");

  // Create linked list of buffers
  for (int i = 0; i < NBUCKETS; i++) 
  {
    char lock_name[10] = "bcache";
    snprintf(lock_name, sizeof(lock_name), "bcache%d", i);
    // printf("===initlock %d: lockname ===> %s\n", i, lock_name);
    initlock(&bcache.lock[i], lock_name);
    bcache.head[i].prev = &bcache.head[i];
    bcache.head[i].next = &bcache.head[i];
  }

  // 将缓冲区平均分配到各个桶中
    for (b = bcache.buf; b < bcache.buf + NBUF; b++) {
        uint index = (b - bcache.buf) % NBUCKETS;
        // printf("===binit: index ===> %d\n", index);
        b->next = bcache.head[index].next;
        b->prev = &bcache.head[index];
        initsleeplock(&b->lock, "buffer");
        bcache.head[index].next->prev = b;
        bcache.head[index].next = b;
    }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno) {
  struct buf *b;
  uint index = hash_function(dev, blockno);

  acquire(&bcache.lock[index]);

  // 查找当前哈希桶
  for(b = bcache.head[index].next; b != &bcache.head[index]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock[index]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // 尝试在当前哈希桶中找到空闲缓冲区
  for(b = bcache.head[index].prev; b != &bcache.head[index]; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.lock[index]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  release(&bcache.lock[index]);

  // 跨哈希桶操作
  acquire(&bcache.global_lock);
  for (int i = 1; i < NBUCKETS; i++) {
    uint idx = (index + i) % NBUCKETS;
    acquire(&bcache.lock[idx]);

    for (b = bcache.head[idx].next; b != &bcache.head[idx]; b = b->next) {
      if (b->refcnt == 0) {
        // 迁移缓冲区
        b->prev->next = b->next;
        b->next->prev = b->prev;
        
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;

        b->next = bcache.head[index].next;
        b->prev = &bcache.head[index];
        bcache.head[index].next->prev = b;
        bcache.head[index].next = b;

        release(&bcache.lock[idx]);
        acquiresleep(&b->lock);
        release(&bcache.global_lock);
        return b;
      }
    }
    release(&bcache.lock[idx]);
  }
  release(&bcache.global_lock);

  panic("bget: no buffers");
}


// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  uint index = hash_function(b->dev, b->blockno);
  acquire(&bcache.lock[index]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.head[index].next;
    b->prev = &bcache.head[index];
    bcache.head[index].next->prev = b;
    bcache.head[index].next = b;
  }
  
  release(&bcache.lock[index]);
}

void
bpin(struct buf *b) {
  uint index = hash_function(b->dev, b->blockno);
  acquire(&bcache.lock[index]);
  b->refcnt++;
  release(&bcache.lock[index]);
}

void
bunpin(struct buf *b) {
  uint index = hash_function(b->dev, b->blockno);
  acquire(&bcache.lock[index]);
  b->refcnt--;
  release(&bcache.lock[index]);
}