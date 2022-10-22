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

#define NBUCKETS 13 // number of bcache hash buckets

struct
{
  struct spinlock lock[NBUCKETS];
  struct buf buf[NBUF];
  struct spinlock lockall;

  // 每个桶共用lock
  struct buf buckets[NBUCKETS];
} bcache;

void binit(void)
{
  struct buf *b;

  initlock(&bcache.lockall, "bcache_all");

  for (int bid = 0; bid < NBUCKETS; bid++)
  {
    initlock(&bcache.lock[bid], "bcache");

    bcache.buckets[bid]
        .prev = &bcache.buckets[bid];
    bcache.buckets[bid].next = &bcache.buckets[bid];
  }

  for (int i = 0; i < NBUF; i++)
  {
    b = &bcache.buf[i];
    int bid = i % NBUCKETS;
    b->next = bcache.buckets[bid].next;
    b->prev = &bcache.buckets[bid];
    initsleeplock(&b->lock, "buffer");
    bcache.buckets[bid].next->prev = b;
    bcache.buckets[bid].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf *
bget(uint dev, uint blockno)
{
  struct buf *b;

  int mybid = blockno % NBUCKETS;
  acquire(&bcache.lock[mybid]);

  // Is the block already cached?
  for (b = bcache.buckets[mybid].next; b != &bcache.buckets[mybid]; b = b->next)
  {
    if (b->dev == dev && b->blockno == blockno)
    {
      b->refcnt++;
      release(&bcache.lock[mybid]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for (b = bcache.buckets[mybid].prev; b != &bcache.buckets[mybid]; b = b->prev)
  {
    if (b->refcnt == 0)
    {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.lock[mybid]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.lock[mybid]);

  // If no unused buffer in this bucket
  // Steal one from other buckets
  for (int i = 1; i < NBUCKETS; i++)
  {
    int bid = (mybid + i) % NBUCKETS;

    // 为了消除死锁，按照升序请求占用
    if (bid < mybid)
    {
      acquire(&bcache.lock[bid]);
      acquire(&bcache.lock[mybid]);
    }
    else
    {
      acquire(&bcache.lock[mybid]);
      acquire(&bcache.lock[bid]);
    }

    for (b = bcache.buckets[bid].prev; b != &bcache.buckets[bid]; b = b->prev)
    {
      if (b->refcnt == 0)
      {
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;

        // move b(bucket[bid]) to bucket[mybid]
        b->prev->next = b->next;
        b->next->prev = b->prev;
        b->next = bcache.buckets[mybid].next;
        b->prev = &bcache.buckets[mybid];
        bcache.buckets[mybid].next->prev = b;
        bcache.buckets[mybid].next = b;
        release(&bcache.lock[mybid]);
        release(&bcache.lock[bid]);
        acquiresleep(&b->lock);
        return b;
      }
    }
    release(&bcache.lock[mybid]);
    release(&bcache.lock[bid]);
  }
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf *
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if (!b->valid)
  {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void bwrite(struct buf *b)
{
  if (!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void brelse(struct buf *b)
{
  if (!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int bid = b->blockno % NBUCKETS;
  acquire(&bcache.lock[bid]);
  b->refcnt--;
  if (b->refcnt == 0)
  {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.buckets[bid].next;
    b->prev = &bcache.buckets[bid];
    bcache.buckets[bid].next->prev = b;
    bcache.buckets[bid].next = b;
  }

  release(&bcache.lock[bid]);
}

void bpin(struct buf *b)
{
  int bid = b->blockno % NBUCKETS;
  acquire(&bcache.lock[bid]);
  b->refcnt++;
  release(&bcache.lock[bid]);
}

void bunpin(struct buf *b)
{
  int bid = b->blockno % NBUCKETS;
  acquire(&bcache.lock[bid]);
  b->refcnt--;
  release(&bcache.lock[bid]);
}
