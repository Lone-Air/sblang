/*
 * SB - Language
 * Compiler Frontend and Runtime
 * Main entry point for the SB language
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "vm/vm.h"
#include "bytecode/bytecode.h"
#include "parser/parser.h"
#include "lexer/lexer.h"
#include "error/error.h"
#include "information.h"

/* Command line options structure */
typedef struct {
    bool compile_only;      /* -c flag: compile only, don't execute */
    char* output_file;      /* -o flag: output file name */
    char** input_files;     /* Input file paths */
    int file_count;         /* Number of input files */
} Options;

/* Function prototypes */
static void print_usage(const char* program_name);
static void print_version(void);
static Options parse_arguments(int argc, char** argv);
static bool compile_file(const char* input_file, const char* output_file);
static bool execute_file(const char* input_file);
static bool is_bytecode_file(const char* filename);
static bool file_exists(const char* filename);
static char* get_output_filename(const char* input_file);
static char* read_file(const char* filename);

int main(int argc, char** argv) {
    /* Parse command line arguments */
    Options options = parse_arguments(argc, argv);
    
    if (options.file_count == 0) {
        fprintf(stderr, "Error: No input files specified\n");
        print_usage(argv[0]);
        return 1;
    }
    
    /* Check for multiple files with -o option */
    if (options.compile_only && options.output_file && options.file_count > 1) {
        fprintf(stderr, "Error: Cannot specify output file with multiple input files\n");
        return 1;
    }
    
    bool success = true;
    
    /* Process each input file */
    for (int i = 0; i < options.file_count; i++) {
        const char* input_file = options.input_files[i];
        
        /* Check if file exists */
        if (!file_exists(input_file)) {
            fprintf(stderr, "Error: File '%s' does not exist\n", input_file);
            success = false;
            break;
        }
        
        /* Store current working directory */
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd)) == NULL) {
            fprintf(stderr, "Error: Failed to get current working directory\n");
            success = false;
            break;
        }
        
        if (options.compile_only) {
            /* Compile only mode */
            char* output_file = NULL;
            
            if (options.output_file && options.file_count == 1) {
                output_file = options.output_file;
            } else {
                output_file = get_output_filename(input_file);
            }
            
            printf("--- Compiling '%s' to '%s'...\n", input_file, output_file);
            
            if (!compile_file(input_file, output_file)) {
                fprintf(stderr, "Error: Failed to compile '%s'\n", input_file);
                success = false;
            } else {
                printf("Successfully compiled '%s'\n", input_file);
            }
            
            if (!options.output_file && output_file) {
                free(output_file);
            }
        } else {
            /* Execute mode */
            printf("--- Running '%s'...\n", input_file);
            
            if (!execute_file(input_file)) {
                fprintf(stderr, "Error: Failed to execute '%s'\n", input_file);
                success = false;
            }
        }
        
        /* Restore working directory */
        if (chdir(cwd) != 0) {
            fprintf(stderr, "Warning: Failed to restore working directory\n");
        }
        
        if (!success) {
            break;
        }
    }
    
    /* Clean up */
    if (options.input_files) {
        free(options.input_files);
    }
    if (options.output_file) {
        free(options.output_file);
    }
    
    return success ? 0 : 1;
}

/* Parse command line arguments */
static Options parse_arguments(int argc, char** argv) {
    Options options = {
        .compile_only = false,
        .output_file = NULL,
        .input_files = NULL,
        .file_count = 0
    };
    
    /* Allocate space for input files */
    options.input_files = (char**)malloc(argc * sizeof(char*));
    
    bool files_started = false;
    
    for (int i = 1; i < argc; i++) {
        if (!files_started && argv[i][0] == '-') {
            /* Process flags */
            if (strcmp(argv[i], "-c") == 0) {
                options.compile_only = true;
            } else if (strcmp(argv[i], "-o") == 0) {
                if (i + 1 >= argc) {
                    fprintf(stderr, "Error: -o requires an argument\n");
                    exit(1);
                }
                options.output_file = strdup(argv[++i]);
            } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
                print_version();
                exit(0);
            } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
                print_usage(argv[0]);
                exit(0);
            } else {
                fprintf(stderr, "Error: Unknown option '%s'\n", argv[i]);
                print_usage(argv[0]);
                exit(1);
            }
        } else {
            /* All remaining arguments are files */
            files_started = true;
            options.input_files[options.file_count++] = argv[i];
        }
    }
    
    return options;
}

/* Print usage information */
static void print_usage(const char* program_name) {
    printf("SB Language Compiler & Runtime v%s\n", VERSION);
    printf("Create by Laman28 - Release under LGPL License\n");
    printf("Usage: %s [options] file1 [file2 ...]\n", program_name);
    printf("\nOptions:\n");
    printf("  -c              Compile only (generate .sbc files)\n");
    printf("  -o <file>       Specify output file (only with single input file)\n");
    printf("  -v, --version   Show version information\n");
    printf("  -h, --help      Show this help message\n");
    printf("\nExamples:\n");
    printf("  %s program.sb              Execute a source file\n", program_name);
    printf("  %s program.sbc             Execute a bytecode file\n", program_name);
    printf("  %s -c program.sb           Compile to bytecode\n", program_name);
    printf("  %s -c -o out.sbc prog.sb   Compile with custom output\n", program_name);
}

/* Print version information */
static void print_version(void) {
    printf("SB Language Compiler & Runtime v%s\n", VERSION);
    printf("Create by Laman28 - Release under LGPL License\n");
}

/* Check if file exists */
static bool file_exists(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (file) {
        fclose(file);
        return true;
    }
    return false;
}

/* Check if file is bytecode based on extension */
static bool is_bytecode_file(const char* filename) {
    size_t len = strlen(filename);
    return len > 4 && strcmp(filename + len - 4, ".sbc") == 0;
}

/* Get output filename for bytecode */
static char* get_output_filename(const char* input_file) {
    size_t len = strlen(input_file);
    char* output = (char*)malloc(len + 5);
    
    /* Check if input has .sb extension */
    if (len > 3 && strcmp(input_file + len - 3, ".sb") == 0) {
        strncpy(output, input_file, len - 3);
        output[len - 3] = '\0';
        strcat(output, ".sbc");
    } else {
        strcpy(output, input_file);
        strcat(output, ".sbc");
    }
    
    return output;
}

/* Read entire file into memory */
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

/* Compile source file to bytecode */
static bool compile_file(const char* input_file, const char* output_file) {
    /* Read source file */
    char* source = read_file(input_file);
    if (!source) {
        fprintf(stderr, "Error: Failed to read file '%s'\n", input_file);
        return false;
    }
    
    /* Lexical analysis */
    _sbToken* tokens = _sbLexer(source);
    if (!tokens) {
        fprintf(stderr, "Error: Failed to tokenize '%s'\n", input_file);
        free(source);
        return false;
    }
    
    /* Parse */
    Parser* parser = create_tkstate(tokens);
    if (!parser) {
        fprintf(stderr, "Error: Failed to create parser\n");
        freeTkList(tokens);
        free(source);
        return false;
    }
    
    reset_error();
    ASTNode* ast = parse_program(parser);
    
    if (!ast || syntaxErrorDetector) {
        fprintf(stderr, "Error: Failed to parse '%s'\n", input_file);
        if (syntaxErrorDetector) {
            fprintf(stderr, "Syntax error detected in file\n");
        }
        destroy_tkstate(parser);
        freeTkList(tokens);
        free(source);
        return false;
    }
    
    /* Generate bytecode */
    BytecodeGenerator* gen = create_bytecode_generator();
    if (!gen) {
        fprintf(stderr, "Error: Failed to create bytecode generator\n");
        free_ast(ast);
        destroy_tkstate(parser);
        freeTkList(tokens);
        free(source);
        return false;
    }
    
    if (!generate_bytecode(gen, ast)) {
        fprintf(stderr, "Error: Failed to generate bytecode\n");
        destroy_bytecode_generator(gen);
        free_ast(ast);
        destroy_tkstate(parser);
        freeTkList(tokens);
        free(source);
        return false;
    }
    
    /* Save bytecode to file */
    if (!save_bytecode(gen, output_file)) {
        fprintf(stderr, "Error: Failed to save bytecode to '%s'\n", output_file);
        destroy_bytecode_generator(gen);
        free_ast(ast);
        destroy_tkstate(parser);
        freeTkList(tokens);
        free(source);
        return false;
    }
    
    /* Clean up */
    destroy_bytecode_generator(gen);
    free_ast(ast);
    destroy_tkstate(parser);
    freeTkList(tokens);
    free(source);
    
    return true;
}

/* Execute a file (source or bytecode) */
static bool execute_file(const char* input_file) {
    BytecodeGenerator* gen = NULL;
    bool needs_cleanup = false;
    
    /* Check if it's a bytecode file */
    if (is_bytecode_file(input_file)) {
        /* Load bytecode directly */
        gen = load_bytecode(input_file);
        if (!gen) {
            fprintf(stderr, "Error: Failed to load bytecode from '%s'\n", input_file);
            return false;
        }
    } else {
        /* Compile source file first */
        char* source = read_file(input_file);
        if (!source) {
            fprintf(stderr, "Error: Failed to read file '%s'\n", input_file);
            return false;
        }
        
        /* Lexical analysis */
        _sbToken* tokens = _sbLexer(source);
        if (!tokens) {
            fprintf(stderr, "Error: Failed to tokenize '%s'\n", input_file);
            free(source);
            return false;
        }
        
        /* Parse */
        Parser* parser = create_tkstate(tokens);
        if (!parser) {
            fprintf(stderr, "Error: Failed to create parser\n");
            freeTkList(tokens);
            free(source);
            return false;
        }
        
        reset_error();
        ASTNode* ast = parse_program(parser);
        
        if (!ast || syntaxErrorDetector) {
            fprintf(stderr, "Error: Failed to parse '%s'\n", input_file);
            if (syntaxErrorDetector) {
                fprintf(stderr, "Syntax error detected in file\n");
            }
            destroy_tkstate(parser);
            freeTkList(tokens);
            free(source);
            return false;
        }
        
        /* Generate bytecode */
        gen = create_bytecode_generator();
        if (!gen) {
            fprintf(stderr, "Error: Failed to create bytecode generator\n");
            free_ast(ast);
            destroy_tkstate(parser);
            freeTkList(tokens);
            free(source);
            return false;
        }
        
        if (!generate_bytecode(gen, ast)) {
            fprintf(stderr, "Error: Failed to generate bytecode\n");
            destroy_bytecode_generator(gen);
            free_ast(ast);
            destroy_tkstate(parser);
            freeTkList(tokens);
            free(source);
            return false;
        }
        
        /* Clean up parsing resources */
        free_ast(ast);
        destroy_tkstate(parser);
        freeTkList(tokens);
        free(source);
        needs_cleanup = true;
    }
    
    /* Create VM */
    VM* vm = create_vm();
    if (!vm) {
        fprintf(stderr, "Error: Failed to create VM\n");
        if (needs_cleanup) destroy_bytecode_generator(gen);
        return false;
    }
    
    /* Load bytecode into VM */
    if (!vm_load_bytecode(vm, gen)) {
        fprintf(stderr, "Error: Failed to load bytecode into VM\n");
        vm_print_error(vm);
        destroy_vm(vm);
        if (needs_cleanup) destroy_bytecode_generator(gen);
        return false;
    }
    
    /* Execute */
    VMError result = vm_execute(vm);
    
    if (result != VM_OK) {
        vm_print_error(vm);
        destroy_vm(vm);
        if (needs_cleanup) destroy_bytecode_generator(gen);
        return false;
    }
    
    /* Clean up */
    destroy_vm(vm);
    if (needs_cleanup) destroy_bytecode_generator(gen);
    
    return true;
}