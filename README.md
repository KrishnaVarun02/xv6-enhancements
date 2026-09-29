# xv6 virtual memory, threads, and scheduling

A runnable extension of MIT's RISC-V xv6, based on the December 2023 revision
[`dd2574bc1097a912e799340172b8b6ef42ac5ceb`](https://github.com/mit-pdos/xv6-riscv/commit/dd2574bc1097a912e799340172b8b6ef42ac5ceb).
This is a new implementation of the described coursework features, not a recovered
Autumn 2023 submission. Upstream code, acknowledgements, and its MIT license are
retained. This repository uses a fresh commit history; the upstream revision above
records the source provenance. The original introduction is in [`README`](README).

## Features

- Demand-zero `sbrk`: virtual reservations consume physical pages only on access.
  Load/store faults and kernel `copyin`, `copyout`, and `copyinstr` share fault handling.
- Anonymous `mmap` with read-only/read-write protection, lazy population, private
  fork copies, hole reuse, whole-region `munmap`, and cleanup at exec/last reap.
- `clone`/`join`: independently scheduled kernel tasks sharing an address space,
  with separate registers, kernel/user stacks, and supervisor-only trapframe slots.
  Reference counts preserve memory after a parent exits. Pipe descriptors reference
  the same pipe objects; user spinlocks protect shared data.
- Runtime round-robin or proportional-share stride scheduling, with 1–100 tickets.
- `getproc`, `ps`, and `schedbench`: task state, dispatches, CPU timer ticks,
  syscall/page-allocation counts, and local resident-page counts.

## Build and run

On Ubuntu/Debian:

```sh
sudo apt-get install make gcc-riscv64-linux-gnu binutils-riscv64-linux-gnu qemu-system-misc
make -j4 TOOLPREFIX=riscv64-linux-gnu- kernel/kernel fs.img
make TOOLPREFIX=riscv64-linux-gnu- qemu
```

On macOS (Homebrew and Command Line Tools required):

```sh
brew install qemu riscv64-elf-gcc
make -j4 TOOLPREFIX=riscv64-elf- kernel/kernel fs.img
make TOOLPREFIX=riscv64-elf- qemu
```

In the xv6 shell:

```text
exttest
ps
schedbench 0
schedbench 1
```

Use `Ctrl-A`, then `X` to exit QEMU. Set `CPUS=1` when comparing the scheduling
policies; SMP selection is approximate and multiple workers may run at once.

## Verify

```sh
make test TOOLPREFIX=riscv64-elf-
python3 tests/qemu_test.py --cpus 1
# Selected upstream compatibility tests can be run individually:
python3 tests/qemu_test.py --command 'usertests pipe1' --expect 'ALL TESTS PASSED'
```

`exttest` executes in the guest. It checks physical allocation counts, zero-filled
pages, faults from syscall buffers, fork isolation, read-only write rejection,
80,000 increments from four concurrent threads, pipe messages, join status,
orphaned thread lifetime, and scheduler/monitor argument validation. The host
harness detects failed assertions, kernel panics, unexpected exits, and timeouts.
CI builds and boots with one and three virtual CPUs.

## System call API

See [`kernel/extensions.h`](kernel/extensions.h) and [`user/user.h`](user/user.h).

| Call | Contract |
| --- | --- |
| `mmap(length, prot)` | Returns a page-aligned anonymous address or `MAP_FAILED`; `prot` is `PROT_READ` or `PROT_READ \| PROT_WRITE`. |
| `munmap(addr, length)` | Removes one entire mapping; length is rounded up to a page. Returns 0 or -1. |
| `clone(entry, arg, stack)` | Shares VM; starts `entry(arg)` on an aligned, writable 4 KiB stack supplied by the caller. Returns child TID or -1. The entry must call `exit`. |
| `join(&status)` | Reaps one direct thread child and returns its TID; `wait` reaps process children. |
| `setscheduler(policy)` | Selects `SCHED_RR` or `SCHED_STRIDE` globally and resets service accounting. |
| `settickets(n)` | Sets the caller's stride weight, 1–100. |
| `getproc(pid, &info)` | Copies a task snapshot; returns 0 or -1. Resident counts are populated for the caller only. |

Thread stacks and shared buffers should be reserved before cloning. File descriptor
tables are copied, while file offsets and pipe objects are shared by reference.
Each task closes its own descriptors on exit.

## Deliberate scope

This is an educational kernel, not a POSIX implementation. `mmap` is anonymous,
private across fork, and shared within a clone group. File-backed mapping,
`MAP_FIXED`, partial unmapping, futexes, and remote TLB shootdowns are not implemented.
There are at most 16 mappings occupying a 64 MiB arena.

To prevent stale translations on other CPUs, `sbrk`, `mmap`, `munmap`, `fork`, and
`exec` reject changes while an address space has unjoined siblings (including
zombies). Existing reservations can fault concurrently; a VM spinlock serializes
page-table updates. Join siblings before changing the layout. This restriction is
explicitly tested. Memory allocation counters count successful demand allocations,
not all hardware exceptions. A runnable task's scheduling weight determines service
per dispatch; sleep-heavy tasks and SMP workloads need different fairness analysis.

The upstream quick regression suite (`usertests -q`) passed with three CPUs.
The slower suite has not been run. Lazy allocation changes when physical exhaustion
is reported: a reservation may succeed and a later access can fail.
See [design notes](docs/design.md) and [validation record](docs/validation.md).
