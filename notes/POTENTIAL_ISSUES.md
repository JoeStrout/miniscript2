This file tracks possible issues we become aware of during development, but haven't yet dealt with.


## Type maps are shared by every VM (issue #37)

The core type maps (`_listType`, `_stringType`, and friends in `CoreIntrinsics`)
are static, so every interpreter in the process sees the same ones.  That means
a script which extends a built-in type — `string.reverse = function ... ` — has
extended it for every other VM as well, including ones created later.

This is deliberate as far as it goes: `Intrinsic.EnsureBuilt` explicitly does
*not* rebuild the type maps per VM, because doing so would discard whatever a
script had added to `list`, `string` or `map`, and would also clear short names
a host registered during its own setup.  The maps are GC roots, so nothing
sweeps them out from under us.  But it has never been decided whether
cross-VM leakage is what we actually want, and a host embedding two independent
interpreters would probably say no.

(An earlier version of this entry worried that the type maps held *function
indices* that would differ between VMs, so one VM would invoke the wrong
function entirely.  That cannot happen: `Intrinsic.GetFunc` builds a funcRef
over a `FuncDef` object, not an index into any VM's function table, and the
index-based `CALLFN` opcode is gone.)


## Waiting intrinsics busy-wait instead of yielding (issue #45)

`wait` (and `exec`, while its process is still running) returns a partial
result but never sets `vm.yielding`.  The VM handles that by stopping the batch
(`cyclesLeft = 0` in the CALL path) or, on each re-invocation in `Run()`,
returning without running any bytecode.  But since `yielding` is still false,
`RunUntilDone`/`RunREPLSlice` just call `vm.Run()` again in a tight loop until
their time slice expires, and the hosts' `Thread.Sleep(1)` (which is gated on
`vm.yielding`) never happens.  So a `wait 10` burns a full core for ten seconds.

**Proposed fix: the VM yields automatically on a waiting partial result.**  A
partial result means "nothing for me to do until later" -- *unless* the
intrinsic pushed a manual call (`import`), in which case the module code is
meant to run inline in the same slice.  So:

- In the CALL path, where a not-done result with no `_hasPendingManualCall`
  currently sets `cyclesLeft = 0`, also set `yielding = true` (the check at
  the dispatch top then stops there).
- In `Run()`, where a re-invoked continuation is still not done, set
  `yielding = true` before returning.

This covers `wait`, `exec`, and any host intrinsic that waits, without each one
needing to know about the flag.

The `yielding` flag itself stays: the `yield` intrinsic returns done but still
needs to end the slice, and hosts need it to tell "script is waiting" (sleep, or
end the frame early) from "time slice ran out" (keep going).

One trade-off: an intrinsic that goes partial merely to chunk its own work,
rather than to wait on something external, would lose up to a host tick per
resumption.  Nothing does that today; if something ever needs to, we could add
an opt-out on `IntrinsicResult`.
