/*
 * SB - Language
 * Compiler Frontend and Runtime
 * Main entry point for the SB language
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include "vm/vm.h"
#include "bytecode/bytecode.h"
#include "parser/parser.h"
#include "lexer/lexer.h"
#include "error/error.h"
#include "information.h"
#include "builtin/base_functions.h"

#ifdef ENABLE_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif

#if defined(__clang__)
#define COMPILER_INFORMATION "Clang"
#elif defined(__ICC) || defined(__INTEL_COMPILER)
#define COMPILER_INFORMATION "Intel ICC"
#elif defined(__GNUC__) || defined(__GNUG__)
#define COMPILER_INFORMATION "GCC"
#elif defined(_MSC_VER)
#define COMPILER_INFORMATION "MSVC"
#else
#define COMPILER_INFORMATION "Unknown Compiler"
#endif

static void print_compiler_version() {
#if defined(__clang__)
    printf("%d.%d.%d", __clang_major__, __clang_minor__, __clang_patchlevel__);
#elif defined(__GNUC__)
    printf("%d.%d.%d", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#elif defined(_MSC_VER)
    printf("%d", _MSC_VER);
#else
    printf("unknown");
#endif
}

#if defined(_WIN32) || defined(_WIN64)
#define COMPILED_FOR_PLATFORM "nt"
#elif defined(__linux__) || defined(__unix__)
#define COMPILED_FOR_PLATFORM "posix"
#elif defined(__APPLE__) && defined(__MACH__)
#define COMPILED_FOR_PLATFORM "darwin"
#else
#define COMPILED_FOR_PLATFORM "unknown"
#endif

// Debug mode
bool debugmode = false;
// Enable repl
bool start_repl = false;

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
static void run_repl();

int main(int argc, char** argv) {
    /* Parse command line arguments */
    init_repl_check_syntax();
    Options options = parse_arguments(argc, argv);

    bool success = true;
    
    if (options.file_count == 0) {
        //fprintf(stderr, "Error: No input files specified\n");
        //print_usage(argv[0]);
        start_repl = true;
    }

    if (start_repl) {
        run_repl();
        goto clean_memory;
    }
    
    /* Check for multiple files with -o option */
    if (options.compile_only && options.output_file && options.file_count > 1) {
        success = false;
        fprintf(stderr, "Error: Cannot specify output file with multiple input files\n");
        goto clean_memory;
    }

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
        if (getcwd(cwd, sizeof(cwd)) == nullptr) {
            fprintf(stderr, "Error: Failed to get current working directory\n");
            success = false;
            break;
        }
        
        if (options.compile_only) {
            /* Compile only mode */
            char* output_file = nullptr;
            
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
        }
        else {
            /* Execute mode */
            if (options.file_count > 1)
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
clean_memory:
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
        .output_file = nullptr,
        .input_files = nullptr,
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
            }
            else if (strcmp(argv[i], "-o") == 0) {
                if (i + 1 >= argc) {
                    fprintf(stderr, "Error: -o requires an argument\n");
                    exit(1);
                }
                options.output_file = _s_strdup(argv[++i]);
            }
            else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
                print_version();
                exit(0);
            }
            else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
                print_usage(argv[0]);
                exit(0);
            }
            else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "-g") == 0 || strcmp(argv[i], "--debug") == 0) {
                debugmode = true;
            }
            else if (strcmp(argv[i], "--repl") == 0) {
                start_repl = true;
            }
            else {
                fprintf(stderr, "Error: Unknown option '%s'\n", argv[i]);
                print_usage(argv[0]);
                exit(1);
            }
        }
        else {
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
    printf("  --repl            Start a sblang repl\n");
    printf("  -d, -g, --debug   Enable debugging for vm when vm shutdown\n");
    printf("  -c                Compile only (generate .sbc files)\n");
    printf("  -o <file>         Specify output file (only with single input file)\n");
    printf("  -v, --version     Show version information\n");
    printf("  -h, --help        Show this help message\n");
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

/* Check if file is bytecode based on magic value */
static bool is_bytecode_file(const char* filename) {
    return is_valid_bytecode_file(filename);
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
    
    /* Set parser for source line tracking */
    bytecode_set_parser(gen, parser);
    
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
    BytecodeGenerator* gen = nullptr;
    bool needs_cleanup = false;
    char* source_content = nullptr;
    bool bytecode = false;
    
    /* Check if it's a bytecode file */
    if (is_bytecode_file(input_file)) {
        /* Load bytecode directly */
        bytecode = true;
        gen = load_bytecode(input_file);
        if (!gen) {
            fprintf(stderr, "Error: Failed to load bytecode from '%s'\n", input_file);
            return false;
        }
    }
    else {
        /* Compile source file first */
        char* source = read_file(input_file);
        if (!source) {
            fprintf(stderr, "Error: Failed to read file '%s'\n", input_file);
            return false;
        }
        
        /* Store source content for error reporting */
        source_content = _s_strdup(source);
        
        /* Lexical analysis */
        _sbToken* tokens = _sbLexer(source);
        if (!tokens) {
            fprintf(stderr, "Error: Failed to tokenize '%s'\n", input_file);
            if (source_content) free(source_content);
            free(source);
            return false;
        }
        
        /* Parse */
        Parser* parser = create_tkstate(tokens);
        if (!parser) {
            fprintf(stderr, "Error: Failed to create parser\n");
            freeTkList(tokens);
            if (source_content) free(source_content);
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
            if (source_content) free(source_content);
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
            if (source_content) free(source_content);
            free(source);
            return false;
        }
        
        /* Set parser for source line tracking */
        bytecode_set_parser(gen, parser);
        
        if (!generate_bytecode(gen, ast)) {
            fprintf(stderr, "Error: Failed to generate bytecode\n");
            destroy_bytecode_generator(gen);
            free_ast(ast);
            destroy_tkstate(parser);
            freeTkList(tokens);
            if (source_content) free(source_content);
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
    //printf("DEBUG: About to create VM\n");
    _sbVM* vm = create_vm();
    if (debugmode)
        enable_debug(vm);

    if (!vm) {
        fprintf(stderr, "Error: Failed to create VM\n");
        if (needs_cleanup) destroy_bytecode_generator(gen);
        if (source_content) free(source_content);
        return false;
    }
    
    /* Set source information for backtrace */
    vm_set_source_info(vm, input_file, bytecode);
    if (source_content) {
        free(source_content);
        source_content = nullptr;
    } else {
        vm_set_bytecode_execution(vm, true);
    }
    
    /* Load bytecode into VM */
    if (!vm_load_bytecode(vm, gen)) {
        fprintf(stderr, "Error: Failed to load bytecode into VM\n");
        //vm_print_error(vm);
        destroy_vm(vm);
        if (needs_cleanup) destroy_bytecode_generator(gen);
        return false;
    }
    
    /* Execute */
    VMError result = vm_execute(vm);
    
    if (result != VM_OK) {
        //vm_print_error(vm);
        destroy_vm(vm);
        if (needs_cleanup) destroy_bytecode_generator(gen);
        return false;
    }

    if (vm->debug) {
        printf("\n=== Normally end of running ===\n");
        vm_print_status(vm);
    }

    /* Clean up */
    destroy_vm(vm);
    if (needs_cleanup) destroy_bytecode_generator(gen);
    
    /* Clean up any static buffers */
    //vm_cleanup_static_buffers(vm);
    
    return true;
}

static char* _sbl_input(const char *s) {
    char* buffer;
#ifndef ENABLE_READLINE
    printf("%s", s);
    buffer = malloc(sizeof(char));
    assert(buffer != nullptr);
    int length = 0;
    int ch;
    while (true) {
        ch = getchar();

        if(ch=='\n'||ch=='\0') break;

        if (ch == EOF) {
            putchar('\n');
            free(buffer);
            exit(0);
            buffer = nullptr;
        }

        buffer = realloc(buffer, (length + 1) * sizeof(char));
        assert(buffer != nullptr);
        buffer[length++] = ch;
    }

    buffer[length] = '\0';

#else
    buffer = readline(s);
#endif

    return buffer;
}

void run_repl() {
    _sbVM* vm = create_vm(); // Init for virtual machine
    vm_set_source_info(vm, "<stdin>", false);

    if (debugmode)
        enable_debug(vm);

    printf("SBLang - v%s - REPL - By Laman28\n", VERSION);
    printf("Compiled with %s(version: ", COMPILER_INFORMATION);
    print_compiler_version();
    printf(") on %s [compiled on %s %s]\n", COMPILED_FOR_PLATFORM, __DATE__, __TIME__);

    while (true) {
        incomplete_syntax = false;
        char* buffer = _sbl_input("> ");

        if (buffer == nullptr) break;
        if (is_empty(buffer)) continue;

        char* buf = _s_strdup(buffer);
        free(buffer);

#ifdef ENABLE_READLINE
        add_history(buf);
#endif

        replcs(true);
        check:
        _sbToken* tk = _sbLexer(buf);
        if (incomplete_syntax) goto addition_input;
        if (!tk) continue;

        Parser* parser = create_tkstate(tk);
        assert(parser != nullptr);

        reset_error();
        ASTNode* ast = parse_program(parser);

        if (check_for_incomplete_syntax()) {
            freeTkList(tk);
            free_ast(ast);
            destroy_tkstate(parser);

            addition_input:
            incomplete_syntax = false;
            char* continue_input = _sbl_input(">> ");
            if (continue_input == nullptr) break;
            if (is_empty(continue_input)) goto addition_input;

#ifdef ENABLE_READLINE
            add_history(continue_input);
#endif
            int len = strlen(buf) + strlen(continue_input);

            char* nbuf = calloc(len + 2, sizeof(char));
            assert(nbuf != nullptr);

            strcat(nbuf, buf);
            strcat(nbuf, "\n");
            strcat(nbuf, continue_input);

            free(continue_input);
            free(buf);
            buf = nbuf;
            goto check;
        }
        replcs(false);
        reset_error();

        parser = create_tkstate(tk);
        ast = parse_program(parser);
        if (syntaxErrorDetector) {
            freeTkList(tk);
            free_ast(ast);
            destroy_tkstate(parser);
            goto clean;
        }

        BytecodeGenerator* gen = create_bytecode_generator();
        assert(gen != nullptr);
        bytecode_set_parser(gen, parser);
        if (!generate_bytecode(gen, ast)) {
            destroy_bytecode_generator(gen);
            freeTkList(tk);
            free_ast(ast);
            destroy_tkstate(parser);
            continue;
        }

        vm_load_bytecode(vm, gen);
        destroy_bytecode_generator(gen);
        freeTkList(tk);
        free_ast(ast);
        destroy_tkstate(parser);

        vm_execute(vm);

        _sbValue result = vm_peek(vm, 0);
        if (result.type != VAL_NULL) {
            _sbValue s;
            if (result.type == VAL_STRING)
                s = toString(vm, result, true);
            else
                s = toString(vm, result, false);
            printf("(%s)-> %s\n", v_type(result), s.as.string);
        }
        vm->stack_top = 0;

        clean:
        free(buf);
    }

    if (vm->debug) {
        printf("\n=== Normally end of running ===\n");
        vm_print_status(vm);
    }

    destroy_vm(vm);
}