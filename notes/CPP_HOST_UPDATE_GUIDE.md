# C++ Host App Update Guide

This document contains notes for developers of C++ apps that embed MiniScript, and want to update their existing code to MS2.

## Creating and Managing The Interpreter

### Include just `miniscript.h`

MS1 hosts included specific headers such as `MiniscriptInterpreter.h` and
`MiniscriptIntrinsics.h`.  In MS2, include the single public umbrella header:
```cpp
#include "miniscript.h"
```
Do **not** include the individual `*.g.h` files directly.  Those are transpiler
output; their names and the way code is split across them track the C# sources
and may change between releases.  `miniscript.h` is hand-written and stable, and
pulls in what a host needs.  Your compiler's include path still needs the `cpp`,
`cpp/core`, and `generated` directories (the generated files include each other,
and `core_includes.h`, by bare name).

### Create with `Interpreter::New`, not `new Interpreter`

In MS1 the interpreter was a heap object, with output delegates set as fields:
```cpp
// MiniScript 1.x
Interpreter *interp = new Interpreter();
interp->standardOutput = &Print;
interp->errorOutput   = &PrintErr;
interp->implicitOutput = &Print;
```
In MS2, `Interpreter` is a lightweight **value type** wrapping a shared pointer.
Watch out: the default constructor makes a *null* interpreter, so `new
Interpreter()` compiles but gives you a null-storage object that crashes on
first use.  Create one with the `New` factory and hold it by value; set the
output delegates through their `set_...` accessors (or pass them to `New`):
```cpp
// MiniScript 2
Interpreter interp = Interpreter::New();
interp.set_standardOutput(&Print);
interp.set_errorOutput(&PrintErr);
interp.set_implicitOutput(&Print);
```
Member access is now `.` rather than `->`, and you test for a null interpreter
with `IsNull(interp)`.

### Output callbacks take `Boolean`, not `bool`

`TextOutputMethod` is `void (*)(String, Boolean)`.  `Boolean` is a distinct
struct (it is required to make the C# transpilation work), and a function
pointer to `void Print(String, bool)` will not bind to it.  Change the second
parameter's type:
```cpp
void Print(String s, Boolean lineBreak) { ... }
```

### Errors are reported, not thrown

MS1 could throw a `MiniscriptException`, which hosts typically caught around
`RunUntilDone`:
```cpp
// MiniScript 1.x
try {
    interp->RunUntilDone(0.1, true);
} catch (MiniscriptException& mse) {
    ShowError(mse.message);
}
```
MS2 does **not** throw for compiler or runtime errors, and there is no
`MiniscriptException` type.  All errors are delivered through the `errorOutput`
delegate; the most recent error is also retained as an error `Value`, available
via `interp.Error()` (whose `__isa` chain you can inspect to distinguish error
types).  Remove the try/catch and do your error handling in the `errorOutput`
callback:
```cpp
// MiniScript 2
interp.RunUntilDone(0.1, true);   // errors surface via errorOutput / interp.Error()
```
Because `MiniscriptException` no longer exists, a leftover `catch` block will
fail to compile rather than silently going dead — a small nudge to migrate the
logic into your error callback.

### Checking for completion: `Done()`

`interp.Done()` returns true when there is no VM, or the VM has reached the end
of its code — the same meaning as in 1.x.  (`Running()` is its inverse.)  So a
main loop needs only the `->` → `.` change:
```cpp
if (!interp.Done()) interp.RunUntilDone(0.1, true);
```

### Host metadata

Set your host identity before the first use of the `version` intrinsic, as in
1.x — these are still MiniScript globals:
```cpp
MiniScript::hostName    = "raylib-miniscript";
MiniScript::hostInfo    = "https://github.com/JoeStrout/raylib-miniscript";
MiniScript::hostVersion = "0.3";   // NOTE: a String now, not a double
```
The one change: `hostVersion` is a `String`, not a `double` (the 1.x double
caused formatting pain, so it is now whatever string you want to report).

### Stack traces

The 1.x helper `Intrinsics::StackList(interp->vm)` is replaced by methods on the
VM: `interp.vm().BuildStackTrace()` (or `CurrentStackTrace()`), which return the
trace as a `Value` rather than a `ValueList`.

## Updating Intrinsic Code

Much of the shape of MS1 intrinsic code carries over to MS2, but there are a few
mechanical changes.  Consider this MS1 intrinsic that registers a `Wrap` function
into a module map:

**MiniScript 1.x:**
```
	Intrinsic *i = Intrinsic::Create("Wrap");
	i->AddParam("value", Value::zero);
	i->AddParam("min", Value::zero);
	i->AddParam("max", Value(1.0));
	i->code = [](Context *context, IntrinsicResult partialResult) -> IntrinsicResult {
		float value = context->GetVar(String("value")).FloatValue();
		float min = context->GetVar(String("min")).FloatValue();
		float max = context->GetVar(String("max")).FloatValue();
		return IntrinsicResult(Wrap(value, min, max));
	};
	raylibModule.SetValue("Wrap", i->GetFunc());
```

The MS2 equivalent:

**MiniScript 2:**
```
	Intrinsic i = Intrinsic::Create("Wrap");
	i.AddParam("value", Value::zero);
	i.AddParam("min", Value::zero);
	i.AddParam("max", Value(1.0));
	i.set_Code(INTRINSIC_LAMBDA {
		float value = context.GetVar(String("value")).FloatValue();
		float min = context.GetVar(String("min")).FloatValue();
		float max = context.GetVar(String("max")).FloatValue();
		return IntrinsicResult(Wrap(value, min, max));
	});
	raylibModule.SetValue("Wrap", i.GetFunc());
```

where `INTRINSIC_LAMBDA` is defined as:
```
#define INTRINSIC_LAMBDA [](MiniScript::Context context, \
    MiniScript::IntrinsicResult partialResult) -> MiniScript::IntrinsicResult
```

The changes to note:

- `Intrinsic` is now a **value type**, not a pointer: `Intrinsic i = Intrinsic::Create(...)`, not `Intrinsic *i = ...`.  Consequently every `i->` becomes `i.`, which is a straightforward search & replace.
- The one exception is the function body.  MS1's assignable `code` field is now a `Code` **property**, so `i->code = <lambda>` becomes `i.set_Code(<lambda>)` — note the capital `C`, and the setter call rather than a field assignment.  (`Code`'s type, `NativeCallbackDelegate`, is a plain function pointer, so the capture-less lambda that `INTRINSIC_LAMBDA` expands to binds to it directly.)
- The intrinsic callback signature changed: the first parameter is now `Context` (a value) rather than `Context *`, to better match C#.  `Context` does **not** overload `->`, so `context->GetVar(...)` / `context->GetArg(...)` become `context.GetVar(...)` / `context.GetArg(...)`.

### GetArg vs. GetVar

The example above uses `context.GetVar(String("value"))` to look up arguments by
name, mirroring the 1.x style.  This still works: `GetVar` searches the current
function's parameter list and returns the matching argument.  However, it is the
*slow* path — it does a linear, string-compare search of the parameter names on
every call.

New code should prefer `context.GetArg(i)`, which fetches the argument by
position (the first declared parameter is index 0, the second is index 1, and so
on) with a direct array index.  The order matches your `AddParam` calls, so the
example body is more efficiently written as:
```
	float value = context.GetArg(0).FloatValue();
	float min   = context.GetArg(1).FloatValue();
	float max   = context.GetArg(2).FloatValue();
```
Reserve `GetVar` for cases where you genuinely need name-based lookup.

### Handling Module Maps

A very common pattern is to group a set of unnamed functions into a "module", i.e. a map.  In MS1, you could just declare a static ValueDict, fill that out once, and then return it as an IntrinsicResult, implicitly wrapping it in a fresh (but identical) Value every time.

**MiniScript 1.x:**
```
static IntrinsicResult intrinsic_Foo(Context *context, IntrinsicResult partialResult) {
	return IntrinsicResult(FooModule());  // where FooModule() returns a ValueDict
}
```

In MS2, that exact pattern does not work; wrapping a ValueDict in a Value is a bit more expensive, and (more importantly), it needs to be added as a root value for the GC system, or else its contents will get garbage collected.  So the correct pattern is: convert the ValueDict to a Value, and call `GCManager::AddRoot` on it, only once.  Then cache that result and return it every time it is needed.

**MiniScript 2 (the harder way):**
```
static Value fooModule = Value::Null;
static IntrinsicResult intrinsic_Foo(Context context, IntrinsicResult partialResult) {
	if (fooModule.IsNull()) {
		fooModule = Value(FooModule());  // where KeyModule() returns a ValueDict
		GCManager::AddRoot(fooModule);
	}
	return IntrinsicResult(fooModule);
}
```

However, there is an easier route; a new `StaticMap()` function will do that Value-wrapping, AddRoot-calling, and caching for you.  So you can just do this:

**MiniScript 2 (the easy way):**
```
static IntrinsicResult intrinsic_Foo(Context context, IntrinsicResult partialResult) {
	return IntrinsicResult(StaticMap(FooModule()));
}
```

The easy way has _slightly_ worse performance, in that it does a hashtable lookup (among all your static maps) on each call.  In most cases that overhead is trivial, but if you're really worried about it, you can use the harder way above.

### Returning a *fresh* map or list: `DynamicMap` / `DynamicList`

`StaticMap` is only for maps you build **once** and hand back on every call (a
module, or a type/class prototype used with `isa`) — it roots the map so it is
never collected, and returns the same identity each time.

Very often, though, an intrinsic builds a **new** map or list each call and
returns it as data (e.g. `mousePos` returning `{x, y}`, or a function returning a
list of results).  In MS1 you wrapped these with `Value(dict)` / `Value(list)`.
In MS2 that implicit wrap is gone; use the collectable helpers, which wrap the
container in a Value **without** rooting it, so it is freed once the script drops
its last reference:

```cpp
ValueDict result;
result.SetValue("x", Value(x));
result.SetValue("y", Value(y));
return IntrinsicResult(DynamicMap(result));   // fresh, collectable map

ValueList out;
out.Add(Value(a)); out.Add(Value(b));
return IntrinsicResult(DynamicList(out));      // fresh, collectable list
```

Rule of thumb:

| The container is…                                   | Use          |
|-----------------------------------------------------|--------------|
| built once, returned every call (module/prototype)  | `StaticMap`  |
| a class prototype assigned to `__isa`               | `StaticMap`  |
| built fresh each call and returned as data          | `DynamicMap` / `DynamicList` |

Like `StaticMap`, both take a non-const reference, so pass an lvalue (a named
local), not a temporary.  For nested containers (a list of maps, a map with list
fields), wrap each level with the matching helper.  For an empty-list default
parameter value, use `Value::make_list(0)` (there is no `ValueList()` to wrap).

### The instance-creation pattern

Host "constructor" helpers that wrap a native object in a MiniScript map recur
constantly.  They set `__isa` to a shared class prototype and return a fresh
instance, so they use **both** helpers:

```cpp
// MiniScript 2
Value TextureToValue(Texture tex) {
    ValueDict map;
    map.SetValue(Value::magicIsA, StaticMap(TextureClass())); // shared prototype
    map.SetValue("_handle", Value((double)(intptr_t)new Texture(tex)));
    map.SetValue("width",  Value(tex.width));
    return DynamicMap(map);                                    // fresh instance
}
```

For this to work, the `*Class()` accessor must return `ValueDict&` (a reference
to its internal `static ValueDict`), so `StaticMap` gets a stable lvalue whose
address keys the cache — change `ValueDict FooClass();` to `ValueDict& FooClass();`.

## Reading Maps and Lists Back Out

MS1's `Value::GetDict()` and `Value::GetList()` are still available and still
return a `ValueDict` / `ValueList`.  They now return a **shared view** of the
Value's backing storage, so mutating the returned container writes back to the
Value (as in 1.x):

```cpp
ValueList pts = context.GetArg(0).GetList();   // iterate points
ValueDict m   = self.GetDict();
m.SetValue("width", Value(w));                 // writes back into `self`
```

To iterate a map, MS1's `GetIterator()` / `ValueDictIterator` are replaced by a
range-for over `Keys()`:

```cpp
// MiniScript 1.x:  for (auto it = d.GetIterator(); !it.Done(); it.Next()) { it.Key(); it.Value(); }
for (Value key : d.Keys()) {
    Value val = d.Lookup(key, Value::Null);
    ...
}
```

`ValueDict::Lookup(key, defaultValue)` (map lookup with a default) and the
single-argument `Value::Lookup(key)` (returns the mapped value or null) both
carry over from 1.x.

## Reporting Runtime Errors

MS1 intrinsics raised errors by throwing — `RuntimeException("msg").raise()`,
`IndexException().raise()`, etc.  MS2 has no exceptions.  Instead, tell the VM to
raise a runtime error and then **return** from the intrinsic:

```cpp
// MiniScript 1.x:  if (bad) RuntimeException("bad arg").raise();
if (bad) { context.vm.RaiseRuntimeError("bad arg"); return IntrinsicResult::Null; }
```

**The `return` is mandatory.**  Unlike a thrown exception, `RaiseRuntimeError`
does *not* unwind the C++ stack — execution continues on the next line — so code
after it will run (with the VM already in an error state) unless you return.

When one intrinsic has many such checks, a small local helper keeps it tidy:

```cpp
static IntrinsicResult raiseError(Context context, const char* msg) {
    context.vm.RaiseRuntimeError(msg);
    return IntrinsicResult::Null;
}
...
if (offset >= data->length) return raiseError(context, "index out of range");
```

### Errors from a helper that has no `Context`

A plain helper (no `Context`/VM handle) cannot call `RaiseRuntimeError`.  MS1
helpers that threw must change how they signal failure.  Two good options:

- **Return the error outward.**  Add an out-parameter (or return a status) and
  let the calling intrinsic decide.  For example, an HTTP helper that in 1.x
  "returned the body or threw" becomes one that returns the body and sets a
  `std::string* errOut`; the intrinsic then reports or returns the message.
- **Guard at the call site.**  If only one intrinsic calls the helper, check the
  precondition in that intrinsic (where a `Context` is available) and
  `RaiseRuntimeError` there, leaving the helper a defensive no-op.

(An intrinsic that in 1.x wrapped a call in `try { ... } catch (MiniscriptException&)`
has no exception to catch in MS2 — replace the try/catch with a status check.)

## Opaque Handles (wrapping native pointers)

MS1 wrapped native resources with a `RefCountedStorage` subclass and
`Value::NewHandle(new MyStorage(...))`, reading the pointer back via
`value.data.ref`.  MS2 models a handle as a first-class GC value with a
**finalizer** callback (invoked when the handle is collected), and there is no
`RefCountedStorage`.  Use the `Value::NewHandle` / `HandlePtr` wrappers:

```cpp
// MiniScript 1.x:
//   class FileHandle : public RefCountedStorage { FILE* f; ~FileHandle(){ if(f) fclose(f);} };
//   Value h = Value::NewHandle(new FileHandle(fp));
//   FileHandle* s = (FileHandle*)h.data.ref;
// MiniScript 2:
struct FileHandle { FILE* f; FileHandle(FILE* x):f(x){} ~FileHandle(){ if(f) fclose(f);} };
Value h = Value::NewHandle(new FileHandle(fp), [](void* p){ delete (FileHandle*)p; });
FileHandle* s = (FileHandle*)h.HandlePtr();      // null if h is not a handle
if (h.Type() == ValueType::Handle) { ... }
```

The finalizer runs during garbage collection, so it replaces MS1's
destructor-on-last-release; do your `fclose`/`free`/`delete` there.  (If you only
need to stash a raw pointer as data — not own a resource — you can still just
store it as a number, e.g. `Value((double)(intptr_t)ptr)`.)

## Value Types and Small Gotchas

- **String Values are implicit again.**  `Value("literal")` and
  `Value(someString)` both work (the `const char*` constructor is no longer
  `explicit`), so map keys like `d.SetValue("width", v)` need no change.
- **`value.type` → `value.Type()`.**  MS2 has no `type` field; use the accessor,
  which returns a `ValueType` (`Null, Number, String, List, Map, Function,
  Handle, Error` — note `Error` is new).  So `v.type == ValueType::Map` becomes
  `v.Type() == ValueType::Map`.  (Idiomatic MS2 also offers `v.IsMap()`,
  `v.IsList()`, `v.IsNull()`, … predicates.)
- **Integer widths.**  `Value` has constructors for `int`, `unsigned`, and
  `double`, but not `long`/`long long`/`size_t`/`off_t`, which are therefore
  *ambiguous*.  Cast wide integers explicitly: `Value((double)fileSize)`.
- **`Value::GetString()` → `Value::ToString()`**; **`list.Item(i)` → `list[i]`**.
- **`Value::null` → `Value::Null`** (capital N, matching C#).

## Mechanical search-and-replace cheat sheet

Most of a module ports with these textual substitutions (verify each in context):

| MiniScript 1.x                         | MiniScript 2                              |
|----------------------------------------|-------------------------------------------|
| `#include "Miniscript*.h"`, `SimpleString.h` | `#include "miniscript.h"`           |
| `Intrinsic *i = Intrinsic::Create(...)`| `Intrinsic i = Intrinsic::Create(...)`    |
| `i->AddParam(...)`, `i->GetFunc()`     | `i.AddParam(...)`, `i.GetFunc()`          |
| `i->code = <lambda>;`                  | `i.set_Code(<lambda>);`                   |
| `Context *context`, `context->X`       | `Context context`, `context.X`            |
| `value.type == ValueType::Map`         | `value.Type() == ValueType::Map`          |
| `Value::null`                          | `Value::Null`                             |
| `Value(freshDict)` / `Value(freshList)`| `DynamicMap(freshDict)` / `DynamicList(freshList)` |
| `IntrinsicResult(ModuleDict())`        | `IntrinsicResult(StaticMap(ModuleDict()))`|
| `RuntimeException("m").raise();`       | `context.vm.RaiseRuntimeError("m"); return IntrinsicResult::Null;` |
| `d.GetIterator()` / `ValueDictIterator`| `for (Value k : d.Keys())` + `d.Lookup(k, Value::Null)` |
| `Value::NewHandle(new S(...))`         | `Value::NewHandle(new S(...), [](void*p){delete (S*)p;})` |
| `v.data.ref`                           | `v.HandlePtr()`                           |
| `try { ... } catch (MiniscriptException&)` | remove; check status / error-return   |
