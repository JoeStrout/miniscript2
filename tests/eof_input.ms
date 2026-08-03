// Regression test: `input` must return null at end of file, and must not
// crash the host.  Run this with stdin closed (`< /dev/null`); it is driven by
// the "test" target of tools/build.sh, which checks the output below.
//
// EOF has to be distinguishable from an empty line: a script that loops until
// it gets valid input has no other way to stop when there is nothing left to
// read.  See notes/bugs.md (entry 2, fixed 2026-08-03).

x = input("prompt: ")
print
print "isNull: " + (x == null)
print "isString: " + (x isa string)
