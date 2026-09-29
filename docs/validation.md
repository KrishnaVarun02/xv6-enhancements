# Validation record

Executed locally on macOS arm64 with GCC 16.2.0 (riscv64-elf) and QEMU 11.1.1.

| Check | Result |
| --- | --- |
| Kernel and all user programs, warnings as errors | Passed |
| Extension guest suite, 3 CPUs | Passed |
| Extension guest suite, 1 CPU | Passed |
| Upstream `usertests -q`, 3 CPUs | `ALL TESTS PASSED` |
| Stride proportional-share test, 1 CPU | Passed: weight 10 received 16 ticks; weight 30 received 44 ticks |

Commands:

```sh
make -j6 TOOLPREFIX=riscv64-elf- kernel/kernel fs.img
python3 tests/qemu_test.py --cpus 3
python3 tests/qemu_test.py --cpus 1
python3 tests/qemu_test.py --command 'usertests -q' --expect 'ALL TESTS PASSED' --timeout 300
python3 tests/qemu_test.py --cpus 1 --command schedtest --expect 'SCHEDULER TEST PASSED'
```

The quick suite exercises process creation/reaping, file operations, pipe I/O,
preemption, memory boundaries, invalid pointers, executable permissions, and
allocation failures. Its deliberate invalid accesses print `usertrap` diagnostics;
these are expected task terminations, not kernel panics. The test harness requires
the final success marker.

The slow upstream suite was not run. The scheduling sample is an observed finite
run, not a throughput guarantee. The test accepts a broad weight ratio to account
for tick-boundary effects. No original coursework benchmark is claimed.
