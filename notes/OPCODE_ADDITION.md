## Overview

For MS2Proto3, opcodes must be added very specifically so that all operations work correctly from assembly to execution.

## Procedure

To add a new opcode, the following must be implemented:

1. cs/Bytecode.cs: New opcodes must be added to `Opcode` enum. They must follow the existing format, with `r` indicating a register, `k` a constant table lookup, and `i` an immediate value. Also, matching entries must be placed in `BytecodeUtil.ToMnemonic()` and `BytecodeUtil.FromMnemonic()`.
2. cpp/core/dispatch_macros.h: The raw opcodes must be added to the list of macros in `VM_OpCodes(X)`.
3. cs/Assembler.cs: The assembly version of the opcodes must be implemented in `Assembler.AddLine()`.
4. cs/Disassembler.cs: The raw opcodes and assembly opcodes must be matched together in both `Disassembler.AssemOp()` and `Disassembler.ToString()`.
5. cs/VM.cs: Implementation of the raw opcodes must be completed in `VM.Execute()`
6. VM_DESIGN.md: The new opcodes should be documented.

Opcodes should be implemented in the same order across all files, *as best as possible*. If your new opcode comes immediately after the JUMP opcode in cs/Bytecode.cs, then it should be implemented after JUMP in cs/VM.cs, for example.

## Additional Encoding/Decoding

Sometimes you'll need to add an additional encoding or decoding operation in `cs/Bytecode.cs`.

## Raw Opcodes vs Assembly Opcodes

`LOAD` is an assembly opcode. Depending on how it'll be used, it will be translated into one of the following raw opcodes: `LOAD_rA_rB`, `LOAD_rA_iBC`, or `LOAD_rA_kBC`.

## Example

Let's say we want to implement an INC opcode that increments a register's value by 1. The assembly opcode would be `INC`, but the raw opcode interpreted by the VM would be `INC_rA`. Let's assume we only know how to add support for integers just now, and not for any other data types. We can begin to add the opcode like this:

### cs/Bytecode.cs

Inside `Opcode`, we would add `INC_rA` somewhere near the math opcodes, perhaps like this:

```cs
        ADD_rA_rB_rC,
		SUB_rA_rB_rC,
		MULT_rA_rB_rC,
		DIV_rA_rB_rC,
		MOD_rA_rB_rC,
        INC_rA, // Our added opcode
        JUMP_iABC,
```

Inside `BytecodeUtil.ToMnemonic()`, we add the following:

```cs
case Opcode.INC_rA:     return "INC_rA";
```

Inside `BytecodeUtil.FromMnemonic()`, we add the following:

```cs
if (s == "INC_rA")     return Opcode.INC_rA;
```

### cs/Assembler.cs:

We can implement the assembly side like this:

```cs
else if (mnemonic == "INC") {
    if (parts.Count != 2) return Error("Syntax error", mnemonic, line);
    Byte reg = ParseRegister(parts[1]);
    instruction = BytecodeUtil.INS_A(Opcode.INC_rA, reg);
}
```

*Note: If the encoding function `BytecodeUtil.INS_A` doesn't exist, then we'll need to go back to `cs/Bytecode.cs` and add it.*

### cs/Disassembler.cs

We'll add the following to `Disassembler.AssemOp()`:

```cs
case Opcode.INC_rA: return "INC";
```

We'll also add the following to `Disassembler.ToString()`:

```cs
case Opcode.INC_rA:
    return StringUtils.Format("{0} r{1}",
        mnemonic,
        (Int32)BytecodeUtil.Au(instruction));
```

### cs/VM.cs

We'll add the following to `VM.Execute()`:

```cs
case Opcode.INC_rA: { // CPP: VM_CASE(INC_rA) {
    // R[A]++
    Byte a = BytecodeUtil.Au(instruction);
    stack[baseIndex + a] = value_add(stack[baseIndex + a], val_one);

    // We assume R[A] is an integer.
    // TODO: Add support for other data types, like strings.

    break; // CPP: VM_NEXT();
}
```

### cpp/core/dispatch_macros.h

We'll add the following to the `VM_OpCodes` macro, in the same place we added it in `Opcode` in `cs/Bytecode.cs`:

```cs
X(INC_rA) \
```

### VM_DESIGN.md

We'll add the following to the opcode documentation table:

| INC_rA | R[A]++ |
