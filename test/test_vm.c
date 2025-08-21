/*
 * SB - Language
 * By Laman28
 * VM Test Demo
 * Not welcome to use /XD
 */

#include "../vm/vm.h"
#include "../bytecode/bytecode.h"
#include "../parser/parser.h"
#include "../lexer/lexer.h"
#include "../error/error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Value native_print(VM* vm, Value* args, int arg_count) {
    for (int i = 0; i < arg_count; i++) {
        switch (args[i].type) {
            case VAL_NULL:
                printf("null");
                break;
            case VAL_NUMBER:
                printf("%.6f", args[i].as.number);
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
    printf("\n");
    
    return create_null();
}

Value native_input(VM* vm, Value* args, int arg_count) {
    if (arg_count > 0 && args[0].type == VAL_STRING) {
        printf("%s", args[0].as.string);
    }
    
    char buffer[256];
    if (fgets(buffer, sizeof(buffer), stdin)) {
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len - 1] == '\n') {
            buffer[len - 1] = '\0';
        }
        return create_string(buffer);
    }
    
    return create_null();
}

Value native_tonumber(VM* vm, Value* args, int arg_count) {
    if (arg_count == 0) {
        return create_number(0);
    }
    
    switch (args[0].type) {
        case VAL_NUMBER:
            return args[0];
        case VAL_STRING: {
            char* endptr;
            double num = strtod(args[0].as.string, &endptr);
            if (*endptr == '\0') {
                return create_number(num);
            }
            return create_null();
        }
        case VAL_BOOL:
            return create_number(args[0].as.boolean ? 1.0 : 0.0);
        default:
            return create_null();
    }
}

Value native_tostring(VM* vm, Value* args, int arg_count) {
    if (arg_count == 0) {
        return create_string("");
    }
    
    char buffer[256];
    switch (args[0].type) {
        case VAL_NULL:
            return create_string("null");
        case VAL_NUMBER:
            snprintf(buffer, sizeof(buffer), "%.6f", args[0].as.number);
            return create_string(buffer);
        case VAL_STRING:
            return args[0];
        case VAL_BOOL:
            return create_string(args[0].as.boolean ? "true" : "false");
        default:
            return create_string("<object>");
    }
}

int main(int argc, char** argv) {
    const char* test_code = 
        "x = 10;\n"
        "y = 20;\n"
        "z = x + y;\n"
        "print(\"x =\", x);\n"
        "print(\"y =\", y);\n"
        "print(\"z = x + y =\", z);\n"
        "\n"
        "i = 0;\n"
        "while(i < 5) {\n"
        "    print(\"Loop iteration:\", i);\n"
        "    i = i + 1;\n"
        "}\n"
        "\n"
        "if(z > 25) {\n"
        "    print(\"z is greater than 25\");\n"
        "} else {\n"
        "    print(\"z is not greater than 25\");\n"
        "}\n"
        "\n"
        "result = (12 ** 2) + (3 * 4) - 10;\n"
        "print(\"Complex expression result:\", result);\n"
        "\n"
        "bit_result = (15 & 7) | (8 ^ 3);\n"
        "print(\"Bitwise operations:\", bit_result);\n";

    printf("=== SB Language Virtual Machine Test ===\n\n");
    printf("Test Program:\n");
    printf("----------------------------------------\n");
    printf("%s", test_code);
    printf("----------------------------------------\n\n");

    printf("=== Compiling to Bytecode ===\n");
    
    _sbToken* tokens = _sbLexer(test_code);
    if (!tokens) {
        fprintf(stderr, "Failed to tokenize input\n");
        return 1;
    }

    Parser* parser = create_tkstate(tokens);
    if (!parser) {
        fprintf(stderr, "Failed to create parser state\n");
        freeTkList(tokens);
        return 1;
    }

    reset_error();
    ASTNode* ast = parse_program(parser);
    
    if (!ast || syntaxErrorDetector) {
        fprintf(stderr, "Failed to parse input\n");
        destroy_tkstate(parser);
        freeTkList(tokens);
        return 1;
    }

    BytecodeGenerator* gen = create_bytecode_generator();
    if (!gen) {
        fprintf(stderr, "Failed to create bytecode generator\n");
        free_ast(ast);
        destroy_tkstate(parser);
        freeTkList(tokens);
        return 1;
    }

    if (!generate_bytecode(gen, ast)) {
        fprintf(stderr, "Failed to generate bytecode\n");
        destroy_bytecode_generator(gen);
        free_ast(ast);
        destroy_tkstate(parser);
        freeTkList(tokens);
        return 1;
    }

    printf("Bytecode generated successfully!\n");
    printf("Total instructions: %zu\n\n", gen->instructions->count);

    printf("=== Creating Virtual Machine ===\n");
    VM* vm = create_vm();
    if (!vm) {
        fprintf(stderr, "Failed to create VM\n");
        destroy_bytecode_generator(gen);
        free_ast(ast);
        destroy_tkstate(parser);
        freeTkList(tokens);
        return 1;
    }

    printf("=== Registering Native Functions ===\n");
    vm_register_native(vm, "print", native_print);
    vm_register_native(vm, "input", native_input);
    vm_register_native(vm, "tonumber", native_tonumber);
    vm_register_native(vm, "tostring", native_tostring);
    printf("Registered: print, input, tonumber, tostring\n\n");

    printf("=== Loading Bytecode into VM ===\n");
    if (!vm_load_bytecode(vm, gen)) {
        fprintf(stderr, "Failed to load bytecode into VM\n");
        vm_print_error(vm);
        destroy_vm(vm);
        destroy_bytecode_generator(gen);
        free_ast(ast);
        destroy_tkstate(parser);
        freeTkList(tokens);
        return 1;
    }
    printf("Bytecode loaded successfully!\n\n");

    printf("=== Executing Program ===\n");
    printf("Output:\n");
    printf("----------------------------------------\n");
    
    VMError result = vm_execute(vm);
    
    printf("----------------------------------------\n\n");
    
    if (result != VM_OK) {
        printf("=== Execution Error ===\n");
        vm_print_error(vm);
    } else {
        printf("=== Execution Completed Successfully ===\n");
    }

    printf("\n=== Final VM State ===\n");
    printf("Stack: ");
    vm_print_stack(vm);
    
    printf("\nGlobal Variables:\n");
    for (size_t i = 0; i < vm->globals.count; i++) {
        printf("  %s = ", vm->globals.vars[i].name);
        Value val = vm->globals.vars[i].value;
        switch (val.type) {
            case VAL_NULL:
                printf("null\n");
                break;
            case VAL_NUMBER:
                printf("%.6f\n", val.as.number);
                break;
            case VAL_STRING:
                printf("\"%s\"\n", val.as.string);
                break;
            case VAL_BOOL:
                printf("%s\n", val.as.boolean ? "true" : "false");
                break;
            default:
                printf("<object>\n");
                break;
        }
    }

    printf("\n=== Testing External Value Push ===\n");
    printf("Pushing external value 42 to stack...\n");
    vm_push_external(vm, create_number(42));
    printf("Stack after push: ");
    vm_print_stack(vm);

    printf("\n=== Cleanup ===\n");
    destroy_vm(vm);
    destroy_bytecode_generator(gen);
    free_ast(ast);
    destroy_tkstate(parser);
    freeTkList(tokens);

    printf("All resources freed successfully!\n");
    printf("\n=== Test completed! ===\n");
    
    return 0;
}