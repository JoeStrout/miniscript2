// Map insert, lookup, field access, and iteration.
//
// There was no map benchmark until now, which is part of why the iteration
// defect below went unmeasured for so long.  Maps are the most heavily used
// structure in the language -- every object, every class, every namespace, and
// `locals`/`outer`/`globals` themselves -- so all four shapes here matter.
//
// The four sections, and what each is watching:
//
//   1. INSERT      -- growing a map one key at a time, including the rehashes.
//   2. LOOKUP      -- m[k] with a computed key, the plain hash-table path.
//   3. FIELD       -- obj.x on a small map, which is what object and method
//                     access actually compile to.  Small maps are the common
//                     case by a wide margin; this keeps them represented.
//   4. ITERATION   -- `for kv in m`, the shape that motivated this file.
//
// Section 4 is the one to watch.  As of 2026-09-15 iteration is O(n^2): the
// map's entry order lives in the host language's Dictionary, and GCMap.KeyAt /
// GCMap.ValueAt reach the i'th entry by re-walking the enumeration from the
// start -- twice per step, since ITERGET calls both.  Iterating a 2000-entry
// map 50 times (100000 logical steps) took 1.17s on C# and 0.23s on C++, where
// 200000 plain lookups took 0.19s and 0.046s.  Iteration should cost about what
// a lookup does, so there is roughly 8x on the table for C# and 5x for C++.
//
// The other three sections are here as guard rails: the fix for section 4 moves
// where entries are stored, so insert, lookup and field access must not regress
// to buy it.
//
// Sizes are chosen so the whole run is a few seconds, and so that section 1
// crosses several rehash boundaries.  All arithmetic stays well under 2^53.

N = 200000        // insert / lookup count
FIELD_PASSES = 200000
ITER_SIZE = 2000
ITER_PASSES = 50

check = 0

// ── 1. Insert ───────────────────────────────────────────────────────────────
m = {}
i = 1
while i <= N
	m[i] = i * 2
	i = i + 1
end while
check = check + m.len            // + 200000

// ── 2. Lookup ───────────────────────────────────────────────────────────────
// Read every key back, in a different order than it was inserted, so this is
// hash-table work rather than a walk down one bucket chain.
sum = 0
i = N
while i >= 1
	sum = sum + m[i]
	i = i - 1
end while
check = check + sum % 1000003    // + 80000

// ── 3. Field access on a small map ──────────────────────────────────────────
obj = {"x": 3, "y": 5, "z": 7}
sum = 0
i = 0
while i < FIELD_PASSES
	sum = sum + obj.x + obj.y + obj.z
	i = i + 1
end while
check = check + sum % 1000003    // + 999994

// ── 4. Iteration ────────────────────────────────────────────────────────────
small = {}
i = 1
while i <= ITER_SIZE
	small[i] = i
	i = i + 1
end while

sum = 0
p = 0
while p < ITER_PASSES
	for kv in small
		sum = sum + kv.value
	end for
	p = p + 1
end while
check = check + sum % 1000003    // + 49700

// Total: 200000 + 80000 + 999994 + 49700 = 1329694
print "Result in r0:"
print check
