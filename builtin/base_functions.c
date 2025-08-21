/*
 * SB - Language
 * By Laman28
 * Module - builtin
 * Not welcome to use /XD
 */

#include "base_functions.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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
static Value builtin_print(VM* vm, Value* args, int arg_count) {
    for (int i = 0; i < arg_count; i++) {
        switch (args[i].type) {
            case VAL_NULL:
                printf("null");
                break;
            case VAL_NUMBER:
                /* Check if it's an integer */
                double num = args[i].as.number;
                if (num == (int)num) {
                    printf("%d", (int)num);
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
static Value builtin_input(VM* vm, Value* args, int arg_count) {
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
        return create_string(buffer);
    }

    return create_null();
}

/* Built-in len function */
static Value builtin_len(VM* vm, Value* args, int arg_count) {
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

/* Built-in type function */
static Value builtin_type(VM* vm, Value* args, int arg_count) {
    if (arg_count != 1) {
        vm_error(vm, VM_ARGUMENT_MISMATCH, "type() expects exactly 1 argument");
        return create_null();
    }

    switch (args[0].type) {
        case VAL_NULL:
            return create_string("null");
        case VAL_NUMBER:
            return create_string("number");
        case VAL_STRING:
            return create_string("string");
        case VAL_BOOL:
            return create_string("bool");
        case VAL_FUNCTION:
            return create_string("function");
        case VAL_NATIVE:
            return create_string("native");
        case VAL_STRUCT:
            return create_string("struct");
        case VAL_STRUCT_INSTANCE:
            return create_string("instance");
        case VAL_LIST:
            return create_string("list");
        default:
            return create_string("unknown");
    }
}

/* Built-in exit function */
static Value builtin_exit(VM* vm, Value* args, int arg_count) {
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
static Value builtin_toString(VM* vm, Value* args, int arg_count) {
    if (arg_count != 1) {
        vm_error(vm, VM_ARGUMENT_MISMATCH, "toString() expects exactly 1 argument");
        return create_null();
    }

    switch (args[0].type) {
        case VAL_NULL:
            return create_string("null");
        case VAL_NUMBER:
            char* _s = double_to_string(args[0].as.number);
            Value v = create_string(_s);
            free(_s);
            return v;
        case VAL_STRING:
            return create_string(args[0].as.string);
        case VAL_BOOL:
            return create_string(args[0].as.boolean ? "true" : "false");
        case VAL_FUNCTION:
            char* _s1 = calloc(12 + strlen(args[0].as.function->name), sizeof(char));
            strcpy(_s1, "<function:");
            strcat(_s1, args[0].as.function->name);
            strcat(_s1, ">");
            Value v1 = create_string(_s1);
            free(_s1);
            return v1;
        case VAL_NATIVE:
            return create_string("<native_function>");
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
            Value v3 = create_string(_s3);
            free(_s3);
            return v3;
        case VAL_STRUCT_INSTANCE:
            ssize_t size1 = 13;
            size1 += strlen(args[0].as.instance->struct_def->name);
            char* _s4 = calloc(size1, sizeof (char));
            strcpy(_s4, "{Instance[");
            strcat(_s4, args[0].as.instance->struct_def->name);
            strcat(_s4, "]}");
            Value v4 = create_string(_s4);
            free(_s4);
            return v4;
        case VAL_LIST:
            return create_string("list");
        default:
            return create_string("<?undefined type>");
    }
}

/* Register built-in functions */
void register_builtin_functions(VM* vm) {
    vm_register_native(vm, "print", builtin_print);
    vm_register_native(vm, "input", builtin_input);
    vm_register_native(vm, "len", builtin_len);
    vm_register_native(vm, "type", builtin_type);
    vm_register_native(vm, "toString", builtin_toString);
    vm_register_native(vm, "exit", builtin_exit);
}

