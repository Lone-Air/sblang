/* 
 * SB - Language
 * By Laman28
 * Compiler - Lexer
 * Not welcome to use /XD
 */

#include "lexer.h"
#include "../dynarray/dynarray.h"
#include "../error/error.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define UPDATE(x, c, l, p, t) tks = (_sbToken*)realloc(tks, sizeof(_sbToken) * (tkc + 2)); /* Reallocate a larger memory for token pool */ \
                  if(x != nullptr){ /* Check if the token is `nullptr` */ \
                      tks[tkc].tk = (char*)malloc(sizeof(char) * (strlen(x) + 1)); \
                      set_zero(tks[tkc].tk, sizeof(char) * (strlen(x) + 1)); \
                      strcpy(tks[tkc].tk, x); \
                  } \
                  else { \
                      tks[tkc].tk = nullptr; \
                  } \
                  tks[tkc].column = c; \
                  tks[tkc].line = l; \
                  tks[tkc].type = t; \
                  tks[tkc].pos = p; \
                  tkc ++; // Counter

#define RESET() delete_array(buffer); \
                buffer = new_array(DAT_TYPE_CHAR); \
                tk_sc = -1; \
                tk_sl = -1; \
                tk_sp = -1;

#define NEXT(_c, _s) _c = *(_s++)

#define NEXT_C(_c, _s) _c = *(_s++);\
                       col ++; \
                       pos ++;

#define SETPOS(c, l, p) tk_sc = c; \
                        tk_sl = l; \
                        tk_sp = p;

char* const keyList[] = {"if", "else", "while", "for", "load", "function", "return", "struct", "global", "continue", "break", nullptr};
_sbTk const typeList[] = {_sbIf, _sbElse, _sbWhile, _sbFor, _sbLoad, _sbFunction, _sbReturn, _sbStruct, _sbGlobal, _sbContinue, _sbBreak, -1};

short backslash(char c){ // Transfer the backslash with character into single character
    switch (c){
        case '\0': return -1;
        case '\\': return '\\';
        case 'n':  return '\n';
        case 't':  return '\t';
        case 'e':  return '\033';
        case 'r':  return '\r';
        case '\n': return -3;
    }
    return -2;
}

_sbTk _keyword_detect(const char* key){ // Type detector
    for(int i = 0; keyList[i] != nullptr; i++){
        if (strcmp(keyList[i], key) == 0) // Detect whether it is a built-in keyword
          return typeList[i];
    }
    return _sbKey;
}

_sbToken* _sbPreLexer(const char* src){ // Pre-Lexer
    _sbToken* tks; /* Initialization */
    int tkc = 0;

    tks = (_sbToken*)malloc(sizeof(_sbToken));
    assert(tks != nullptr);
    set_zero(tks, sizeof(_sbToken));

    char c;
    Array* buffer = new_array(DAT_TYPE_CHAR);

    int col = 0;
    int pos = 0;
    int line = 0;

    int nummode = 0;

    int strmode = 0;
    char strfront = '\0';

    int tk_sc = -1;
    int tk_sl = -1;
    int tk_sp = -1;

    while ((NEXT(c, src)) != '\0'){
        switch (c){
            case ',': case ';': case '.':
            case '[': case ']': case '%':
            case '(': case ')': case '{':
            case '}': case '+': case '^':
            case '~':
                if (strmode){ /* Skip saving token within a string */
                    append(buffer, c, nullptr);
                }
                else if (nummode){ /* Here is nolonger a number, save it and then save the symbol */
                    UPDATE(buffer->data->dat_char, tk_sc, tk_sl, tk_sp, _sbNum);
                    RESET();
                    nummode = 0;
                    append(buffer, c, nullptr);
                    SETPOS(col, line, pos);
                    UPDATE(buffer->data->dat_char, tk_sc, tk_sl, tk_sp, _sbSym);
                    RESET();
                }
                else {
                    if (!(strcmp(buffer->data->dat_char, "") == 0)){ /* If here were keywords within buffer, save it and then save the symbol */
                        UPDATE(buffer->data->dat_char, tk_sc, tk_sl, tk_sp, _keyword_detect(buffer->data->dat_char));
                        RESET();
                    }
                    SETPOS(col, line, pos);
                    append(buffer, c, nullptr);
                    UPDATE(buffer->data->dat_char, tk_sc, tk_sl, tk_sp, _sbSym);
                    RESET();
                }
                break; /* Catch symbols */
            case '/':
                if (strmode){ /* Skip saving token within a string */
                    append(buffer, c, nullptr);
                    break;
                }
                else if (nummode){ /* Here is nolonger a number, save it */
                    UPDATE(buffer->data->dat_char, tk_sc, tk_sl, tk_sp, _sbNum);
                    RESET();
                    nummode = 0;
                }
                if (!(strcmp(buffer->data->dat_char, "") == 0)){ /* If here were keywords within buffer, save it and then continue */
                    UPDATE(buffer->data->dat_char, tk_sc, tk_sl, tk_sp, _keyword_detect(buffer->data->dat_char));
                    RESET();
                }
                SETPOS(col, line, pos);
                append(buffer, c, nullptr);
                NEXT_C(c, src);
                switch(c){
                  case '\0':
                    UPDATE(buffer->data->dat_char, tk_sc, tk_sl, tk_sp, _sbSym);
                    RESET();
                    goto endlexer;
                  case '/': // Single line note // ...
                    RESET();
                    while((NEXT(c, src)) != '\0'){
                        col ++;
                        if(c == '\n'){
                            line ++;
                            pos = 0;
                            break;
                        }
                    }
                    if(!c) goto endlexer;
                    break;
                  case '*': // Multiple line notes /* ... */
                    RESET();
                    while((NEXT(c, src)) != '\0'){
                        col ++;
                        if(c == '\n'){
                            line ++;
                            pos = 0;
                        }
                        switch(c){
                          case '*':
                            NEXT_C(c, src);
                            if(c == '/') goto end_of_note;
                            break;
                        }
                    }
end_of_note:
                    if(!c) goto endlexer;
                    break;
                  default: // Others /<others>
                    UPDATE(buffer->data->dat_char, tk_sc, tk_sl, tk_sp, _sbSym);
                    RESET();
                    src --;
                    col --;
                    break;
                }
                break;
            case '!':
                if (strmode){ /* Skip saving token within a string */
                    append(buffer, c, nullptr);
                    break;
                }
                else if (nummode){ /* Here is nolonger a number, save it */
                    UPDATE(buffer->data->dat_char, tk_sc, tk_sl, tk_sp, _sbNum);
                    RESET();
                    nummode = 0;
                }
                if (!(strcmp(buffer->data->dat_char, "") == 0)){ /* If here were keywords within buffer, save it and then continue */
                    UPDATE(buffer->data->dat_char, tk_sc, tk_sl, tk_sp, _keyword_detect(buffer->data->dat_char));
                    RESET();
                }
                SETPOS(col, line, pos);
                append(buffer, c, nullptr);
                NEXT_C(c, src);
                switch(c){
                  case '\0':
                    UPDATE(buffer->data->dat_char, tk_sc, tk_sl, tk_sp, _sbSym);
                    RESET();
                    goto endlexer;
                  case '=': // Symbol `!=`
                    append(buffer, c, nullptr);
                    UPDATE(buffer->data->dat_char, tk_sc, tk_sl, tk_sp, _sbSym);
                    RESET();
                    break;
                  default:
                    UPDATE(buffer->data->dat_char, tk_sc, tk_sl, tk_sp, _sbSym);
                    RESET();
                    src --;
                    col --;
                    pos --;
                    break;
                }
                break;
            case '-':
                if (strmode){ /* Skip saving token within a string */
                    append(buffer, c, nullptr);
                    break;
                }
                else if (nummode){ /* Here is nolonger a number, save it */
                    UPDATE(buffer->data->dat_char, tk_sc, tk_sl, tk_sp, _sbNum);
                    RESET();
                    nummode = 0;
                }
                if (!(strcmp(buffer->data->dat_char, "") == 0)){ /* If here were keywords within buffer, save it and then continue */
                    UPDATE(buffer->data->dat_char, tk_sc, tk_sl, tk_sp, _keyword_detect(buffer->data->dat_char));
                    RESET();
                }
                SETPOS(col, line, pos);
                append(buffer, c, nullptr);
                NEXT_C(c, src);
                switch(c){
                    case '\0':
                        UPDATE(buffer->data->dat_char, tk_sc, tk_sl, tk_sp, _sbSym);
                        RESET();
                        goto endlexer;
                    case '>': // Symbol `->`
                        append(buffer, c, nullptr);
                        UPDATE(buffer->data->dat_char, tk_sc, tk_sl, tk_sp, _sbSym);
                        RESET();
                        break;
                    default:
                        UPDATE(buffer->data->dat_char, tk_sc, tk_sl, tk_sp, _sbSym);
                        RESET();
                        src --;
                        col --;
                        pos --;
                        break;
                }
                break;
            case '*':
                if (strmode){ /* Skip saving token within a string */
                    append(buffer, c, nullptr);
                    break;
                }
                else if (nummode){ /* Here is nolonger a number, save it */
                    UPDATE(buffer->data->dat_char, tk_sc, tk_sl, tk_sp, _sbNum);
                    RESET();
                    nummode = 0;
                }
                if (!(strcmp(buffer->data->dat_char, "") == 0)){ /* If here were keywords within buffer, save it and then continue */
                    UPDATE(buffer->data->dat_char, tk_sc, tk_sl, tk_sp, _keyword_detect(buffer->data->dat_char));
                    RESET();
                }
                SETPOS(col, line, pos);
                append(buffer, c, nullptr);
                NEXT_C(c, src);
                switch(c){
                    case '\0':
                        UPDATE(buffer->data->dat_char, tk_sc, tk_sl, tk_sp, _sbSym);
                        RESET();
                        goto endlexer;
                    case '*': // Symbol `**`
                        append(buffer, c, nullptr);
                        UPDATE(buffer->data->dat_char, tk_sc, tk_sl, tk_sp, _sbSym);
                        RESET();
                        break;
                    default:
                        UPDATE(buffer->data->dat_char, tk_sc, tk_sl, tk_sp, _sbSym);
                        RESET();
                        src --;
                        col --;
                        pos --;
                        break;
                }
                break;
            case '&':
                if (strmode){ /* Skip saving token within a string */
                    append(buffer, c, nullptr);
                    break;
                }
                else if (nummode){ /* Here is nolonger a number, save it */
                    UPDATE(buffer->data->dat_char, tk_sc, tk_sl, tk_sp, _sbNum);
                    RESET();
                    nummode = 0;
                }
                if (!(strcmp(buffer->data->dat_char, "") == 0)){ /* If here were keywords within buffer, save it and then continue */
                    UPDATE(buffer->data->dat_char, tk_sc, tk_sl, tk_sp, _keyword_detect(buffer->data->dat_char));
                    RESET();
                }
                SETPOS(col, line, pos);
                append(buffer, c, nullptr);
                NEXT_C(c, src);
                switch(c){
                    case '\0':
                        UPDATE(buffer->data->dat_char, tk_sc, tk_sl, tk_sp, _sbSym);
                        RESET();
                        goto endlexer;
                    case '&': // Symbol `&&`
                        append(buffer, c, nullptr);
                        UPDATE(buffer->data->dat_char, tk_sc, tk_sl, tk_sp, _sbSym);
                        RESET();
                        break;
                    default:
                        UPDATE(buffer->data->dat_char, tk_sc, tk_sl, tk_sp, _sbSym);
                        RESET();
                        src --;
                        col --;
                        pos --;
                        break;
                }
                break;
            case '|':
                if (strmode){ /* Skip saving token within a string */
                    append(buffer, c, nullptr);
                    break;
                }
                else if (nummode){ /* Here is nolonger a number, save it */
                    UPDATE(buffer->data->dat_char, tk_sc, tk_sl, tk_sp, _sbNum);
                    RESET();
                    nummode = 0;
                }
                if (!(strcmp(buffer->data->dat_char, "") == 0)){ /* If here were keywords within buffer, save it and then continue */
                    UPDATE(buffer->data->dat_char, tk_sc, tk_sl, tk_sp, _keyword_detect(buffer->data->dat_char));
                    RESET();
                }
                SETPOS(col, line, pos);
                append(buffer, c, nullptr);
                NEXT_C(c, src);
                switch(c){
                    case '\0':
                        UPDATE(buffer->data->dat_char, tk_sc, tk_sl, tk_sp, _sbSym);
                        RESET();
                        goto endlexer;
                    case '|': // Symbol `&&`
                        append(buffer, c, nullptr);
                        UPDATE(buffer->data->dat_char, tk_sc, tk_sl, tk_sp, _sbSym);
                        RESET();
                        break;
                    default:
                        UPDATE(buffer->data->dat_char, tk_sc, tk_sl, tk_sp, _sbSym);
                        RESET();
                        src --;
                        col --;
                        pos --;
                        break;
                }
                break;
            case '>': case '<': case '=':
                if (strmode){ /* Skip saving token within a string */
                    append(buffer, c, nullptr);
                    break;
                }
                else if (nummode){ /* Here is nolonger a number, save it */
                    UPDATE(buffer->data->dat_char, tk_sc, tk_sl, tk_sp, _sbNum);
                    RESET();
                    nummode = 0;
                    
                }
                if (!(strcmp(buffer->data->dat_char, "") == 0)){ /* If here were keywords within buffer, save it and then continue */
                    UPDATE(buffer->data->dat_char, tk_sc, tk_sl, tk_sp, _keyword_detect(buffer->data->dat_char));
                    RESET();
                }
                SETPOS(col, line, pos);
                append(buffer, c, nullptr);
                NEXT_C(c, src);
                switch(c){
                  case '\0':
                    UPDATE(buffer->data->dat_char, tk_sc, tk_sl, tk_sp, _sbSym);
                    RESET();
                    goto endlexer;
                  case '=': case '>': case '<': // Symbol `>=` `<=` `==` `><` `<>` `>>` `<<`
                    append(buffer, c, nullptr);
                    UPDATE(buffer->data->dat_char, tk_sc, tk_sl, tk_sp, _sbSym);
                    RESET();
                    break;
                  default:
                    UPDATE(buffer->data->dat_char, tk_sc, tk_sl, tk_sp, _sbSym);
                    RESET();
                    src --;
                    col --;
                    pos --;
                    break;
                }
                break;
            case '1': case '2': case '3':
            case '4': case '5': case '6':
            case '7': case '8': case '9':
            case '0':
                if(strmode || strcmp(buffer->data->dat_char, "") != 0 ){ /* Skip enabling number mode within a string */
                    append(buffer, c, nullptr);
                }
                else if (nummode){ /* Keep saving numbers */
                    append(buffer, c, nullptr);
                }
                else {
                    if(!(strcmp(buffer->data->dat_char, "") == 0)){ /* If here were keywords within buffer, save it and then enable number mode */
                        UPDATE(buffer->data->dat_char, tk_sc, tk_sl, tk_sp, _keyword_detect(buffer->data->dat_char));
                        RESET();
                    }
                    append(buffer, c, nullptr);
                    nummode = 1;
                    SETPOS(col, line, pos);
                }
                break; /* Enable number mode */
            case '\'':  case '"':
                if (nummode){ // End of number mode and save the number
                    nummode = 0;
                    UPDATE(buffer->data->dat_char, tk_sc, tk_sl, tk_sp, _sbNum);
                    RESET();
                }
                if (strmode == 0){ // Enable string mode
                    if(!(strcmp(buffer->data->dat_char, "") == 0)){
                        UPDATE(buffer->data->dat_char, tk_sc, tk_sl, tk_sp, _keyword_detect(buffer->data->dat_char));
                        RESET();
                    }
                    strmode = 1;
                    strfront = c; // Set the start character of string (' or ")
                    SETPOS(col, line, pos);
                }
                else {
                    if(strfront != c) // Check if the character is the end of string
                      append(buffer, c, nullptr);
                    else {
                        UPDATE(buffer->data->dat_char, tk_sc, tk_sl, tk_sp, _sbStr);
                        RESET();
                        strmode = 0;
                        strfront = '\0';
                    }
                }
                break; /* Enable/Disable string */
            case '\\':
                if (nummode){
                    nummode = 0;
                    UPDATE(buffer->data->dat_char, tk_sc, tk_sl, tk_sp, _sbNum);
                    RESET();
                }
                if(!(strcmp(buffer->data->dat_char, "") == 0) && !strmode){
                    UPDATE(buffer->data->dat_char, tk_sc, tk_sl, tk_sp, _keyword_detect(buffer->data->dat_char));
                    RESET();
                }
                NEXT_C(c, src);
                short _backslash = backslash(c);
                if(_backslash == -1){ // A backslash fell at the end of the file
                    lexError("Unexpected EOF appeared", pos, line);
                    return nullptr;
                }
                else if(_backslash == -2){
                    char* errinfo = malloc(sizeof(char) * 32);
                    sprintf(errinfo, "Invalid backslash transfer '\\%c'", c);
                    lexError(errinfo, pos, line);
                    free(errinfo);
                    return nullptr;
                }
                else if(_backslash == -3){
                    line ++;
                    pos = 0;
                    break;
                }
                else{
                    append(buffer, _backslash, nullptr);
                }
                break; /* Catch backslash+character */
            case ' ': case '\t':
                if(strmode){
                    append(buffer, c, nullptr);
                    break;
                }
                if (nummode){
                    nummode = 0;
                    UPDATE(buffer->data->dat_char, tk_sc, tk_sl, tk_sp, _sbNum);
                    RESET();
                    break;
                }
                if(strcmp(buffer->data->dat_char, "") == 0) 
                  break;
                UPDATE(buffer->data->dat_char, tk_sc, tk_sl, tk_sp, _keyword_detect(buffer->data->dat_char));
                RESET();
                break;
            case '\n':
                if(strmode){
                    line ++;
                    pos = 0;
                    append(buffer, c, nullptr);
                    break;
                }
                if (nummode){ // End a number
                    nummode = 0;
                    UPDATE(buffer->data->dat_char, tk_sc, tk_sl, tk_sp, _sbNum);
                    RESET();
                    line ++;
                    pos = 0;
                    break;
                }
                if(strcmp(buffer->data->dat_char, "") == 0) {
                    line ++;
                    pos = 0;
                    break;
                }
                UPDATE(buffer->data->dat_char, tk_sc, tk_sl, tk_sp, _keyword_detect(buffer->data->dat_char));
                RESET();
                line ++;
                pos = 0;
                break; /* Next line */
            default:
                if (nummode){
                    nummode = 0;
                    UPDATE(buffer->data->dat_char, tk_sc, tk_sl, tk_sp, _sbNum);
                    RESET();
                }
                if(strmode){
                    append(buffer, c, nullptr);
                    break;
                }
                if(tk_sc == -1 && tk_sl == -1){
                    SETPOS(col, line, pos);
                }
                append(buffer, c, nullptr);
                break;
        }
    col ++;
    pos ++;
    }

endlexer:

    if(strmode){ // File had ended, but the string syntex still doesn't end
        lexError("UnexpectedEOF", pos, line);
        return nullptr;
    }

    if(!(strcmp(buffer->data->dat_char, "") == 0)){ // If the buffer is not empty, save it
        if(!nummode){ // The last token is number or not
          UPDATE(buffer->data->dat_char, tk_sc, tk_sl, tk_sp, _keyword_detect(buffer->data->dat_char));
        }
        else{
          UPDATE(buffer->data->dat_char, tk_sc, tk_sl, tk_sp, _sbNum);
        }
        RESET();
    }

    delete_array(buffer);

    UPDATE(nullptr, col, line, pos, _sbEnd); // End of tokens

    return tks;
}

_sbToken* _sbLexer(const char* src){ // Final Lexer
    _sbToken* pre = _sbPreLexer(src); /* Pre-Lexer */
    _sbToken* pre_s = pre;

    if (pre == nullptr) return nullptr;

    _sbToken* tks; /* Initialization */
    int tkc = 0;

    tks = (_sbToken*)malloc(sizeof(_sbToken));
    set_zero(tks, sizeof(_sbToken));

    char* buff;

    _sbToken _tk;
    _sbToken _tk_l;

    while ((_tk = *(pre ++)).type != _sbEnd){
        if (_tk.type == _sbNum){
            _tk_l = _tk; // Temporary save current token
            _tk = *(pre ++); // Next token
            switch(_tk.type){
              case _sbEnd: // Is the last token
                UPDATE(_tk_l.tk, _tk_l.column, _tk_l.line, _tk_l.pos, _tk_l.type);
                goto end_of_flexer;
              case _sbSym: // <Num>?
                if (strcmp(_tk.tk, ".") == 0){ // <Num>.?
                    _tk = *(pre ++);
                    if (_tk.type == _sbNum){ // <Num>.<Num>
                        buff = (char*)calloc(sizeof(char), 2 + strlen(_tk_l.tk) + strlen(_tk.tk));
                        strcat(buff, _tk_l.tk);
                        strcat(buff, ".");
                        strcat(buff, _tk.tk);
                        UPDATE(buff, _tk.column, _tk.line, _tk.pos, _sbNum);
                        free(buff);
                        break;
                    }
                    else{ // <Num>.<others>
                        buff = (char*)calloc(sizeof(char), 2 + strlen(_tk_l.tk));
                        strcat(buff, _tk_l.tk);
                        strcat(buff, ".");
                        UPDATE(buff, _tk_l.column, _tk_l.line, _tk_l.pos, _sbNum);
                        free(buff);

                        if (_tk.type == _sbEnd){ // Is the last token
                            goto end_of_flexer;
                        }
                        else{ // Isn't the last token, save it
                            UPDATE(_tk.tk, _tk.column, _tk.line, _tk.pos, _tk.type);
                        }
                        break;
                    }
                }
                UPDATE(_tk_l.tk, _tk_l.column, _tk_l.line, _tk_l.pos, _tk_l.type);
                UPDATE(_tk.tk, _tk.column, _tk.line, _tk.pos, _tk.type);
                break;
              default: // Single number
                UPDATE(_tk_l.tk, _tk_l.column, _tk_l.line, _tk_l.pos, _tk_l.type);
                UPDATE(_tk.tk, _tk.column, _tk.line, _tk.pos, _tk.type);
                break;
            }
        }
        else if (_tk.type == _sbSym){ // Token is symbol
            if (!(strcmp(_tk.tk, ".") == 0)){ // Check whether the symbol is `.`
                UPDATE(_tk.tk, _tk.column, _tk.line, _tk.pos, _tk.type); // others, save it only
                continue;
            }

            _tk_l = _tk; // Is `.`
            _tk = *(pre ++);
            switch(_tk.type){
              case _sbEnd: // Is the last token
                UPDATE(_tk_l.tk, _tk_l.column, _tk_l.line, _tk_l.pos, _tk_l.type);
                goto end_of_flexer;
              case _sbNum: // .<Num>
                buff = (char*)calloc(sizeof(char), 2 + strlen(_tk_l.tk));
                strcat(buff, ".");
                strcat(buff, _tk.tk);
                UPDATE(buff, _tk.column, _tk.line, _tk.pos, _sbNum);
                break;
              default: // .<others>
                UPDATE(_tk_l.tk, _tk_l.column, _tk_l.line, _tk_l.pos, _tk_l.type);
                UPDATE(_tk.tk, _tk.column, _tk.line, _tk.pos, _tk.type);
                break;
            }
        }
        else{
            UPDATE(_tk.tk, _tk.column, _tk.line, _tk.pos, _tk.type); // Token is not number or symbol
        }
    }
end_of_flexer:

    UPDATE(nullptr, _tk.column, _tk.line, _tk.pos, _sbEnd); // End of tokens
    freeTkList(pre_s);

    return tks;
}

void freeTkList(_sbToken* tk){
    _sbToken* tk_s = tk;

    _sbToken _tk;
    while((_tk = *(tk ++)).type != _sbEnd){
        free(_tk.tk); // Free tokens
    }

    free(tk_s); // Free main token pool
}

#ifdef _SBL_LEXER_TEST

int main(){
    const char* s = "function f1(arg1){\n"
                    "    if(arg1>1024){\n"
                    "        return arg1;\n"
                    "    }\n"
                    "    else{\n"
                    "        return f1(arg1 + 1);\n"
                    "    }\n"
                    "}\n"
                    "print(\"result: \", f1(0), '\n');";
    _sbToken* _l = _sbLexer(s);

    _sbToken* _l_s = _l;

    if(_l != nullptr){
        _sbToken t;
        while((t = *(_l++)).tk != nullptr){
            fprintf(stdout, "{token: %s, line: %d, column: %d, position: %d, type: %d}\n", t.tk, t.line, t.column, t.pos, t.type);
        }
    }
    freeTkList(_l_s);
    return 0;
}

#endif