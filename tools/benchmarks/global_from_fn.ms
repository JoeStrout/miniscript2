// Free names read from *inside* a function: what stage 5 of the globals rework
// is about (see notes/GLOBALS.md).
//
// The other three globals benchmarks measure top-level access (global_loop),
// the same loop in registers (global_loop_fn), and reaching globals by computed
// name through the `globals` map (global_churn).  None of them measure the case
// that most real code is made of: a function body mentioning an intrinsic or a
// top-level name.  That name has no register in the function, so before stage 5
// every read walked out through LookupVariable and finished with a hash lookup.
//
// The loop body reads two globals and one intrinsic per iteration, and keeps its
// accumulator and counter in locals, so the difference between this and
// global_loop_fn is the free-name path and nothing else.
//
// All values stay under 1e8, so the arithmetic is exact in a double.

gA = 3
gB = 4

run = function(n)
	total = 0
	i = 0
	while i < n
		total = total + floor(gA + gB)
		i = i + 1
	end while
	return total
end function

print "Result in r0:"
print run(3000000)
