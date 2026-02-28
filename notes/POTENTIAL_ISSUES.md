This file tracks possible issues we become aware of during development, but haven't yet dealt with.


## Intrinsics vs. VMs

There is a possible issue if a user tries to set up two different VMs with different intrinsic functions.  The type maps (e.g. `_listType`) are static, and hold FuncRef indices based on whichever VM called `RegisterAll` most recently.  Those indices will vary depending on how many compiled functions there were.  So they're certainly not going to match up between VMs.  This means that when two different VMs are running at once, one of them is going to just flat-out invoke the wrong function (possibly a user function, like @main) instead of the intended intrinsic.

Ideally, the intrinsics would have stable function indices that never change.  Maybe we can do something like use negative values for intrinsics, and non-negative values for compiled functions.


