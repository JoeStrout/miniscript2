// nestedImportMain: a 2-level import test (run this one).
//
//   nestedImportMain  ->  import "nestedImportOuter"  ->  import "nestedImportInner"
//
// This exercises a manually-pushed call (import) that runs *inside* another
// manually-pushed call.  It used to fail: the inner import clobbered the outer
// one, so import("nestedImportOuter") returned the bare string "nestedImportOuter"
// instead of the module's map, and the imported module's map was polluted with a
// bogus "libname" key.  This script verifies both are now correct.
//
// The three files must sit together in the examples folder so import's default
// search path ($MS_SCRIPT_DIR) finds them:
//   - nestedImportMain.ms   (this file)
//   - nestedImportOuter.ms  (imported here; imports the inner module)
//   - nestedImportInner.ms  (leaf module)

outerMod = import("nestedImportOuter")

// Simple assertion helper that reports and tallies failures.
assert = function(desc, got, want)
	if got == want then
		print "  ok: " + desc
	else
		print "  FAIL: " + desc + " (got " + got + ", wanted " + want + ")"
		globals.failures = globals.failures + 1
	end if
end function

failures = 0

// 1. The outer import must return the module's MAP, not its name.
assert "import returns a map", outerMod isa map, true

// 2. Values the outer module computed from the inner module must survive.
assert "outerMod.fromInner", outerMod.fromInner, 42
assert "outerMod.doubled", outerMod.doubled, 84
assert "outerMod.label", outerMod.label, "outer-loaded"

// 3. The inner module is reachable through the outer one.
assert "nested inner.answer", outerMod.nestedImportInner.answer, 42
assert "nested inner.double()", outerMod.nestedImportInner.double(10), 20

// 4. No bogus "libname" key leaked into the imported module's map.
assert "no libname pollution", outerMod.hasIndex("libname"), false

print ""
if failures == 0 then
	print "ALL PASSED"
else
	print failures + " CHECK(S) FAILED"
end if
