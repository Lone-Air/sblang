/*
 * SB - Language
 * Demo Math Library for VM
 * This demonstrates how to create a shared library for the SB VM
 */

#include "../../vm/vm.h"
#include "../../error/error.h"
#include <math.h>

/* Native sqrt function */
static _sbValue native_sqrt(_sbVM* vm, _sbValue* args, int arg_count) {
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
static _sbValue native_abs(_sbVM* vm, _sbValue* args, int arg_count) {
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

/* Native mod function */
static _sbValue native_mod(_sbVM* vm, _sbValue* args, int arg_count) {
    if (arg_count != 1) {
        vm_error(vm, VM_ARGUMENT_MISMATCH, "mod expects exactly 1 argument");
        return create_null();
    }

    if (args[0].type != VAL_NUMBER || args[1].type != VAL_NUMBER) {
        vm_error(vm, VM_TYPE_ERROR, "mod expects a number");
        return create_null();
    }

    return create_number(fmod(args[0].as.number, args[1].as.number));
}

/* Native ceil function */
static _sbValue native_ceil(_sbVM* vm, _sbValue* args, int arg_count) {
    if (arg_count != 1) {
        vm_error(vm, VM_ARGUMENT_MISMATCH, "ceil expects exactly 1 argument");
        return create_null();
    }

    if (args[0].type != VAL_NUMBER) {
        vm_error(vm, VM_TYPE_ERROR, "ceil expects a number");
        return create_null();
    }

    return create_number(ceil(args[0].as.number));
}

/* Native fract function */
static _sbValue native_fract(_sbVM* vm, _sbValue* args, int arg_count) {
    if (arg_count != 1) {
        vm_error(vm, VM_ARGUMENT_MISMATCH, "fract expects exactly 1 argument");
        return create_null();
    }

    if (args[0].type != VAL_NUMBER) {
        vm_error(vm, VM_TYPE_ERROR, "fract expects a number");
        return create_null();
    }

    double result, intpart;

    result = modf(args[0].as.number, &intpart);

    return create_number(result);
}

/* Native int function */
static _sbValue native_int(_sbVM* vm, _sbValue* args, int arg_count) {
    if (arg_count != 1) {
        vm_error(vm, VM_ARGUMENT_MISMATCH, "int expects exactly 1 argument");
        return create_null();
    }

    if (args[0].type != VAL_NUMBER) {
        vm_error(vm, VM_TYPE_ERROR, "int expects a number");
        return create_null();
    }

    double intpart;

    modf(args[0].as.number, &intpart);

    return create_number(intpart);
}

/* Native frexpm function */
static _sbValue native_frexpm(_sbVM* vm, _sbValue* args, int arg_count) {
    if (arg_count != 1) {
        vm_error(vm, VM_ARGUMENT_MISMATCH, "frexpm expects exactly 1 argument");
        return create_null();
    }

    if (args[0].type != VAL_NUMBER) {
        vm_error(vm, VM_TYPE_ERROR, "frexpm expects a number");
        return create_null();
    }

    int exponent;

    return create_number(frexp(args[0].as.number, &exponent));
}

/* Native frexpe function */
static _sbValue native_frexpe(_sbVM* vm, _sbValue* args, int arg_count) {
    if (arg_count != 1) {
        vm_error(vm, VM_ARGUMENT_MISMATCH, "frexpe expects exactly 1 argument");
        return create_null();
    }

    if (args[0].type != VAL_NUMBER) {
        vm_error(vm, VM_TYPE_ERROR, "frexpe expects a number");
        return create_null();
    }

    int exponent;

    frexp(args[0].as.number, &exponent);

    return create_number(exponent);
}

/* Native log function */
static _sbValue native_log(_sbVM* vm, _sbValue* args, int arg_count) {
    if (arg_count != 1) {
        vm_error(vm, VM_ARGUMENT_MISMATCH, "log expects exactly 1 argument");
        return create_null();
    }

    if (args[0].type != VAL_NUMBER || args[1].type != VAL_NUMBER) {
        vm_error(vm, VM_TYPE_ERROR, "log expects a number");
        return create_null();
    }

    return create_number(log(args[0].as.number) / log(args[1].as.number)); // 0: value, 1: base
}

/* Native ln function */
static _sbValue native_ln(_sbVM* vm, _sbValue* args, int arg_count) {
    if (arg_count != 1) {
        vm_error(vm, VM_ARGUMENT_MISMATCH, "ln expects exactly 1 argument");
        return create_null();
    }

    if (args[0].type != VAL_NUMBER) {
        vm_error(vm, VM_TYPE_ERROR, "ln expects a number");
        return create_null();
    }

    return create_number(log(args[0].as.number));;
}

/* Native lg function */
static _sbValue native_lg(_sbVM* vm, _sbValue* args, int arg_count) {
    if (arg_count != 1) {
        vm_error(vm, VM_ARGUMENT_MISMATCH, "lg expects exactly 1 argument");
        return create_null();
    }

    if (args[0].type != VAL_NUMBER) {
        vm_error(vm, VM_TYPE_ERROR, "lg expects a number");
        return create_null();
    }

    return create_number(log10(args[0].as.number));
}

/* Native log2 function */
static _sbValue native_log2(_sbVM* vm, _sbValue* args, int arg_count) {
    if (arg_count != 1) {
        vm_error(vm, VM_ARGUMENT_MISMATCH, "log2 expects exactly 1 argument");
        return create_null();
    }

    if (args[0].type != VAL_NUMBER) {
        vm_error(vm, VM_TYPE_ERROR, "log2 expects a number");
        return create_null();
    }

    return create_number(log2(args[0].as.number));
}

/* Native exp function */
static _sbValue native_exp(_sbVM* vm, _sbValue* args, int arg_count) {
    if (arg_count != 1) {
        vm_error(vm, VM_ARGUMENT_MISMATCH, "exp expects exactly 1 argument");
        return create_null();
    }

    if (args[0].type != VAL_NUMBER) {
        vm_error(vm, VM_TYPE_ERROR, "exp expects a number");
        return create_null();
    }

    return create_number(exp(args[0].as.number));
}

/* Native pow function */
static _sbValue native_pow(_sbVM* vm, _sbValue* args, int arg_count) {
    if (arg_count != 2) {
        vm_error(vm, VM_ARGUMENT_MISMATCH, "pow expects exactly 1 argument");
        return create_null();
    }

    if (args[0].type != VAL_NUMBER || args[1].type != VAL_NUMBER) {
        vm_error(vm, VM_TYPE_ERROR, "pow expects two numbers");
        return create_null();
    }

    return create_number(pow(args[0].as.number, args[1].as.number));
}

/* Native ldexp function */
static _sbValue native_ldexp(_sbVM* vm, _sbValue* args, int arg_count) {
    if (arg_count != 2) {
        vm_error(vm, VM_ARGUMENT_MISMATCH, "ldexp expects exactly 1 argument");
        return create_null();
    }

    if (args[0].type != VAL_NUMBER || args[1].type != VAL_NUMBER) {
        vm_error(vm, VM_TYPE_ERROR, "ldexp expects two numbers");
        return create_null();
    }

    return create_number(ldexp(args[0].as.number, args[1].as.number));
}

/* Native sin function */
static _sbValue native_sin(_sbVM* vm, _sbValue* args, int arg_count) {
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
static _sbValue native_cos(_sbVM* vm, _sbValue* args, int arg_count) {
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

/* Native tan function */
static _sbValue native_tan(_sbVM* vm, _sbValue* args, int arg_count) {
    if (arg_count != 1) {
        vm_error(vm, VM_ARGUMENT_MISMATCH, "tan expects exactly 1 argument");
        return create_null();
    }

    if (args[0].type != VAL_NUMBER) {
        vm_error(vm, VM_TYPE_ERROR, "tan expects a number");
        return create_null();
    }

    return create_number(tan(args[0].as.number));
}

/* Native asin function */
static _sbValue native_asin(_sbVM* vm, _sbValue* args, int arg_count) {
    if (arg_count != 1) {
        vm_error(vm, VM_ARGUMENT_MISMATCH, "asin expects exactly 1 argument");
        return create_null();
    }

    if (args[0].type != VAL_NUMBER) {
        vm_error(vm, VM_TYPE_ERROR, "asin expects a number");
        return create_null();
    }

    return create_number(asin(args[0].as.number));
}

/* Native acos function */
static _sbValue native_acos(_sbVM* vm, _sbValue* args, int arg_count) {
    if (arg_count != 1) {
        vm_error(vm, VM_ARGUMENT_MISMATCH, "acos expects exactly 1 argument");
        return create_null();
    }

    if (args[0].type != VAL_NUMBER) {
        vm_error(vm, VM_TYPE_ERROR, "acos expects a number");
        return create_null();
    }

    return create_number(acos(args[0].as.number));
}

/* Native atan function */
static _sbValue native_atan(_sbVM* vm, _sbValue* args, int arg_count) {
    if (arg_count != 1) {
        vm_error(vm, VM_ARGUMENT_MISMATCH, "atan expects exactly 1 argument");
        return create_null();
    }

    if (args[0].type != VAL_NUMBER) {
        vm_error(vm, VM_TYPE_ERROR, "atan expects a number");
        return create_null();
    }

    return create_number(atan(args[0].as.number));
}

/* Native sinh function */
static _sbValue native_sinh(_sbVM* vm, _sbValue* args, int arg_count) {
    if (arg_count != 1) {
        vm_error(vm, VM_ARGUMENT_MISMATCH, "sinh expects exactly 1 argument");
        return create_null();
    }

    if (args[0].type != VAL_NUMBER) {
        vm_error(vm, VM_TYPE_ERROR, "sinh expects a number");
        return create_null();
    }

    return create_number(sinh(args[0].as.number));
}

/* Native cosh function */
static _sbValue native_cosh(_sbVM* vm, _sbValue* args, int arg_count) {
    if (arg_count != 1) {
        vm_error(vm, VM_ARGUMENT_MISMATCH, "cosh expects exactly 1 argument");
        return create_null();
    }

    if (args[0].type != VAL_NUMBER) {
        vm_error(vm, VM_TYPE_ERROR, "cosh expects a number");
        return create_null();
    }

    return create_number(cosh(args[0].as.number));
}

/* Native tanh function */
static _sbValue native_tanh(_sbVM* vm, _sbValue* args, int arg_count) {
    if (arg_count != 1) {
        vm_error(vm, VM_ARGUMENT_MISMATCH, "tanh expects exactly 1 argument");
        return create_null();
    }

    if (args[0].type != VAL_NUMBER) {
        vm_error(vm, VM_TYPE_ERROR, "tanh expects a number");
        return create_null();
    }

    return create_number(tanh(args[0].as.number));
}

/* Native max function */
static _sbValue native_max(_sbVM* vm, _sbValue* args, int arg_count) {
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
static _sbValue native_min(_sbVM* vm, _sbValue* args, int arg_count) {
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
int _sbLibInit(_sbVM* vm) {
    if (!vm) {
        return 1; /* Error: invalid VM */
    }

    /* Register all math functions */
    vm_register_native(vm, "sqrt", native_sqrt);
    vm_register_native(vm, "abs", native_abs);
    vm_register_native(vm, "mod", native_mod);
    vm_register_native(vm, "ceil", native_ceil);
    vm_register_native(vm, "fract", native_fract);
    vm_register_native(vm, "int", native_int);
    vm_register_native(vm, "frexpm", native_frexpm);
    vm_register_native(vm, "frexpe", native_frexpe);
    vm_register_native(vm, "log", native_log);
    vm_register_native(vm, "ln", native_ln);
    vm_register_native(vm, "lg", native_lg);
    vm_register_native(vm, "log2", native_log2);
    vm_register_native(vm, "exp", native_exp);
    vm_register_native(vm, "pow", native_pow);
    vm_register_native(vm, "ldexp", native_ldexp);
    vm_register_native(vm, "sin", native_sin);
    vm_register_native(vm, "cos", native_cos);
    vm_register_native(vm, "tan", native_tan);
    vm_register_native(vm, "asin", native_asin);
    vm_register_native(vm, "acos", native_acos);
    vm_register_native(vm, "atan", native_atan);
    vm_register_native(vm, "sinh", native_sinh);
    vm_register_native(vm, "cosh", native_cosh);
    vm_register_native(vm, "tanh", native_tanh);
    vm_register_native(vm, "max", native_max);
    vm_register_native(vm, "min", native_min);
    
    /* Set some math constants */
    vm_define_global(vm, "PI", create_number(3.141592653589793));
    vm_define_global(vm, "E", create_number(2.718281828459045));
    
    return 0; /* Success */
}