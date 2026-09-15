This file tracks possible issues we become aware of during development, but haven't yet dealt with.


## Type maps are shared by every VM

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
