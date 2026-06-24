Title: **Computed Lists**
Status: Accepted

## Context

The `range` intrinsic is often used to give us a range of values to iterate over, but we never do anything else with it.  (There is no form of `for` loop that does not iterate over a list, so this is a very common pattern.)  When the range is large, we are allocating and deallocating big lists in memory for no good reason.

Python deals with the same issue by returning a different kind of object (an iterable); if you actually _want_ a list, you have to cast it through the `list()` function.  That's a gotcha for new users and an annoyance for all users.  We'd like a solution that retains the efficiency gains of lazily computing the list elements, but without making users have to manually materialize it when that is needed.

## Decision

Allow a list to be "computed" rather than stored, but keep this as an implementation detail (perhaps exposed via the `info` intrinsic, if at all).  A computed list could be constructed via `range`, and via `*` when the left-hand side is a single-element list containing an immutable value (in this case we store `null` for the increment).  Note that by either method, we still apply the MAX_COLLECTION_SIZE limit.

Internally, this means adding a `Computed` flag on GCList, which changes the meaning of Items.  When Computed == true, then Items have exactly three elements:

- Items[0]: the base value
- Items[1]: the increment
- Items[2]: the number of elements (i.e. length)

Items should be made private with this change, with inline methods protecting all access to it.  These methods will hide the difference between computed and stored (materialized) items.  In the computed case:

- Getting item index i: verifies i (after resolving a negative index) is in the range [0, length), and then returns base value + increment * i (or if increment is `null`, just returns the base value).
- Getting the length: just returns the number of elements.
- Popping a value: returns the computed last value, and decrements length.
- Any other mutation: _materializes_ the list before mutating.

Materializing a list means setting Computed = False, and replacing items with computed values.

## Alternatives Considered

- A new data type just for this purpose: complicates the language, more things for the user to learn, and requires user to understand and jump through hoops, as in Python.
- A general "iterable" interface: would allow the user to define their own iterables, but again brings more complications and hoop-jumping in the common case.
- Having the compiler recognize the idiom of `for` with `range`, and compile this to bytecode that computes the loop variable directly.  This would get much of the benefit of this proposal in common cases, but moves the complexity into the compiler, and loses other (though more rare) cases where holding onto a computed list would have benefit.
- We considered keeping a list computed when `push`ing an element that would match the next computed element, but for now have decided to forego this as `push` is very commonly used, and this optimization would very rarely apply.

## Consequences

- `range()` suddenly becomes efficient even on very large ranges.
- So does `[x] * n` (even when `x` is not a number!).
- Computed lists are transparent to the user -- everything "just works".
- There may be an unexpected hiccup later in processing when a large computed list is materialized upon first mutation.
- We will need to update perhaps a dozen or so places where code currently accesses Items directly.
- Any code that mutates a list (possibly triggering materialization) must write back the updated GCItem value (just as SetFrozen does); otherwise, we could end up with a corrupted list where the Computed flag does not match the actual meaning of its Items.
- No update to MarkChildren is required; it will work as-is on the values stored in Items (including the base item, which may be a GC immutable list or map).
- With a fractional step, the result could differ slightly from what `range` currently produces (which uses repeated addition rather than multiplication).  The new behavior is arguably more correct.
