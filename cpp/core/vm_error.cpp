// vm_error.c
//
// C-callable bridge for raising VM runtime errors from core C code.

#include "vm_error.h"
#include <stdio.h>

static vm_error_callback_t s_error_callback = NULL;

void vm_error_set_callback(vm_error_callback_t callback) {
    s_error_callback = callback;
}

void vm_raise_runtime_error(const char* message) {
    if (s_error_callback) {
        s_error_callback(message);
    } else {
        fprintf(stderr, "Runtime Error (no VM): %s\n", message);
    }
}
