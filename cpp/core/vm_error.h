// vm_error.h
//
// C-callable bridge for raising VM runtime errors from core C code
// (e.g., value_list.c, value_map.c).

#ifndef VM_ERROR_H
#define VM_ERROR_H

#ifdef __cplusplus
extern "C" {
#endif

// Function pointer type for the runtime error callback
typedef void (*vm_error_callback_t)(const char* message);

// Set the callback (called by the C++ VM during initialization)
void vm_error_set_callback(vm_error_callback_t callback);

// Raise a runtime error via the callback (called by C code)
void vm_raise_runtime_error(const char* message);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // VM_ERROR_H
