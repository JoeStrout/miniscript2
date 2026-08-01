// Global variable access, the direct cost.
//
// Every named variable in this program is a global, because every statement is
// at top level.  The loop body is deliberately almost nothing but global reads
// and writes -- 5 reads and 4 writes per iteration -- so the time is dominated
// by whatever it costs to reach a global.
//
// This is the benchmark that answers "what does it cost to stop keeping
// top-level variables in registers?" (see notes/GLOBALS.md).  Compare it with
// the same loop written inside a function, which stays register-based:
// global_loop_fn.ms.
//
// All values stay under 1e6, so the arithmetic is exact in a double.

gTotal = 0
gA = 1
gB = 2
gI = 0
gN = 10000000

while gI < gN
	gTotal = (gTotal + gA + gB) % 999983
	gA = gB
	gB = gTotal
	gI = gI + 1
end while

print "Result in r0:"
print gTotal
