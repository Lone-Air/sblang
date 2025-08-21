/*
 * SB - Language
 * By Laman28
 * Compiler - Bytecode Test
 * Not welcome to use /XD
 */

#include "../bytecode/bytecode.h"
#include "../parser/parser.h"
#include "../lexer/lexer.h"
#include "../error/error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char* read_file(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* content = (char*)malloc(size + 1);
    if (!content) {
        fclose(file);
        return NULL;
    }

    fread(content, 1, size, file);
    content[size] = '\0';
    fclose(file);

    return content;
}

int main(int argc, char **argv) {
    const char* test_code;
    if (argc == 1) {
        test_code = "struct Structure1{\n"
                               "    member1, member2\n"
                               "}\n"
                               "variable2 -> Structure1;\n"
                               "variable2.member1 = 2;\n"
                               "variable2.member2 = 3;\n"
                               "print(variable2.member1);\n"
                               "print(variable2.member2);";
    }
    else {
         test_code = read_file(argv[1]);
    }

    printf("=== SB Language Bytecode Generator Test ===\n\n");
    printf("Input code:\n%s\n\n", test_code);

    printf("=== Lexing ===\n");
    _sbToken* tokens = _sbLexer(test_code);
    if (!tokens) {
        fprintf(stderr, "Failed to tokenize input\n");
        return 1;
    }

    printf("=== Parsing ===\n");
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

    printf("Parsing successful!\n\n");

    printf("=== Generating Bytecode ===\n");
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

    printf("Bytecode generation successful!\n");

    print_bytecode(gen);

    printf("\n=== Saving Bytecode ===\n");
    if (save_bytecode(gen, "output.sbc")) {
        printf("Bytecode saved to output.sbc\n");
    } else {
        fprintf(stderr, "Failed to save bytecode\n");
    }

    printf("\n=== Loading Bytecode ===\n");
    BytecodeGenerator* loaded_gen = load_bytecode("output.sbc");
    if (loaded_gen) {
        printf("Bytecode loaded successfully from output.sbc\n");
        printf("Loaded %zu instructions\n", loaded_gen->instructions->count);
        destroy_bytecode_generator(loaded_gen);
    } else {
        fprintf(stderr, "Failed to load bytecode\n");
    }

    destroy_bytecode_generator(gen);
    free_ast(ast);
    destroy_tkstate(parser);
    freeTkList(tokens);

    printf("\n=== Test completed successfully! ===\n");
    return 0;
}