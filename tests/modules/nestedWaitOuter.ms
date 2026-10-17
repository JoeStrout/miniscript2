// Fixture for testSuite.txt (SECTION: IMPORT): a module that imports another
// module, both at top level and from inside a function, with waits between.
wait 0.01
import "nestedWaitInner"
wait 0.01
f = function
	import "nestedWaitInner"
	wait 0.01
	return nestedWaitInner.a + "f"
end function
b = nestedWaitInner.a + "B" + f
