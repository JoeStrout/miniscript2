// Control for global_loop.ms: the identical loop, inside a function.
//
// Every variable here is a function local, so it lives in a register and is
// reached by register index.  The bytecode differs from global_loop.ms only in
// how the five variables are addressed, so the gap between the two benchmarks
// is the cost of the global access path and nothing else.
//
// Keep this in lockstep with global_loop.ms; the pair is only meaningful if the
// loop bodies stay identical.  Both must print the same result.

run = function(gN)
	gTotal = 0
	gA = 1
	gB = 2
	gI = 0

	while gI < gN
		gTotal = (gTotal + gA + gB) % 999983
		gA = gB
		gB = gTotal
		gI = gI + 1
	end while

	return gTotal
end function

print "Result in r0:"
print run(10000000)
