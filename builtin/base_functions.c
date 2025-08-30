/*
 * SB - Language
 * By Laman28
 * Module - builtin
 * Not welcome to use /XD
 */

#include "base_functions.h"
#include "../error/error.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static char* c2s(char c) {
    char* s = calloc(2, sizeof(char));
    *s = c;
    return s;
}

/* Type transferation */
char* double_to_string(double value) {
    // Temporary buffer
    char temp[350];
    int len = sprintf(temp, "%g", value);

    // Real length of buffer
    char* result = (char*)malloc((len + 1) * sizeof(char));
    if (result == nullptr) {
        return nullptr;
    }

    strcpy(result, temp);
    return result;
}

/* Built-in print function */
static _sbValue builtin_print(_sbVM* vm, _sbValue* args, int arg_count) {
    for (int i = 0; i < arg_count; i++) {
        switch (args[i].type) {
            case VAL_NULL:
                printf("null");
                break;
            case VAL_NUMBER:
                /* Check if it's an integer */
                double num = args[i].as.number;
                //printf("--- PRINT DEBUG: %d, %lf, %lld\n", num == (long long int)num, num, (long long int)num);
                if (num == (long long int)num) {
                    printf("%lld", (long long int)num);
                } else {
                    printf("%.6g", num);
                }
                break;
            case VAL_STRING:
                printf("%s", args[i].as.string);
                break;
            case VAL_BOOL:
                printf("%s", args[i].as.boolean ? "true" : "false");
                break;
            default:
                printf("<object>");
                break;
        }

        if (i < arg_count - 1) {
            printf(" ");
        }
    }
    //printf("\n");

    return create_null();
}

/* Built-in input function */
static _sbValue builtin_input(_sbVM* vm, _sbValue* args, int arg_count) {
    /* Print prompt if provided */
    if (arg_count > 0 && args[0].type == VAL_STRING) {
        printf("%s", args[0].as.string);
        fflush(stdout);
    }

    char buffer[1024];
    if (fgets(buffer, sizeof(buffer), stdin)) {
        /* Remove newline */
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len - 1] == '\n') {
            buffer[len - 1] = '\0';
        }
        return create_string(vm, buffer);
    }

    return create_null();
}

/* Built-in len function */
static _sbValue builtin_len(_sbVM* vm, _sbValue* args, int arg_count) {
    if (arg_count != 1) {
        vm_error(vm, VM_ARGUMENT_MISMATCH, "len() expects exactly 1 argument");
        return create_null();
    }

    switch (args[0].type) {
        case VAL_STRING:
            if (args[0].as.string) {
                return create_number((double)strlen(args[0].as.string));
            }
            return create_number(0);

        case VAL_LIST:
            if (args[0].as.list) {
                return create_number((double)args[0].as.list->count);
            }
            return create_number(0);

        default:
            vm_error(vm, VM_TYPE_ERROR, "len() expects a string or list");
            return create_null();
    }
}

/* Built-in ord function */
static _sbValue builtin_ord(_sbVM* vm, _sbValue* args, int arg_count) {
    if (arg_count != 1) {
        vm_error(vm, VM_ARGUMENT_MISMATCH, "ord() expects exactly 1 argument");
        return create_null();
    }

    if (args[0].type == VAL_STRING) {
        if (strlen(args[0].as.string) != 1) {
            vm_error(vm, VM_TYPE_ERROR, "ord() expects single character, but string of length %d found", strlen(args[0].as.string));
            return create_null();
        }
        int result = (int)args[0].as.string[0];
        return create_number(result);
    }
    else {
        vm_error(vm, VM_TYPE_ERROR, "ord() expects a string");
        return create_null();
    }
}

/* Built-in chr function */
static _sbValue builtin_chr(_sbVM* vm, _sbValue* args, int arg_count) {
    if (arg_count != 1) {
        vm_error(vm, VM_ARGUMENT_MISMATCH, "chr() expects exactly 1 argument");
        return create_null();
    }

    if (args[0].type == VAL_NUMBER) {
        char result_str[2];
        result_str[0] = (char)args[0].as.number;
        result_str[1] = '\0';
        return create_string(vm, result_str);
    }
    else {
        vm_error(vm, VM_TYPE_ERROR, "chr() expects a number");
        return create_null();
    }
}

/* Built-in type function */
static _sbValue builtin_type(_sbVM* vm, _sbValue* args, int arg_count) {
    if (arg_count != 1) {
        vm_error(vm, VM_ARGUMENT_MISMATCH, "type() expects exactly 1 argument");
        return create_null();
    }

    switch (args[0].type) {
        case VAL_NULL:
            return create_string(vm, "null");
        case VAL_NUMBER:
            return create_string(vm, "number");
        case VAL_STRING:
            return create_string(vm,"string");
        case VAL_BOOL:
            return create_string(vm,"bool");
        case VAL_FUNCTION:
            return create_string(vm,"function");
        case VAL_NATIVE:
            return create_string(vm,"native");
        case VAL_STRUCT:
            return create_string(vm,"struct");
        case VAL_STRUCT_INSTANCE:
            return create_string(vm,"instance");
        case VAL_LIST:
            return create_string(vm,"list");
        default:
            return create_string(vm,"unknown");
    }
}

/* Built-in address function */
static _sbValue builtin_address(_sbVM* vm, _sbValue* args, int arg_count) {
    if (arg_count != 1) {
        vm_error(vm, VM_ARGUMENT_MISMATCH, "address() expects exactly 1 argument");
        return create_null();
    }

    return create_number((uintptr_t)&args[0].as);
}

/* Built-in exit function */
static _sbValue builtin_exit(_sbVM* vm, _sbValue* args, int arg_count) {
    int exit_code = 0;

    if (arg_count > 0 && args[0].type == VAL_NUMBER) {
        exit_code = (int)args[0].as.number;
    }

    /* Stop VM execution */
    vm->running = false;

    /* For now, we'll just return null and handle exit in the main loop */
    return create_null();
}

/* Built-in toString function */
static _sbValue builtin_toString(_sbVM* vm, _sbValue* args, int arg_count) {
    if (arg_count != 1) {
        vm_error(vm, VM_ARGUMENT_MISMATCH, "toString() expects exactly 1 argument");
        return create_null();
    }

    switch (args[0].type) {
        case VAL_NULL:
            return create_string(vm, "null");
        case VAL_NUMBER:
            char* _s = double_to_string(args[0].as.number);
            _sbValue v = create_string(vm,_s);
            free(_s);
            return v;
        case VAL_STRING:
            return create_string(vm,args[0].as.string);
        case VAL_BOOL:
            return create_string(vm,args[0].as.boolean ? "true" : "false");
        case VAL_FUNCTION:
            char* _s1 = calloc(12 + strlen(args[0].as.function->name) + strlen(args[0].as.function->source_code_file), sizeof(char));
            strcpy(_s1, "<function:");
            strcat(_s1, args[0].as.function->name);
            strcat(_s1, ":");
            strcat(_s1, args[0].as.function->source_code_file);
            strcat(_s1, ">");
            _sbValue v1 = create_string(vm,_s1);
            free(_s1);
            return v1;
        case VAL_NATIVE:
            return create_string(vm,"<native_function>");
        case VAL_STRUCT:
            ssize_t size = 12;
            size += strlen(args[0].as.struct_def->name);
            for (int i = 0; i < args[0].as.struct_def->member_count; i++) {
                size += strlen(args[0].as.struct_def->members[i]);
                if (i != args[0].as.struct_def->member_count - 1)
                    size += 2;
            }
            size++;
            char* _s3 = calloc(size, sizeof (char));
            strcpy(_s3, "{Struct[");
            strcat(_s3, args[0].as.struct_def->name);
            strcat(_s3, "]: ");
            for (int i = 0; i < args[0].as.struct_def->member_count; i++) {
                strcat(_s3, args[0].as.struct_def->members[i]);
                if (i != args[0].as.struct_def->member_count - 1)
                    strcat(_s3, ", ");
            }
            strcat(_s3, "}");
            _sbValue v3 = create_string(vm,_s3);
            free(_s3);
            return v3;
        case VAL_STRUCT_INSTANCE:
            ssize_t size1 = 13;
            size1 += strlen(args[0].as.instance->struct_def->name);
            char* _s4 = calloc(size1, sizeof (char));
            strcpy(_s4, "{Instance[");
            strcat(_s4, args[0].as.instance->struct_def->name);
            strcat(_s4, "]}");
            _sbValue v4 = create_string(vm,_s4);
            free(_s4);
            return v4;
        case VAL_LIST:
            return create_string(vm,"<list object>");
        default:
            return create_string(vm,"<?undefined type>");
    }
}

static void register_builtin_variables(_sbVM* vm) {
    //printf("DEBUG: Starting register_builtin_variables\n");
    
    // Use create_string to properly allocate the string
    //printf("DEBUG: About to create EOL\n");
    _sbValue eol_val = create_string(vm,"\n");
    //printf("DEBUG: Created EOL, about to define global\n");
    vm_define_global(vm, "EOL", eol_val);
    //printf("DEBUG: EOL global defined\n");
    
    vm_define_global(vm, "true", create_bool(true));
    vm_define_global(vm, "false", create_bool(false));
    
    //printf("DEBUG: Finished register_builtin_variables\n");
}

/* Register built-in functions */
void register_builtin_functions(_sbVM* vm) {
    vm_register_native(vm, "print", builtin_print);
    vm_register_native(vm, "input", builtin_input);
    vm_register_native(vm, "len", builtin_len);

    vm_register_native(vm, "ord", builtin_ord);
    vm_register_native(vm, "chr", builtin_chr);

    vm_register_native(vm, "type", builtin_type);
    vm_register_native(vm, "address", builtin_address);
    vm_register_native(vm, "toString", builtin_toString);
    vm_register_native(vm, "exit", builtin_exit);

    register_builtin_variables(vm);
}

