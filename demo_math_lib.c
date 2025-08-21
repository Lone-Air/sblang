/*
 * SB - Language
 * Demo Math Library for VM
 * This demonstrates how to create a shared library for the SB VM
 */

#include "vm/vm.h"
#include <math.h>
#include <stdio.h>

/* Native sqrt function */
static Value native_sqrt(VM* vm, Value* args, int arg_count) {
    if (arg_count != 1) {
        vm_error(vm, VM_ARGUMENT_MISMATCH, "sqrt expects exactly 1 argument");
        return create_null();
    }
    
    if (args[0].type != VAL_NUMBER) {
        vm_error(vm, VM_TYPE_ERROR, "sqrt expects a number");
        return create_null();
    }
    
    double num = args[0].as.number;
    if (num < 0) {
        vm_error(vm, VM_RUNTIME_ERROR, "sqrt of negative number");
        return create_null();
    }
    
    return create_number(sqrt(num));
}

/* Native abs function */
static Value native_abs(VM* vm, Value* args, int arg_count) {
    if (arg_count != 1) {
        vm_error(vm, VM_ARGUMENT_MISMATCH, "abs expects exactly 1 argument");
        return create_null();
    }
    
    if (args[0].type != VAL_NUMBER) {
        vm_error(vm, VM_TYPE_ERROR, "abs expects a number");
        return create_null();
    }
    
    return create_number(fabs(args[0].as.number));
}

/* Native sin function */
static Value native_sin(VM* vm, Value* args, int arg_count) {
    if (arg_count != 1) {
        vm_error(vm, VM_ARGUMENT_MISMATCH, "sin expects exactly 1 argument");
        return create_null();
    }
    
    if (args[0].type != VAL_NUMBER) {
        vm_error(vm, VM_TYPE_ERROR, "sin expects a number");
        return create_null();
    }
    
    return create_number(sin(args[0].as.number));
}

/* Native cos function */
static Value native_cos(VM* vm, Value* args, int arg_count) {
    if (arg_count != 1) {
        vm_error(vm, VM_ARGUMENT_MISMATCH, "cos expects exactly 1 argument");
        return create_null();
    }
    
    if (args[0].type != VAL_NUMBER) {
        vm_error(vm, VM_TYPE_ERROR, "cos expects a number");
        return create_null();
    }
    
    return create_number(cos(args[0].as.number));
}

/* Native max function */
static Value native_max(VM* vm, Value* args, int arg_count) {
    if (arg_count < 1) {
        vm_error(vm, VM_ARGUMENT_MISMATCH, "max expects at least 1 argument");
        return create_null();
    }
    
    double max_val = 0;
    bool first = true;
    
    for (int i = 0; i < arg_count; i++) {
        if (args[i].type != VAL_NUMBER) {
            vm_error(vm, VM_TYPE_ERROR, "max expects only numbers");
            return create_null();
        }
        
        if (first || args[i].as.number > max_val) {
            max_val = args[i].as.number;
            first = false;
        }
    }
    
    return create_number(max_val);
}

/* Native min function */
static Value native_min(VM* vm, Value* args, int arg_count) {
    if (arg_count < 1) {
        vm_error(vm, VM_ARGUMENT_MISMATCH, "min expects at least 1 argument");
        return create_null();
    }
    
    double min_val = 0;
    bool first = true;
    
    for (int i = 0; i < arg_count; i++) {
        if (args[i].type != VAL_NUMBER) {
            vm_error(vm, VM_TYPE_ERROR, "min expects only numbers");
            return create_null();
        }
        
        if (first || args[i].as.number < min_val) {
            min_val = args[i].as.number;
            first = false;
        }
    }
    
    return create_number(min_val);
}

/* Library initialization function */
int _sbLibInit(VM* vm) {
    if (!vm) {
        return 1; /* Error: invalid VM */
    }
    
    printf("[Math Library] Initializing math library...\n");
    
    /* Register all math functions */
    vm_register_native(vm, "sqrt", native_sqrt);
    vm_register_native(vm, "abs", native_abs);
    vm_register_native(vm, "sin", native_sin);
    vm_register_native(vm, "cos", native_cos);
    vm_register_native(vm, "max", native_max);
    vm_register_native(vm, "min", native_min);
    
    /* Set some math constants */
    vm_define_global(vm, "PI", create_number(3.141592653589793));
    vm_define_global(vm, "E", create_number(2.718281828459045));
    
    printf("[Math Library] Math library initialized successfully!\n");
    printf("[Math Library] Available functions: sqrt, abs, sin, cos, max, min\n");
    printf("[Math Library] Available constants: PI, E\n");
    
    return 0; /* Success */
}