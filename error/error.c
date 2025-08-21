/*
 * SB - Language
 * By Laman28
 * Compiler - Error
 * Not welcome to use /XD
 */

#include "error.h"
#include <stdio.h>
#include <stdarg.h>

bool syntaxErrorDetector;

void reset_error() {
    syntaxErrorDetector = false;
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
