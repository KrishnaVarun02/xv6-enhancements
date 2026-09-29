# Design and invariants

## Address space

The low region contains the ELF image, guard/stack pages, and heap. `heapbase` is
recorded after exec so a missing code or guard page cannot be mistaken for lazy
heap. The heap cannot reach `0x40000000`. The anonymous arena occupies
`[0x40000000, 0x44000000)`. Permissions come from the VMA, never from the kind of
fault alone. Writable RISC-V leaves also carry read permission. mmap never adds X.

A shared `vmspace` occupies one allocator page and contains a spinlock, refcount,
and VMA table. The lock protects page installation and mapping metadata. Fault
allocation only uses non-sleeping kalloc/mappages operations. Invalid addresses and
protection failures kill a user task; invalid syscall buffers return an error or
short transfer according to the inherited xv6 syscall's contract.

Each task maps its own trapframe at `TRAPFRAME - slot * PGSIZE`. These mappings are
supervisor-only. `sscratch` communicates the active trapframe VA across trap entry;
userret receives both that VA and satp. This avoids one shared trapframe being
clobbered by concurrent threads. Each return flushes the local TLB.

## Lifetime and locking

- Each allocated task, including zombies, owns a VM reference until reap.
- Each task owns one trapframe; only its mapping is removed when that task is reaped.
- Last-reference cleanup frees user pages, the anonymous arena, and page tables.
- wait_lock precedes a child proc lock, which may precede the VM lock. No path
  holding the VM lock acquires a proc lock or performs sleeping I/O.
- A process can exit before its thread children. Init reaps adopted threads with
  wait; the remaining references keep memory alive in the meantime.
- Layout changes require a single reference. Other running CPUs therefore cannot
  retain a user translation to a page that munmap or exec has just freed.
- Allocations into existing lazy reservations are shared immediately. A competing
  fault rechecks the leaf after acquiring the VM lock.

## Scheduling and counters

RR rotates a per-CPU cursor. Stride selects a runnable task with the lowest pass
value, then charges `1,000,000 / tickets` per dispatch. A proc lock revalidates a
candidate and protects dispatch, preventing two CPUs from running one task.
Selection across CPUs is approximate; comparisons should use one CPU. Newly
allocated tasks start from the most recently selected service value. Policy
changes reset pass accounting. All tasks can select policies, as xv6 has no
privilege/account model.

CPU ticks count timer interruptions of the task on any hart. Dispatch counters
are updated by the scheduler. Fault/syscall counters use atomic increments.
getproc reads a locked task snapshot, then releases locks before copying to user
memory. Remote memory counts are omitted because exec can replace that task's
page table concurrently. Fields are diagnostic snapshots, not a synchronized
system-wide measurement.
