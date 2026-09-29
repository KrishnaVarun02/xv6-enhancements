#!/usr/bin/env python3
"""Boot xv6 and assert guest output, with a strict deadline and saved transcript."""
import argparse, os, selectors, subprocess, sys, time
from pathlib import Path
p=argparse.ArgumentParser()
p.add_argument('--cpus',type=int,default=3)
p.add_argument('--command',default='exttest')
p.add_argument('--expect',default='ALL EXTENSION TESTS PASSED')
p.add_argument('--timeout',type=int,default=120)
p.add_argument('--log',default='test-output.log')
a=p.parse_args()
os.chdir(Path(__file__).resolve().parents[1])
cmd=['qemu-system-riscv64','-machine','virt','-bios','none','-kernel','kernel/kernel','-m','128M','-smp',str(a.cpus),'-nographic','-global','virtio-mmio.force-legacy=false','-drive','file=fs.img,if=none,format=raw,id=x0','-device','virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0']
q=subprocess.Popen(cmd,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
s=selectors.DefaultSelector();s.register(q.stdout,selectors.EVENT_READ)
buf='';sent=False;passed=False;end=time.monotonic()+a.timeout
try:
 while time.monotonic()<end:
  for key,_ in s.select(timeout=.5):
   chunk=os.read(key.fd,65536).decode(errors='replace')
   if not chunk: raise RuntimeError('QEMU exited before success')
   sys.stdout.write(chunk);sys.stdout.flush();buf+=chunk
  if not sent and '$ ' in buf:
   q.stdin.write((a.command+'\n').encode());q.stdin.flush();sent=True
  if sent and a.expect in buf:
   passed=True;break
  if 'panic:' in buf or 'FAIL' in buf or (sent and buf.count('$ ')>1): break
finally:
 q.terminate()
 try:q.wait(timeout=5)
 except subprocess.TimeoutExpired:q.kill();q.wait()
 Path(a.log).write_text(buf)
if not passed:
 sys.exit('Guest test failed or timed out; see '+a.log)
print('\nGuest verification passed.')
