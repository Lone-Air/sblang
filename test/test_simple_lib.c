/*
 * SB - Language
 * Simple test for shared library loading
 */

#include "../vm/vm.h"
#include "../bytecode/bytecode.h"
#include "../parser/parser.h"
#include "../lexer/lexer.h"
#include "../error/error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Native print function */
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
    /* Simple test code */
    const char* test_code = 
        "print(\"Loading math library...\");\n"
        "load math;\n"
        "print(\"Math library loaded!\");\n"
        "print(\"PI value:\");\n"
        "print(PI);\n"
        "print(\"Testing sqrt:\");\n"
        "x = 16;\n"
        "y = sqrt(x);\n"
        "print(y);\n"
        "print(\"Testing abs:\");\n"
        "z = abs(-5);\n"
        "print(z);\n"
        "print(\"Test complete!\");\n";

    printf("=== SB Language Shared Library Loading Demo ===\n\n");
    printf("Test Program:\n");
    printf("----------------------------------------\n");
    printf("%s", test_code);
    printf("----------------------------------------\n\n");

    /* Tokenize the source code */
    printf("=== Compiling to Bytecode ===\n");
    _sbToken* tokens = _sbLexer(test_code);
    if (!tokens) {
        fprintf(stderr, "Failed to tokenize input\n");
        return 1;
    }

    /* Parse the tokens */
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

    /* Generate bytecode */
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

    /* Create and initialize VM */
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

    /* Register native print function */
    printf("=== Registering Native Functions ===\n");
    vm_register_native(vm, "print", native_print);
    printf("Registered: print\n\n");

    /* Load bytecode into VM */
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

    /* Execute the program */
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

    /* Cleanup */
    printf("\n=== Cleanup ===\n");
    destroy_vm(vm);
    destroy_bytecode_generator(gen);
    free_ast(ast);
    destroy_tkstate(parser);
    freeTkList(tokens);

    printf("All resources freed successfully!\n");
    printf("\n=== Demo completed successfully! ===\n");
    
    return 0;
}