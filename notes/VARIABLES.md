# Variables in the MiniScript VM

Variables in MiniScript are a bit tricky, because:

1. They can also be accessed via maps (`locals`, `outer`, and `globals`)
2. Those maps can be passed around, assigned other references, etc.
3. Any read of a variable without `@` will invoke the function, if the variable happens to refer to one.

MiniScript 2.0 uses a register-based VM, so we want to map variables to registers (i.e. fixed locations on the VM's value stack) and use them with simple, fast opcodes wherever possible.  But we need to maintain the above observable behavior.

This document attempts to develop an approach to making all this work, while maintaining good performance.

## Register Metadata

The VM maintains a stack of Values.  Each call frame gets a section of this stack, which maps to its Register 0, Register 1, ... Register N.  Assembly instructions like `LOAD r1, 42` load a value (42) into a particular slot in this stack (the slot which is Register 1 for the current call frame).

We will need to extend this stack with one extra value for each slot: the **name** of the local variable associated with that slot, if any.  This is set only while that variable has a value assigned.

A MiniScript assignment statement actually assigns the name string to the target register (in addition to copying the value).

This leaves open the possibility of reusing a register for different variables at different times, if we can determine (e.g. through static analysis) than an older variable is no longer needed.  (Note: variable lifetime analysis is totally doable, since unless a function either (1) makes use of `locals`, or (2) defines an inner `function`, there is no way any other code can access its locals.)

## VarMaps

Ordinary maps map (arbitrary) keys to values.  A **VarMap** will be a specialized kind of map (perhaps a subclass) which does that, but also maps some (string) keys to slots on the stack.

When looking up a value by key, a VarMap will first see if this is one of the keys it has mapped to a slot.  If so, then it will return the value in that slot if assigned, or throw a key-not-found error if it is not assigned.  If it's not one of those special keys, then it will behave as an ordinary map.

When storing a value by key, a VarMap will first see if this is one of the keys it has mapped to a slot.  If so, it will store the value directly into that slot on the stack, (and ensure that slot is named).  If not, then it will store it as an ordinary map value.

A VarMap supports a "gather" operation that reads the current values (if they are assigned) of all its special keys, and stores them as ordinary map key/value pairs, clearing out its special keys.  After this operation, the VarMap behaves no different than any ordinary map.

(This is how we will support `locals` and `outer` -- see the situations below.)


## Situations (Use Cases)

The following sub-sections each describe a "situation" -- a particular use case involving one or more variables in MiniScript code.  Each will be given a description of the situation (with MiniScript code when possible), a conceptual description of how we can handle it, and (eventually) the VM assembly code that would be used.

The rarity of each case is noted.  We assume that in ordinary code, the common case is to use variables in a simple manner with no special prefixes: `x = y + z`, etc.  Invoking functions is also common (`x = myFunc(y, z)`).  Everything else is rare to some degree.

### Variable assignment (common)

MiniScript code:

```
x = 42
```

#### Concept

The compiler has decided to store `x` in some register, let's say **r7**.  This assignment simply stores the RHS value in the slot, and sets the register name to "x".  In assembly code, there are two ways it might do this: an ASSIGN opcode that stores the value and name at the same time; or a LOAD followed by a NAME.

#### Assembly

```
LOAD r12, 42          # load the value into a temp register
ASSIGN r7, r12, "x"   # copy value into register 7 and name it "x"
```

or:

```
LOAD r7, 42    # load the value into the register 7
NAME r7, "x"   # ensure register 7 is named "x"
```

### `@`-protected variable read (rare-ish)

MiniScript code:

```
_ = @x
```

Possibly an ordinary read (e.g. `_ = x`) could fall into this situation too, if by static analysis the compiler could determine that `x` cannot contain a function reference.

#### Concept

The compiler has mapped `x` to some register, let's say **r7**.  In the register metadata, this slot will have a **name** of "x", assuming the variable has already been set and not subsequently cleared.  In compiling the expression on the RHS of this example, we would want to get the value of `x` into some temp register, let's say **r13**.  Because of `@` (or if, through some static analysis, we know that `x` cannot hold a function reference), we don't want to invoke any function; we just want to get the value as it is stored.

So this is basically a `LOAD`, but we need to correctly handle the case where the variable has been cleared (for example, we have passed `locals` to some function that removed the "x" index).  MiniScript code illustrating this scenario:

```
noXForYou = function(m)
  m.remove "x"
end function
x = 42
noXForYou locals
x
--> Runtime Error: Undefined Identifier: 'x' is unknown in this context
```

To handle this, we will have a `LOADV` opcode, similar to `LOAD` except that it validates the name of the register it is loading from:

1. If the source register name matches the LOADV name, use its value as-is.
2. If not, then look for the same name in the _outer_ stack frame, and then in _globals_. Use the value found, or if not found, throw an undefined-identifier error.

Note that this design leaves only 8 bits for the name constant; thus no function context can have more than 256 variable names, and the compiler will need to sort these to the start of the constants list.

#### Assembly

```
LOADV r13, r7, "x"    # get value from r7 (named x) into r13 (a temp register)
```

### Ordinary variable read (common)

MiniScript code:
```
_ = x
```

#### Concept

This is very similar to above, except that in case `x` is a function reference, we want to actually invoke that function (with no parameters) and use the return value.  We'll do this with a special opcode, `LOADC` (load-and-call).  This will go through the same process as `LOADV` (above) to find the value, and if the value found is not a function reference, it stores it in the target register just like `LOADV`.  But if it *is* a function reference, then it invokes that function, and stores the result, similar to a `CALLF` + `LOAD`.

This opcode needs the same arguments as LOADV: where to store the value (*destRegister*), where to get the value or function reference (*srcRegister*), and the name that identifies the variable we are trying to load (*name*).  That uses up all the space for fields in our opcode; so how do we know where to tell the called function to start its registers in the stack?  I think we'll need to use the `maxRegs` property of the FuncDef (of the current function, not the one we're calling).  The compiler/assembler will need to keep track of the highest register referenced in any way by the code, and then implicit calls will start their registers at `maxRegs+1`.

In order to copy the result into *destRegister* after the call, the call stack will need to include an extra piece of data that means "upon return to this frame, copy from *maxRegs+1* to *destRegister*".

#### Assembly

```
LOADC r13, r7, "x"   # get value from r7 (x), calling if needed, and store in r13
```

### Explicit assignment to local via locals (rare-ish)

MiniScript code:
```
locals.x = 42
```

#### Concept

This is actually exactly equivalent to ordinary `x = 42`.  The compiler should just ignore `locals.` in this case and compile it to the same as the common case.

### Other use of `locals`

Every stack frame will have a weak reference to a VarMap representing its locals.  In common cases, this VarMap will never be created.

Apart from the above trivial case of assignment to `locals.x`, any other use of the `locals` keyword should see if the current stack frame already has a locals VarMap.  If not, then it will create it on the spot, using the **name** metadata for the current registers.  It then pushes a reference to this map into the temp register representing `locals` in the expression, and proceeds compiling the dot operator as for any other map.

#### Assembly

```
LOCALS r1     # get a reference to the `locals` VarMap, and put in r1
```


### Explicit variable read via locals (rare)

MiniScript code:
```
_ = locals.x
```

There isn't much reason to write code like this, since if `x` is a local variable, an ordinary reference would find it; and if it's not, then this code would crash with a key-not-found error.

#### Concept

So, we should treat this as first referencing the `locals` VarMap (creating it if necessary), and then doing a lookup in that.

#### Assembly

```
LOCALS r1     # get a reference to the `locals` VarMap, and put in r1
LOAD r2, "x"
INDEX r13, r1, r2   # r13 = locals.x
```

### Passing locals to another function (rare)

This situation does come up occasionally when using `string.fill` (an extension method in stringUtil).

MiniScript code:
```
name = "Bob"
print "Hello {name}!".fill(locals)
```

#### Concept

This is just another use of the `locals` VarMap.  As a parameter, it gets stuffed into whatever register we're using for function arguments, and the called code picks it up as **r0**.  Reads and writes via that map look like ordinary map indexing, but because it's a VarMap, it reaches into the stack and reads/writes the appropriate slots in the calling function registers.

#### Assembly

```
LOAD r7, "Bob"   # name = "Bob"; assumes name has been mapped to r7
LOAD r17, "Hello {name}!"   # put string into a stack top (func call/return point)
LOCALS r18       # get `locals` into r18 (second parameter)
CALLFN 17, "fill"  # call "fill" with args starting at r17
CALLFN 17, "print" # call "print" with result of previous call
```

### Assignment of outer variable (rare)

Care must be taken here: the `outer` scope is not the previous stack frame, but rather, the stack frame that was current when the function was initially defined.

So, part of the code behind the `function` keyword will grab the VarMap of the current  function, and store it as the `outer` context of the new function.  Note that if that function is invoked while the outer function is still on the stack, then it will be able to read and write live variables.  If the outer function exits before the new function is invoked, that's OK too; in this case the VarMap will have gathered its values and be acting like an ordinary map.

In assembly code, I think we'll need a special opcode just for this.  Maybe SETOUT, given a function reference, takes the local VarMap and stuffs it into that FuncDef.


### Assignment of global variable (common-ish)

`globals` should probably be a keyword, but it essentially evaluates to the VarMap of the bottommost call frame.  Like with any VarMap, you can update or store new key/value pairs in it via assignment with dot syntax.  If the given identifier was already mapped to a register in that call frame, then it will update the register; otherwise, it will just store the value in the map.

### Read of global variable (common)

When you write `_ = globals.foo`, it compiles as a LOOKUP opcode on the `globals` map (with "foo" as a string constant key).  As that map is really a VarMap, this can either return a value from a register, or from one of the extra key/value pairs stored on the map.  If it is not found in either place, then an Undefined Identifier error occurs.

The exception would be: if the compiler knows that `foo` is already assigned to a register, then it returns the value from that register directly.  This might be possible only for code at the global scope (since register references are 0-based from the current stack frame).

### Deleting local variable (rare)

MiniScript code:
```
foo = 42
locals.remove "foo"
```

#### Concept

This uses the standard intrinsic for removing a map key, but as `locals` is a VarMap, if the variable is mapped to a register, then it just clears the **assigned flag** for that register.  (If it's not mapped to a register, then it just does ordinary map behavior.)

This does mean that any register reads in our VM have to check the assigned flag, because if it's false, we need to throw an Undefined Identifier error.


### Deleting outer variable (rare)

As above.

### Deleting global variable (rare-ish)

Also as above.
