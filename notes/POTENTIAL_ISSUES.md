This file tracks possible issues we become aware of during development, but haven't yet dealt with.


## Intrinsics vs. VMs

There is a possible issue if a user tries to set up two different VMs with different intrinsic functions.  The type maps (e.g. `_listType`) are static, and hold FuncRef indices based on whichever VM called `RegisterAll` first.  This might be fine, if the VMs differ only in intrinsics that come *after* the built-in core intrinsics.  Or it might not.  We need to test this case and look for problems.

