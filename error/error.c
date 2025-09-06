/*
 * SB - Language
 * By Laman28
 * Compiler - Error
 * Not welcome to use /XD
 */

#include "error.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

bool syntaxErrorDetector;
bool repl_check_syntax;
bool incomplete_syntax;

void init_repl_check_syntax() {
    repl_check_syntax = false;
    incomplete_syntax = false;
}

bool check_for_incomplete_syntax() {
    return incomplete_syntax;
}

void replcs(bool b) {
    repl_check_syntax = b;
}

void reset_error() {
    syntaxErrorDetector = false;
    incomplete_syntax = false;
}

void lexError(const char* errinfo, int c, int l){
    fprintf(stderr, "An error occurred during processing\n");
    fprintf(stderr, "\tat line: %d, col: %d\n", l, c);
    fprintf(stderr, "LexerError: %s\n", errinfo);
}

void memoryError(Parser* parser, const char* errinfo){
    fprintf(stderr, "An error occurred during processing\n");
    if (parser->position < parser->size) {
        _sbToken* token = &parser->tk[parser->position];
        fprintf(stderr, "\tat line %d, column %d: \n", token->line, token->pos);
    } else if (parser->position > 0 && parser->position - 1 < parser->size) {
        _sbToken* token = &parser->tk[parser->position - 1];
        fprintf(stderr, "\tafter line %d, column %d: \n", token->line, token->pos);
    }
    fprintf(stderr, "MemoryError: %s\n", errinfo);
}

void syntaxError(Parser* parser, const char* format, ...) {
    if (repl_check_syntax)
        return;
    syntaxErrorDetector = true;
    fprintf(stderr, "An error occurred during processing\n");

    // 打印位置信息
    if (parser->position < parser->size) {
        _sbToken* token = &parser->tk[parser->position];
        fprintf(stderr, "\tat line %d, column %d: \n", token->line, token->pos);
    } else if (parser->position > 0 && parser->position - 1 < parser->size) {
        _sbToken* token = &parser->tk[parser->position - 1];
        fprintf(stderr, "\tafter line %d, column %d: \n", token->line, token->pos);
    }

    // 打印错误消息
    fprintf(stderr, "SyntaxError: ");
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    fprintf(stderr, "\n");
}

void expect_token(Parser* parser, const char* expected) {
    if (parser->position < parser->size) {
        _sbToken* token = &parser->tk[parser->position];
        syntaxError(parser, "expected '%s' but found '%s'", expected, token->tk);
    } else {
        syntaxError(parser, "expected '%s' but reached end of input", expected);
    }
}

void unexpected_token(Parser* parser) {
    if (parser->position < parser->size) {
        _sbToken* token = &parser->tk[parser->position];
        syntaxError(parser, "unexpected token '%s'", token->tk);
    } else {
        syntaxError(parser, "unexpected end of input");
    }
}

void unclosed_delimiter(Parser* parser, const char* delimiter, int start_line, int start_column) {
    syntaxError(parser, "unclosed '%s' (opened at line %d, column %d)",
                 delimiter, start_line, start_column);
}

void missing_semicolon(Parser* parser) {
    //Incomplete sentence
    incomplete_syntax = true;
    syntaxError(parser, "missing ';' at end of statement");
}

bool require_semicolon(Parser* parser) {
    // next is '}' or end of file
    if (parser->position >= parser->size && !match_token(parser, "}")) {
        missing_semicolon(parser);
        return false;
    }

    // require ';'
    if (!match_token(parser, ";")) {
        missing_semicolon(parser);
        return false;
    }

    next(parser); // remove ';'
    return true;
}

void bytecode_error(const char* format, ...) {
    fprintf(stderr, "BytecodeError: ");
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, "\n");
}


/**
 * Set VM error state and error message
 */
void vm_error(_sbVM* vm, VMError error, const char* format, ...) {
    if (!vm) return;

    vm->error = true;

    vm->last_error = error;
    vm->running = false;

    if (vm->error_message) {
        free(vm->error_message);
        vm->error_message = nullptr;
    }

    char buffer[512];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    vm->error_message = _s_strdup(buffer);

    vm_print_error(vm);
    if (vm->debug) {
        printf("\n=== Debug output while outputing an error ===\n");
        vm_print_status(vm);
    }
}

/**
 * Get error string for error code
 */
const char* vm_error_string(VMError error) {
    switch (error) {
        case VM_OK: return "OK";
        case VM_RUNTIME_ERROR: return "RuntimeError";
        case VM_STACK_OVERFLOW: return "StackOverflowError";
        case VM_STACK_UNDERFLOW: return "StackUnderflowError";
        case VM_UNDEFINED_VARIABLE: return "UndefinedVariableError";
        case VM_TYPE_ERROR: return "TypeError";
        case VM_DIVISION_BY_ZERO: return "DivisionByZeroError";
        case VM_INDEX_OUT_OF_BOUNDS: return "IndexOutOfBoundsError";
        case VM_UNDEFINED_FUNCTION: return "UndefinedFunctionError";
        case VM_ARGUMENT_MISMATCH: return "ArgumentMismatchError";
        case VM_LOAD_ERROR: return "LoadError";
        case VM_MEMORY_ERROR: return "MemoryError";
        case VM_INVALID_OPCODE: return "InvalidOpcodeError";
        case VM_UNDEFINED_MEMBER: return "UndefinedMemberError";
        case VM_NOT_A_STRUCT: return "NotAStructError";
        default: return "UnknownError";
    }
}

/**
 * Print error information to stderr
 */
void vm_print_error(_sbVM* vm) {
    if (!vm) return;

    vm->pc--;

    fprintf(stderr, "An error occurred during running");
    if (vm->error_from_native) {
        fprintf(stderr, " (from native function)\n");
    }
    else {
        fprintf(stderr, " (from source code)\n");
    }

    fprintf(stderr, "%s", vm_error_string(vm->last_error));
    if (vm->error_message) {
        fprintf(stderr, ": %s", vm->error_message);
    }
    fprintf(stderr, "\n");

    if (vm->pc < vm->instruction_count) {
        Instruction* current_inst = &vm->instructions[vm->pc];

        // Show location information
        if (vm->is_bytecode_execution) {
            fprintf(stderr, "  at instruction %zu in <bytecode>\n", vm->pc);
        } else if (vm->source_filename && current_inst->source_line >= 0) {
            fprintf(stderr, "  at %s:%d:%d (instruction %zu)\n",
                    vm->source_filename, current_inst->source_line + 1,
                    current_inst->source_column + 1, vm->pc);

            // Show source code snippet if available
            if (vm->source_content) {
                fprintf(stderr, "  Traceback (most recent call last):\n");

                // Split source content into lines
                char* content_copy = _s_strdup(vm->source_content);
                if (content_copy) {
                    char* line = _no_skip_strtok(content_copy, "\n");
                    int line_num = 1;
                    int error_line = current_inst->source_line + 1; // Convert from 0-based to 1-based

                    // Show context: 2 lines before and after the error line
                    int start_line = (error_line - 2) > 1 ? (error_line - 2) : 1;
                    int end_line = error_line + 2;

                    while (line != nullptr && line_num <= end_line) {
                        if (line_num >= start_line) {
                            if (line_num == error_line) {
                                fprintf(stderr, " -> %4d | %s\n", line_num, line);
                            } else {
                                fprintf(stderr, "    %4d | %s\n", line_num, line);
                            }
                        }
                        line = _no_skip_strtok(nullptr, "\n");
                        line_num++;
                    }
                    free(content_copy);
                } else {
                    fprintf(stderr, "    <bytecode>\n");
                }
            }
        } else {
            fprintf(stderr, "  at instruction %zu\n", vm->pc);
        }
    }
    fprintf(stderr, "\n");
    vm->pc++;
}

