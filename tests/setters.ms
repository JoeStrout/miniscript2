// tests/setters.ms -- extended exercise of property setters ("x=" members).
//
// See notes/LANGUAGE_CHANGES.md, "Property Setters".  The integration suite in
// testSuite.txt covers the rules one at a time, in blocks that each compare a
// few lines of printed output.  This script covers what that format makes
// awkward: long scenarios where state accumulates, combinatorial sweeps over
// name encodings and chain depths, the exact ORDER in which a chain of setters
// runs, and sustained stress on the per-map setter cache (see GCMap._setterStatus
// and GCManager.SetterGeneration) -- which is the part most likely to break
// silently, since a stale cache gives a wrong answer rather than an error.
//
// Deliberately NOT covered here: the cases that terminate the program (a
// read-only property, an invalid setter, a frozen map, a setter returning an
// error).  A runtime error cannot be caught, so each needs its own process;
// those live in testSuite.txt.
//
// Run:  build/cs/miniscript2 tests/setters.ms
//       build/cpp/miniscript2 tests/setters.ms
// Exits 0 if every check passes, 1 otherwise.

checks = 0
failures = 0
log = []

check = function(label, actual, expected)
	globals.checks = globals.checks + 1
	if actual != expected then
		globals.failures = globals.failures + 1
		print "FAIL: " + label
		print "  expected: " + expected
		print "  actual:   " + actual
	end if
end function

// Setters record what they did here; `taken` reads and clears the log, so each
// check sees only what the statements just before it produced.
note = function(s)
	globals.log.push s
end function

taken = function
	s = globals.log.join(",")
	globals.log = []
	return s
end function

// A setter that records "<tag><-<value>" and stores nothing.
recorder = function(tag)
	return function(v)
		note tag + "<-" + v
	end function
end function

// ===========================================================================
print "1. name encodings across the tiny-string boundary"
// ---------------------------------------------------------------------------
// Value.SetterKey appends "=" with bit arithmetic while the result still fits
// 5 UTF-8 bytes, and builds a string otherwise.  Sweep both sides of that line,
// including multi-byte characters, since a wrong byte offset would only show up
// as a lookup that quietly misses.

names = ["a", "ab", "abc", "abcd", "abcde", "abcdef", "abcdefgh",
         "é", "éé", "ééé", "🎉", "🎉a", "🎉🎉", "aé", "aéb"]
enc = {}
for n in names
	enc[n + "="] = recorder(n)
end for
for n in names
	enc[n] = 1
end for
expected = []
for n in names
	expected.push n + "<-1"
end for
check "every name length routes to its own setter", taken, expected.join(",")
check "no name was stored instead of intercepted", enc.indexes.len, names.len

// A one-character difference must not collide with a neighbour's setter.
check "distinct setters, not one shared", names.len, 15

// ===========================================================================
print "2. chain depth"
// ---------------------------------------------------------------------------
// A setter must be found however deep it sits, and the most-derived one must
// win at every depth.

depth = 12
root = {}
root["x="] = recorder("d0")
chain = [root]
for i in range(1, depth)
	chain.push new chain[i - 1]
end for
check "chain built to the expected depth", chain.len, depth + 1

// Found from the far end of the chain.
leaf = new chain[depth]
leaf.x = 1
check "setter found " + (depth + 1) + " links up", taken, "d0<-1"

// Now override at each level from the top down; the nearest one must win.
for i in range(1, depth)
	chain[i]["x="] = recorder("d" + i)
	probe = new chain[depth]
	probe.x = 2
	check "most-derived setter wins after override at depth " + i, taken, "d" + i + "<-2"
end for

// ===========================================================================
print "3. super delegation order"
// ---------------------------------------------------------------------------
// `super.x = v` resumes the search ABOVE the running setter and, finding
// nothing, does the plain store into self.  A chain of delegating setters must
// run outermost-first and store exactly once, on the original receiver.

A = {}
A["x="] = function(v)
	note "A"
	super.x = v
end function
B = new A
B["x="] = function(v)
	note "B"
	super.x = v + 1
end function
C = new B
C["x="] = function(v)
	note "C"
	super.x = v * 10
end function

obj = new C
obj.x = 3
check "delegation runs most-derived first", taken, "C,B,A"
check "value transformed at each hop, stored once", obj.x, 31
check "stored on the receiver, not a prototype", C.hasIndex("x"), 0
check "prototype chain untouched", B.hasIndex("x") + A.hasIndex("x"), 0

// self stays the original receiver the whole way down.
selfCheck = {}
selfCheck["x="] = function(v)
	note "outer:" + refEquals(self, v)
	super.x = v
end function
sub = new selfCheck
sub["x="] = function(v)
	note "inner:" + refEquals(self, v)
	super.x = v
end function
inst = new sub
inst.x = inst
check "self is the receiver at every level", taken, "inner:1,outer:1"

// ===========================================================================
print "4. cache invalidation"
// ---------------------------------------------------------------------------
// Every assignment consults a per-map "does my chain have a setter?" stamp.
// Anything that could change that answer must retire the stamp.  These are the
// cases where a stale stamp would silently skip a setter.

// (a) map is stamped clean, then gains a setter of its own
a = {}
a.k = 1
a["x="] = recorder("a")
a.x = 2
check "setter added after the map was stamped clean", taken, "a<-2"

// (b) instance stamped clean, setter appears on the prototype
P = {}
inst2 = new P
inst2.k = 1
P["x="] = recorder("P")
inst2.x = 2
check "setter added to prototype after instance was stamped", taken, "P<-2"

// (c) same, but the setter lands several links above an already-stamped instance
Q0 = {}
Q1 = new Q0
Q2 = new Q1
q = new Q2
q.k = 1
Q0["x="] = recorder("Q0")
q.x = 2
check "setter added 4 links above a stamped instance", taken, "Q0<-2"

// (d) __isa rewired to a prototype that has setters
withSetter = {}
withSetter["x="] = recorder("R")
r = {}
r.k = 1
r.x = 9
check "no setter before rewiring", taken, ""
check "plain store happened", r.x, 9
r.__isa = withSetter
r.x = 2
check "rewiring __isa takes effect immediately", taken, "R<-2"

// (e) __isa rewired AWAY from a prototype that has setters
r.__isa = {}
r.x = 3
check "rewiring away from a setter restores plain storage", taken, ""
check "plain store after rewiring away", r.x, 3

// (f) instances created after the prototype gained its setter
S = {}
S["x="] = recorder("S")
fresh = new S
fresh.x = 1
check "instance created after the setter exists", taken, "S<-1"

// (g) removal
t = {}
t["x="] = recorder("t")
t.x = 1
check "setter fires before removal", taken, "t<-1"
remove t, "x="
t.x = 5
check "removal restores plain storage", taken, ""
check "plain store after removal", t.x, 5

// (h) removal on a prototype, seen through an instance
U = {}
U["x="] = recorder("U")
u = new U
u.x = 1
check "inherited setter fires", taken, "U<-1"
remove U, "x="
u.x = 6
check "removing a prototype's setter is seen by instances", taken, ""
check "plain store on the instance", u.x, 6

// ===========================================================================
print "5. assignment forms"
// ---------------------------------------------------------------------------
forms = {}
forms["x="] = recorder("f")
forms.x = 1                       // dot
forms["x"] = 2                    // bracket, constant key
k = "x"
forms[k] = 3                      // bracket, computed key
check "dot, constant bracket and computed bracket all intercept", taken, "f<-1,f<-2,f<-3"

// compound assignment reads normally and writes through the setter
comp = {"__v": 5}
comp.v = function; return self.__v; end function
comp["v="] = function(w)
	note "v<-" + w
	self.__v = w
end function
comp.v = comp.v + 3
check "compound assignment routes the write through the setter", taken, "v<-8"
check "compound assignment stored the computed value", comp.__v, 8

// every non-error return value is discarded, and the assignment still counts as
// handled -- nothing reaches storage.  (An error return halts instead; see the
// header note.)
rets = {}
rets["a="] = function(v); note "a"; return "a string"; end function
rets["b="] = function(v); note "b"; return 99; end function
rets["c="] = function(v); note "c"; return [1, 2]; end function
rets["d="] = function(v); note "d"; return {"k": 1}; end function
rets["e="] = function(v); note "e"; return null; end function
rets["f="] = function(v); note "f"; end function          // no return at all
rets.a = 1
rets.b = 2
rets.c = 3
rets.d = 4
rets.e = 5
rets.f = 6
check "every setter ran regardless of what it returned", taken, "a,b,c,d,e,f"
check "no return value leaked into storage", rets.indexes.len, 6

// a setter defined in a map literal
lit = {"y=": recorder("lit")}
litInst = new lit
litInst.y = 1
check "setter defined in a map literal", taken, "lit<-1"

// ===========================================================================
print "6. what is NOT intercepted"
// ---------------------------------------------------------------------------
guard = {}
guard["x="] = recorder("g")
guard["1="] = recorder("num")
guard["=="] = recorder("eq")

x = 42                            // plain variable, same name as the property
check "plain variable assignment", taken, ""
check "variable holds its value", x, 42

guard[1] = "numeric"              // non-string key
check "numeric key is not intercepted", taken, ""
check "numeric key stored", guard[1], "numeric"

guard["x="] = recorder("g2")      // storing a setter key is not intercepted
check "installing a setter does not search for x==", taken, ""
guard.x = 1
check "the replacement setter is the one that fires", taken, "g2<-1"

lst = [1, 2, 3]
lst[0] = 9                        // list index assignment
check "list assignment is not intercepted", taken, ""
check "list element stored", lst[0], 9

remove guard, 1                   // remove is not intercepted
check "remove is not intercepted", taken, ""

// setters on the shared type map must not intercept every map
map["z="] = recorder("typemap")
plain = {}
plain.z = 1
check "type map setters do not apply to all maps", taken, ""
check "plain store on an ordinary map", plain.z, 1
remove map, "z="

// ===========================================================================
print "7. getter/setter pairs"
// ---------------------------------------------------------------------------
Temp = {"__c": 0}
Temp.f = function; return self.__c * 9 / 5 + 32; end function
Temp["f="] = function(v); self.__c = (v - 32) * 5 / 9; end function

t1 = new Temp
t1.f = 212
check "setter converted on the way in", t1.__c, 100
check "getter converted on the way out", t1.f, 212
t1.f = 32
check "round trip at the other end", t1.__c, 0

// two instances do not share the backing field
t2 = new Temp
t2.f = 50
check "instances keep separate state (t1)", t1.f, 32
check "instances keep separate state (t2)", t2.f, 50
check "the prototype's own field is untouched", Temp.__c, 0

// a setter with no getter leaves reads going to plain storage
half = {}
half["n="] = function(v); self.__n = v * 2; end function
h = new half
h.n = 4
check "setter-only: backing field written", h.__n, 8
check "setter-only: the property itself was never stored", h.hasIndex("n"), 0

// ===========================================================================
print "8. stress: many maps, many generations"
// ---------------------------------------------------------------------------
// The cache is a per-map stamp compared against a global generation counter.
// Interleave "define a setter" (which retires every stamp) with assignments to
// unrelated maps, and confirm nothing is skipped or wrongly intercepted.

Proto = {}
Proto["p="] = recorder("P")
pool = []
for i in range(1, 60)
	pool.push new Proto
end for

hits = 0
for i in range(0, 59)
	pool[i].p = i                 // must intercept every time
	pool[i].q = i                 // must store every time
	if pool[i].q == i then hits = hits + 1
end for
check "every instance intercepted its setter", log.len, 60
check "every instance stored its plain key", hits, 60
taken

// Now churn the generation between assignments.
churn = {}
churnHits = 0
for i in range(0, 39)
	scratch = {}
	scratch["s" + i + "="] = recorder("s")   // bumps the generation each time
	churn["k" + i] = i                       // unrelated map, must stay plain
	if churn["k" + i] == i then churnHits = churnHits + 1
end for
check "generation churn does not disturb unrelated maps", churnHits, 40
check "generation churn intercepted nothing spurious", taken, ""

// A map that is stamped clean, re-stamped after churn, then gains a setter.
late = {}
for i in range(0, 9)
	late["f" + i] = i
	scratch2 = {}
	scratch2["z" + i + "="] = recorder("z")
end for
check "clean map survived repeated generation bumps", late.indexes.len, 10
late["f0="] = recorder("late")
late.f0 = 99
check "clean map still notices a setter added at the end", taken, "late<-99"

// ===========================================================================
print "9. aliasing and identity"
// ---------------------------------------------------------------------------
// Two names for the same map share its setter and its cache entry.
orig = {}
alias = orig
orig["x="] = recorder("alias")
alias.x = 1
check "setter installed via one name fires via the other", taken, "alias<-1"
alias["y="] = recorder("alias2")
orig.y = 2
check "and the other way round", taken, "alias2<-2"

// A prototype used by two separate hierarchies.
Shared = {}
Shared["x="] = recorder("shared")
H1 = new Shared
H2 = new Shared
h1 = new H1
h2 = new H2
h1.x = 1
h2.x = 2
check "one prototype serves two hierarchies", taken, "shared<-1,shared<-2"

// Overriding in one hierarchy must not affect the other.
H1["x="] = recorder("h1only")
h1b = new H1
h2b = new H2
h1b.x = 3
h2b.x = 4
check "override is confined to its own hierarchy", taken, "h1only<-3,shared<-4"

// ===========================================================================
print ""
if failures == 0 then
	print "OK -- " + checks + " checks passed"
else
	print "FAILED -- " + failures + " of " + checks + " checks failed"
	exit 1
end if
