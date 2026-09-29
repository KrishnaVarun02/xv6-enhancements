#include "kernel/types.h"
#include "user/user.h"
#define PAGE 4096
#define CHECK(x) do { if(!(x)){ printf("FAIL line %d: %s\n", __LINE__, #x); exit(1); } } while(0)
static volatile int counter, lock;
static int fds[2];
static char *shared;
static void worker(void *arg) {
  for(int i=0;i<20000;i++){
    while(__sync_lock_test_and_set(&lock,1)) ;
    counter++;
    __sync_lock_release(&lock);
  }
  shared[(uint64)arg * PAGE] = 42;
  CHECK(sbrk(PAGE) == (char*)-1);
  CHECK(mmap(PAGE,PROT_READ) == MAP_FAILED);
  char *args[]={"echo",0};
  CHECK(exec("echo",args) == -1);
  CHECK(munmap(shared, 4*PAGE) == -1);
  CHECK(write(fds[1], "x", 1) == 1);
  exit(7);
}
static void survivor(void *arg) {
  sleep(3);
  *(volatile int*)arg = 99;
  exit(0);
}
int main(void) {
  struct procinfo before, after;
  CHECK(getproc(getpid(), &before) == 0);
  char *heap = sbrk(8*PAGE);
  CHECK(heap != (char*)-1);
  CHECK(getproc(getpid(), &after) == 0);
  CHECK(after.resident_pages == before.resident_pages);
  heap[7*PAGE]=23;
  CHECK(heap[0] == 0 && heap[7*PAGE] == 23);
  CHECK(getproc(getpid(), &after) == 0);
  CHECK(after.page_faults >= before.page_faults+2);
  CHECK(pipe(fds)==0);
  CHECK(write(fds[1], "lazy", 4)==4);
  CHECK(read(fds[0], heap+PAGE, 4)==4);
  CHECK(memcmp(heap+PAGE,"lazy",4)==0);
  CHECK(write(fds[1],heap+2*PAGE,1)==1);
  CHECK(read(fds[0],heap+3*PAGE,1)==1 && heap[3*PAGE]==0);
  CHECK(sbrk(-8*PAGE)==heap+8*PAGE);
  printf("PASS lazy allocation and syscall buffers\n");

  CHECK(mmap(0,PROT_READ)==MAP_FAILED);
  shared=mmap(4*PAGE,PROT_READ|PROT_WRITE);
  CHECK(shared!=MAP_FAILED);
  shared[0]=5;
  int child=fork(), status;
  CHECK(child>=0);
  if(child==0){ CHECK(shared[0]==5 && shared[PAGE]==0); shared[0]=8; exit(0); }
  CHECK(wait(&status)==child && status==0 && shared[0]==5);
  char *ro=mmap(PAGE,PROT_READ);
  CHECK(ro!=MAP_FAILED && ro[0]==0);
  CHECK(write(fds[1],"y",1)==1);
  CHECK(read(fds[0],ro,1)<=0 && ro[0]==0);
  child=fork(); CHECK(child>=0);
  if(child==0){ ro[0]=1; exit(8); }
  CHECK(wait(&status)==child && status==-1);
  CHECK(munmap(ro,PAGE)==0);
  CHECK(munmap(shared+PAGE,PAGE)==-1);
  printf("PASS mmap, fork isolation, and permissions\n");

  void *stacks[4];
  for(int i=0;i<4;i++){ stacks[i]=mmap(PAGE,PROT_READ|PROT_WRITE); CHECK(stacks[i]!=MAP_FAILED); }
  for(int i=0;i<4;i++) CHECK(clone(worker,(void*)(uint64)i,stacks[i])>0);
  CHECK(wait(0)==-1);
  CHECK(fork()==-1);
  for(int i=0;i<4;i++) CHECK(join(&status)>0 && status==7);
  CHECK(join(0)==-1 && counter==80000);
  for(int i=0;i<4;i++){ CHECK(shared[i*PAGE]==42); CHECK(munmap(stacks[i],PAGE)==0); }
  char buf[4]; CHECK(read(fds[0],buf,4)==4);
  CHECK(munmap(shared,4*PAGE)==0);
  CHECK(clone(worker,0,(void*)1)==-1);
  close(fds[0]); close(fds[1]);
  printf("PASS clone/join, shared memory, spinlock, and pipes\n");

  // Parent can exit before a thread. Init reaps the orphan; memory lives on.
  child=fork(); CHECK(child>=0);
  if(child==0){
    char *p=mmap(2*PAGE,PROT_READ|PROT_WRITE); CHECK(p!=MAP_FAILED);
    CHECK(clone(survivor,p,p+PAGE)>0); exit(0);
  }
  CHECK(wait(&status)==child && status==0);
  sleep(8);
  CHECK(setscheduler(99)==-1 && settickets(0)==-1);
  CHECK(setscheduler(SCHED_STRIDE)==0 && settickets(25)==0);
  CHECK(getproc(getpid(),&after)==0 && after.tickets==25 && after.switches>0);
  CHECK(setscheduler(SCHED_RR)==0);
  printf("PASS lifecycle and monitoring\n");
  printf("ALL EXTENSION TESTS PASSED\n");
  exit(0);
}
