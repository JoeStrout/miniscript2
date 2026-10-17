// Fixture for testSuite.txt (SECTION: IMPORT): a module that calls a pending
// intrinsic (wait) at top level, before defining what its importer reads.
print "waitInModule start"
wait 0.01
print "waitInModule after wait"
x = 42
