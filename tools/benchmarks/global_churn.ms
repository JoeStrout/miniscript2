// Globals reached by name from inside function calls.
//
// The complement to global_loop.ms.  Nothing here touches a global through a
// top-level named variable; every access comes from inside a function frame,
// which is how a library, a REPL-defined helper, or a seeded child interpreter
// actually reaches globals.  Three shapes, in order of how awkward they are for
// the current implementation (see notes/GLOBALS.md):
//
//   1. a bare free identifier, resolved by VM.LookupVariable
//   2. globals[k] with a computed key, for names @main never mentions
//   3. many such names live at once
//
// Shape 1 is worth watching: today a free identifier in a top-level function
// resolves through the frame's OuterVarMap, which is the globals VarMap, whose
// VarMapBacking.FindOrderIdx is a LINEAR SCAN over every register-bound global.
// So the cost of reading one global today is proportional to how many globals
// the program has.  The padding block below exists to make that realistic
// rather than accidentally trivial -- a real program has dozens of globals, not
// four.  A slot-indexed design should make this flat.
//
// Shape 2 lands in the other tier: because @main never names "dyn0".."dyn31",
// those globals cannot be register-bound and live in the VarMap's hash table
// instead.  The split between the two tiers is what the redesign removes.
//
// For scale: as of 2026-08-01 the churn loop below runs about 33x slower
// against `globals` than the identical loop against an ordinary map (0.12s vs
// 4.0s at 20000 passes, C++ goto build).  Every access pays two linear scans --
// one on the get, one on the set -- before reaching the hash table.  That gap
// is the thing to watch shrink.
//
// All values stay well under 1e6, so the arithmetic is exact in a double.

// ── Padding: a realistic population of top-level globals ────────────────────
// These are never read.  They are here so that the globals map has a typical
// number of register-bound entries for the lookups below to scan past.
p00 = 0; p01 = 1; p02 = 2; p03 = 3; p04 = 4; p05 = 5; p06 = 6; p07 = 7
p08 = 8; p09 = 9; p10 = 10; p11 = 11; p12 = 12; p13 = 13; p14 = 14; p15 = 15
p16 = 16; p17 = 17; p18 = 18; p19 = 19; p20 = 20; p21 = 21; p22 = 22; p23 = 23
p24 = 24; p25 = 25; p26 = 26; p27 = 27; p28 = 28; p29 = 29; p30 = 30; p31 = 31

// ── Shape 1: bare free identifier, read from inside a function ──────────────
step = 7

readLoop = function(n)
	acc = 0
	i = 0
	while i < n
		acc = (acc + step) % 999983    // "step" is a free identifier -> globals
		i = i + 1
	end while
	return acc
end function

// ── Shapes 2 and 3: computed keys, for names @main never mentions ───────────
keys = []
for i in range(0, 31)
	keys.push "dyn" + i
end for

seed = function(keys)
	for k in keys
		globals[k] = 0
	end for
end function

churn = function(keys, passes)
	p = 0
	while p < passes
		for k in keys
			globals[k] = globals[k] + 1
		end for
		p = p + 1
	end while
end function

total = function(keys)
	t = 0
	for k in keys
		t = t + globals[k]
	end for
	return t
end function

// ── Run ─────────────────────────────────────────────────────────────────────
answer = readLoop(2000000)

seed keys
churn keys, 5000
answer = answer + total(keys)

print "Result in r0:"
print answer
