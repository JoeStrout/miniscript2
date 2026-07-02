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
