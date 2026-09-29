#include "kernel/types.h"
#include "user/user.h"
int main(int argc, char **argv) {
  int start=1,end=1024;
  if(argc>1) start=end=atoi(argv[1]);
  printf("PID STATE THREAD TICKETS CPU_TICKS SWITCHES FAULTS NAME\n");
  for(int pid=start;pid<=end;pid++){
    struct procinfo p;
    if(getproc(pid,&p)==0)
      printf("%d %d %d %d %d %d %d %s\n",p.pid,p.state,p.isthread,p.tickets,(int)p.cpu_ticks,(int)p.switches,(int)p.page_faults,p.name);
  }
  exit(0);
}
