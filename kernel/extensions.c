#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

int
vmexclusive(struct proc *p)
{
  acquire(&p->vm->lock);
  int exclusive = p->vm->refs == 1;
  release(&p->vm->lock);
  return exclusive;
}

// Faults from user execution and copyin/copyout use the same permission checks.
// No sleeping operations occur under vm->lock. An existing leaf is never replaced.
uint64
vmfault(pagetable_t pt, uint64 addr, int write)
{
  struct proc *p = myproc();
  if(!p || pt != p->pagetable || addr >= MAXVA)
    return 0;
  uint64 va = PGROUNDDOWN(addr), pa = 0;
  acquire(&p->vm->lock);
  int prot = 0;
  if(addr >= p->heapbase && addr < p->sz)
    prot = PROT_READ | PROT_WRITE;
  for(int i = 0; i < NVMA; i++){
    struct vma *v = &p->vm->areas[i];
    if(v->length && addr >= v->start && addr - v->start < v->length)
      prot = v->prot;
  }
  if(!(prot & PROT_READ) || (write && !(prot & PROT_WRITE)))
    goto done;
  pte_t *pte = walk(pt, va, 0);
  if(pte && (*pte & PTE_V)){
    // Another thread may have satisfied the fault while we waited.
    if((*pte & PTE_U) && (*pte & PTE_R) && (!write || (*pte & PTE_W)))
      pa = PTE2PA(*pte);
    goto done;
  }
  char *mem = kalloc();
  if(!mem)
    goto done;
  memset(mem, 0, PGSIZE);
  int flags = PTE_U | PTE_R | ((prot & PROT_WRITE) ? PTE_W : 0);
  if(mappages(pt, va, PGSIZE, (uint64)mem, flags) < 0){
    kfree(mem);
    goto done;
  }
  pa = (uint64)mem;
  __sync_fetch_and_add(&p->page_faults, 1);
done:
  release(&p->vm->lock);
  return pa;
}

uint64
vmaccess(pagetable_t pt, uint64 va, int write)
{
  if(va >= MAXVA)
    return 0;
  struct proc *p = myproc();
  int shared = p && pt == p->pagetable;
  if(shared) acquire(&p->vm->lock);
  pte_t *pte = walk(pt, va, 0);
  int missing = !pte || !(*pte & PTE_V);
  uint64 pa = 0;
  if(!missing && (*pte & PTE_U) && (*pte & PTE_R) && (!write || (*pte & PTE_W)))
    pa = PTE2PA(*pte);
  if(shared) release(&p->vm->lock);
  if(missing)
    return vmfault(pt, va, write);
  return pa;
}

uint64
dommap(uint64 length, int prot)
{
  struct proc *p = myproc();
  if(!length || length > MMAPEND-MMAPBASE ||
     !(prot & PROT_READ) || (prot & ~(PROT_READ|PROT_WRITE)))
    return -1;
  length = PGROUNDUP(length);
  acquire(&p->vm->lock);
  if(p->vm->refs != 1){
    release(&p->vm->lock);
    return -1;
  }
  int slot = -1;
  for(int i = 0; i < NVMA; i++)
    if(!p->vm->areas[i].length){ slot = i; break; }
  if(slot < 0){ release(&p->vm->lock); return -1; }
  // First-fit allocation with hole reuse; at most NVMA overlaps to skip.
  uint64 start = MMAPBASE;
  for(int attempt = 0; attempt <= NVMA; attempt++){
    int overlap = 0;
    for(int i = 0; i < NVMA; i++){
      struct vma *v = &p->vm->areas[i];
      if(v->length && start < v->start + v->length && start+length > v->start){
        start = v->start + v->length;
        overlap = 1;
        break;
      }
    }
    if(!overlap) break;
  }
  if(start + length > MMAPEND){ release(&p->vm->lock); return -1; }
  p->vm->areas[slot] = (struct vma){ start, length, prot };
  release(&p->vm->lock);
  return start;
}

int
domunmap(uint64 addr, uint64 length)
{
  struct proc *p = myproc();
  if(addr % PGSIZE || !length || length > MMAPEND-MMAPBASE)
    return -1;
  length = PGROUNDUP(length);
  acquire(&p->vm->lock);
  if(p->vm->refs == 1){
    for(int i = 0; i < NVMA; i++){
      struct vma *v = &p->vm->areas[i];
      if(v->length == length && v->start == addr){
        uvmunmap(p->pagetable, addr, length/PGSIZE, 1);
        memset(v, 0, sizeof(*v));
        release(&p->vm->lock);
        return 0;
      }
    }
  }
  release(&p->vm->lock);
  return -1;
}

uint64 sys_mmap(void) { uint64 n; int prot; argaddr(0,&n); argint(1,&prot); return dommap(n,prot); }
uint64 sys_munmap(void) { uint64 a,n; argaddr(0,&a); argaddr(1,&n); return domunmap(a,n); }
uint64 sys_clone(void) { uint64 f,a,s; argaddr(0,&f); argaddr(1,&a); argaddr(2,&s); return clone(f,a,s); }
uint64 sys_join(void) { uint64 s; argaddr(0,&s); return join(s); }
uint64 sys_getproc(void) { int pid; uint64 out; argint(0,&pid); argaddr(1,&out); return getproc(pid,out); }
uint64 sys_setscheduler(void) { int mode; argint(0,&mode); return setscheduler(mode); }
uint64 sys_settickets(void) { int n; argint(0,&n); return settickets(n); }
