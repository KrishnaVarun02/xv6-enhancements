#include "kernel/types.h"
#include "user/user.h"
int main(int argc,char **argv) {
  int mode=(argc>1 && atoi(argv[1])==1) ? SCHED_STRIDE:SCHED_RR;
  setscheduler(mode);
  for(int i=1;i<=3;i++){
    if(fork()==0){
      settickets(i*10);
      int end=uptime()+40;
      volatile uint64 work=0;
      while(uptime()<end) for(int j=0;j<10000;j++) work++;
      struct procinfo p; getproc(getpid(),&p);
      printf("tickets=%d cpu_ticks=%d switches=%d work=%d\n",p.tickets,(int)p.cpu_ticks,(int)p.switches,(int)work);
      exit(0);
    }
  }
  for(int i=0;i<3;i++) wait(0);
  setscheduler(SCHED_RR);
  exit(0);
}
