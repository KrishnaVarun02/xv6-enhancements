#ifndef XV6_EXTENSIONS_H
#define XV6_EXTENSIONS_H
#define PROT_READ 1
#define PROT_WRITE 2
#define MAP_FAILED ((void *)-1)
#define SCHED_RR 0
#define SCHED_STRIDE 1
#define NVMA 16
#define MMAPBASE 0x40000000L
#define MMAPEND  0x44000000L
struct procinfo {
  int pid, state, isthread, tickets;
  uint64 bytes, resident_pages, page_faults, syscalls, switches, cpu_ticks;
  char name[16];
};
#endif
