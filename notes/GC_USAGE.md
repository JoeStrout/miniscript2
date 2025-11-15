# Garbage Collection Usage Guide

This document explains how to properly use the garbage collection (GC) system in the c-nan-boxing-2u implementation.

## Overview

The GC system uses a **shadow stack** approach where you explicitly register local `Value` variables with the garbage collector. The GC can only run at safe points (during allocation or explicit collection), ensuring that all registered values remain valid.

## Basic Usage Pattern

Every function that creates or manipulates `Value` objects should follow this pattern:

```c
void my_function() {
    GC_PUSH_SCOPE();                    // 1. Start GC scope
    GC_LOCALS(var1, var2, var3);       // 2. Declare and protect locals
    
    var1 = make_string("hello");       // 3. Use the variables normally
    var2 = make_list(10);
    var3 = string_concat(var1, var2);
    
    // ... rest of function logic
    
    GC_POP_SCOPE();                     // 4. End GC scope before return
}
```

## Required Steps

### 1. Initialize GC
Call `gc_init()` at the start of your program, and `gc_shutdown()` on exit:
```c
int main() {
    gc_init();
    // ... your program logic
    gc_shutdown();
    return 0;
}
```

### 2. Scope Management
- **`GC_PUSH_SCOPE()`**: Call at the beginning of every function that uses `Value` objects
- **`GC_POP_SCOPE()`**: Call before every return statement in the function

### 3. Variable Protection
Use **`GC_LOCALS(var1, var2, ...)`** to declare and automatically protect up to 16 local variables:
```c
GC_LOCALS(str, list, result, item);
str = make_string("test");
list = make_list(5);
// Variables are automatically tracked by GC
```

For more than 16 variables, you can use GC_LOCALS multiple times.  Or, for individual protection of a single variable at a time, you can use GC_PROTECT:
```c
Value my_var = make_null();
GC_PROTECT(&my_var);  // Pass pointer to the variable
```

## Complete Example

```c
#include "value.h"
#include "nanbox_gc.h"

Value process_words(Value input) {
    GC_PUSH_SCOPE();
    GC_LOCALS(words, result, word, processed);
    
    // Split input into words
    Value space = make_string(" ");
    words = string_split(input, space);
    result = make_list(list_count(words));
    
    // Process each word
    for (int i = 0; i < list_count(words); i++) {
        word = list_get(words, i);
        processed = string_concat(word, make_string("!"));
        list_set(result, i, processed);
    }
    
    GC_POP_SCOPE();
    return result;
}

int main() {
    gc_init();
    
    GC_PUSH_SCOPE();
    GC_LOCALS(input, output);
    
    input = make_string("hello world test");
    output = process_words(input);
    
    printf("Result has %d items\n", list_count(output));
    
    GC_POP_SCOPE();
    gc_shutdown();
    return 0;
}
```

## Important Rules

### ✅ DO:
- Always call `gc_init()` before using any `Value` objects
- Use `GC_PUSH_SCOPE()` at the start of every function
- Protect all local `Value` variables with `GC_LOCALS()` or `GC_PROTECT()`
- Call `GC_POP_SCOPE()` before every return
- Clean up with `gc_shutdown()` at program end

### ❌ DON'T:
- Forget to call `gc_init()` (will cause assertion failures)
- Skip `GC_PUSH_SCOPE()` in functions that use `Value` objects
- Forget to protect local `Value` variables
- Skip `GC_POP_SCOPE()` before returning
- Use `Value` objects after their scope has been popped

## Advanced Usage

### Manual Protection (for more than 16 variables)
```c
void large_function() {
    GC_PUSH_SCOPE();
    
    Value var17 = make_null();
    Value var18 = make_null(); 
    GC_PROTECT(&var17);
    GC_PROTECT(&var18);
    
    // ... use variables
    
    GC_POP_SCOPE();
}
```

### Disabling GC Temporarily
For performance-critical sections where you know GC is not needed:
```c
gc_disable();
// ... critical section (no allocations should trigger GC)
gc_enable();
```

### Manual Collection
Force garbage collection explicitly:
```c
gc_collect();  // Only runs if GC is not disabled
```

## Memory Management Details

- **Allocation**: Objects are allocated from a managed heap
- **Collection**: Triggered automatically when memory threshold is exceeded
- **Mark Phase**: All objects reachable from protected variables are marked
- **Sweep Phase**: Unmarked objects are freed
- **Threshold**: Dynamically adjusted based on collection effectiveness

## Common Mistakes

### 1. Forgetting gc_init()
```c
// WRONG - will crash with assertion
int main() {
    GC_PUSH_SCOPE();  // Assertion failure!
    // ...
}

// CORRECT
int main() {
    gc_init();        // Initialize first
    GC_PUSH_SCOPE();
    // ...
}
```

### 2. Not protecting local variables
```c
// WRONG - variables may be garbage collected
void bad_function() {
    GC_PUSH_SCOPE();
    Value str = make_string("test");  // Not protected!
    // str might be freed during allocation
    GC_POP_SCOPE();
}

// CORRECT
void good_function() {
    GC_PUSH_SCOPE();
    GC_LOCALS(str);
    str = make_string("test");  // Protected
    GC_POP_SCOPE();
}
```

### 3. Missing scope management
```c
// WRONG - no scope management
Value risky_function() {
    Value result = make_string("hello");
    return result;  // result might be garbage collected!
}

// CORRECT
Value safe_function() {
    GC_PUSH_SCOPE();
    GC_LOCALS(result);
    result = make_string("hello");
    GC_POP_SCOPE();
    return result;  // Safe to return
}
```

## Debugging Tips

### Enable GC Debug Mode
Add `-DGC_DEBUG` to your compile flags to see GC activity, and also cause freed blocks to be overwritten with 0xDEADBEEF:
```
gcc -DGC_DEBUG -o myprogram myprogram.c gc.c unicodeUtil.c nanbox_strings.c
```

### Enable Aggressive Collection
Add `-DGC_AGGRESSIVE` to force collection on every allocation (for testing):
```
gcc -DGC_AGGRESSIVE -o myprogram myprogram.c gc.c unicodeUtil.c nanbox_strings.c
```

This helps catch GC-related bugs by making them occur more frequently and predictably.