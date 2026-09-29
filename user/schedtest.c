#include "kernel/types.h"
#include "user/user.h"
struct sample { int weight, ticks; };
int main(void) {
  int fd[2];
  if(pipe(fd)<0 || setscheduler(SCHED_STRIDE)<0) exit(1);
  int end=uptime()+60;
  for(int i=0;i<2;i++){
    int pid=fork();
    if(pid<0) exit(1);
    if(pid==0){
      close(fd[0]);
      int weight=i==0?10:30;
      settickets(weight);
      volatile int work=0;
      while(uptime()<end) for(int j=0;j<10000;j++) work++;
      (void)work;
      struct procinfo p; getproc(getpid(),&p);
      struct sample s={weight,(int)p.cpu_ticks};
      write(fd[1],&s,sizeof(s)); close(fd[1]); exit(0);
    }
  }
  close(fd[1]);
  int low=0, high=0;
  for(int i=0;i<2;i++){
    struct sample s;
    if(read(fd[0],&s,sizeof(s))!=sizeof(s)){ printf("FAIL scheduler samples\n"); exit(1); }
    if(s.weight==10) low=s.ticks; else high=s.ticks;
  }
  close(fd[0]); wait(0); wait(0); setscheduler(SCHED_RR);
  printf("stride ticks: weight10=%d weight30=%d\n",low,high);
  if(low<3 || high<low*2 || high>low*5){ printf("FAIL stride share; run with CPUS=1\n"); exit(1); }
  printf("SCHEDULER TEST PASSED\n"); exit(0);
}
