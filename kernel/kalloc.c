// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end, int cpu_id);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct kmem {
  struct spinlock lock;
  struct run *freelist;
};

uint cpus_num = NCPU;

struct kmem kmem_list[NCPU];

char lock_name[10] = "kmem";

void
kinit()
{
  // printf("===============kinit start==================\n");
  uint64 start = PGROUNDUP((uint64)end);
  uint64 end = PHYSTOP;
  uint64 len = (end - start) / cpus_num;
  for (int i = 0; i < cpus_num; i++) {
    // printf("======第%d个cpu初始化中========", i);
    kmem_list[i].freelist = 0;
    snprintf(lock_name, sizeof(lock_name), "kmem%d", i);
    // printf("lock_name: %s\n", lock_name);
    initlock(&kmem_list[i].lock, lock_name);
    freerange((void*)(start + i * len), (void*)(start + (i + 1) * len), i);
  }
}



void
freerange(void *pa_start, void *pa_end, int cpu_id)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
  {   
    // kfree(p);
    struct run *r;

    if(((uint64)p % PGSIZE) != 0 || (char*)p < end || (uint64)p >= PHYSTOP)
      panic("kfree");

    // Fill with junk to catch dangling refs.
    memset(p, 1, PGSIZE);

    r = (struct run*)p;

    acquire(&kmem_list[cpu_id].lock);
    r->next = kmem_list[cpu_id].freelist;
    kmem_list[cpu_id].freelist = r;
    release(&kmem_list[cpu_id].lock);
  }
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  int cpu_id = cpuid();
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem_list[cpu_id].lock);
  r->next = kmem_list[cpu_id].freelist;
  kmem_list[cpu_id].freelist = r;
  release(&kmem_list[cpu_id].lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  int cpu_id = cpuid();
  // printf("cpu_id: %d正在申请内存........\n", cpu_id);
  struct run *r;

  acquire(&kmem_list[cpu_id].lock);
  r = kmem_list[cpu_id].freelist;
  if(r)
    kmem_list[cpu_id].freelist = r->next;
  release(&kmem_list[cpu_id].lock);

  if(!r) {
    // try to get from other cpus
    for (int i = 0; i < cpus_num; i++) {
      if (i == cpu_id) continue;
      acquire(&kmem_list[i].lock);
      r = kmem_list[i].freelist;
      if(r) {
        kmem_list[i].freelist = r->next;
        release(&kmem_list[i].lock);
        break;
      }
      release(&kmem_list[i].lock);
    }
  }

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
