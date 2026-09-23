// manyVarsMain: reproduces the reported bug where a module that defines more
// than 255 module-scope variables loses all but the last 255 of them when
// imported.  Run this one:
//
//   ./build/cpp/miniscript examples/manyVarsMain.ms
//
// manyVarsModule.ms defines v001..v300, with vNNN == NNN.  All 300 should come
// back in the imported module's map.  If the bug is present, the early names
// (v001, v002, ...) are missing and only the last 255 (v046..v300) survive.

mod = import("manyVarsModule")

expected = 300
missing = []
wrong = []

for i in range(1, expected)
	name = "v" + ("00" + i)[-3:]
	if not mod.hasIndex(name) then
		missing.push name
	else if mod[name] != i then
		wrong.push name + "=" + mod[name]
	end if
end for

print "module vars found: " + mod.len + " (expected at least " + expected + ")"
print "missing: " + missing.len
if missing then print "  first missing: " + missing[0] + "  last missing: " + missing[-1]
print "wrong values: " + wrong.len
if wrong then print "  first wrong: " + wrong[0]

print ""
if missing.len == 0 and wrong.len == 0 then
	print "ALL PASSED"
else
	print "FAILED: " + (missing.len + wrong.len) + " of " + expected + " module variables bad"
end if
