This document describes a new likely feature of MiniScript 2: _frozen values_.

## New Features

Maps and lists will have an internal _frozen_ flag.  Some new intrinsics will interact with this flag:

- `freeze x` will recursively set the frozen flag on x and its contents.
- `frozen(x)` will return true if x is frozen, false if not.
- `frozenCopy(x)` will return x if x is already frozen; otherwise it will make a copy of it with the frozen bit set (and do the same recursively for its contents).

And, using any list or map as a map key actually uses a `frozenCopy` on assignment (automatically).

Any attempt to mutate a frozen list/map will result in a runtime `Attempt to modify a frozen list` (or `map`) error.

## Rationale

So, MiniScript gets immutable types — but not by introducing new types; by simply having a way to make any list/map immutable.  And immutable objects are functionally equivalent to values, so we get all the benefits thereof.  Maps in the MiniScript API which are currently read-only (due to under-the-hood assignOverride features) will become ordinary frozen maps.  And user code now has the ability to do the same thing.

And, the whole issue of mutating map keys goes away, because map keys are now always immutable.  Yet you can still just stuff something like `[5, 7]` (e.g. 2D coordinates) into a map and retrieve by it just fine.  (Hashing and equality tests will ignore the frozen flag; the flag is not part of the "value" of the list/map.) 

What I like about this is that it handles progressively more obscure/advanced situations very naturally:

1. Most people never use maps or lists as keys anyway, so they remain blissfully ignorant.
2. If they do, chances are they don't mutate their keys, so it just works.  There's an extra copy going on, but their list/map keys are probably small, so the impact is minimal.
3. If they do mutate their lists/keys, then look in their map, they'll find that the map still contains the old values.  That's a little surprising, if they understand object references, so maybe they ask or search and learn about freezing and frozenCopy.  Neat!  Everything still works as well as can be (i.e. they can still look up by the old values).
4. If they are now concerned about performance, they can explicitly freeze their keys before insertion, eliminating the copy.

## Open Design Questions

- Should `freeze` return `null`, or return `x`?

- How does this interact with closures (which contain a map of outer vars), if at all?

- Should we freeze and reuse default arguments on function declarations, or instead re-run the code evaluating those expressions every time the default is needed?

