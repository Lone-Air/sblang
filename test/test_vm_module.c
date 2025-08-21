/*
 * SB - Language
 * By Laman28
 * VM Module Loading Test
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

int main(int argc, char** argv) {
    const char* test_code = 
        "print(\"Main program starting...\");\n"
        "load test_module;\n"
        "print(\"Back in main program\");\n"
        "print(\"Module variable value:\", module_var);\n"
        "main_var = module_var * 2;\n"
        "print(\"Main variable:\", main_var);\n";

    printf("=== SB Language VM Module Loading Test ===\n\n");
    printf("Main Program:\n");
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

    printf("Bytecode generated successfully!\n\n");

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
    printf("Registered: print\n\n");

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
    printf("Global Variables:\n");
    for (size_t i = 0; i < vm->globals.count; i++) {
        if (strcmp(vm->globals.vars[i].name, "print") != 0) {
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
    }

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