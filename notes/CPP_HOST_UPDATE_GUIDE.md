# C++ Host App Update Guide

This document contains notes for developers of C++ apps that embed MiniScript, and want to update their existing code to MS2.

## Creating and Managing The Interpreter

TODO

## Updating Intrinsic Code

Many common patterns from MS1 host code work without modification in MS2.  For example, this code:
```
	i = Intrinsic::Create("");
	i->AddParam("value", Value::zero);
	i->AddParam("min", Value::zero);
	i->AddParam("max", Value(1.0));
	i->code = INTRINSIC_LAMBDA {
		float value = context->GetVar(String("value")).FloatValue();
		float min = context->GetVar(String("min")).FloatValue();
		float max = context->GetVar(String("max")).FloatValue();
		return IntrinsicResult(Wrap(value, min, max));
	};
	raylibModule.SetValue("Wrap", i->GetFunc());
```

works fine in both MS1 and MS2, provided that for MS2 we define:
```
#define INTRINSIC_LAMBDA [](MiniScript::Context context, \
    MiniScript::IntrinsicResult partialResult) -> MiniScript::IntrinsicResult
```

(The intrinsic function interface has changed slightly: the first parameter is of type `Context`, rather than `Context*`, to better match C#.  But the `Context` class supports the `->` operator even on non-pointer references, so the rest of your code should still work.)

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
	float value = as_double(context.GetArg(0));
	float min   = as_double(context.GetArg(1));
	float max   = as_double(context.GetArg(2));
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
static Value fooModule = Value::null;
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
static IntrinsicResult intrinsic_Foo(Context *context, IntrinsicResult partialResult) {
	return IntrinsicResult(StaticMap(FooModule()));
}
```

The easy way has _slightly_ worse performance, in that it does a hashtable lookup (among all your static maps) on each call.  In most cases that overhead is trivial, but if you're really worried about it, you can use the harder way above.
