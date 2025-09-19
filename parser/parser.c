/*
 * SB - Language
 * By Laman28
 * Compiler - AST & Parser
 * Not welcome to use /XD
 */

#include "parser.h"

#include <ctype.h>
#include <assert.h>

#include "../error/error.h"

#ifdef _SBL_AST_TEST
#define  __USE_XOPEN2K
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

char* _s_strdup(const char* str) {
    if (!str) return nullptr;

    if (str[0] == '\0') {
        char* copy = (char*)calloc(1, sizeof(char));
        if (copy) {
            copy[0] = '\0';
        }
        return copy;
    }

    size_t len = strlen(str) + 1;
    char* copy = (char*)calloc(len, sizeof(char));
    if (copy) {
        memcpy(copy, str, len);
    }
    return copy;
}

_sbTkState* create_tkstate(_sbToken* tk) {
    auto st = (_sbTkState*)malloc(sizeof(_sbTkState)); // Initialize
    assert(st != nullptr);
    st->tk = (_sbToken*)malloc(sizeof(_sbToken));
    assert(st->tk != nullptr);

    _sbToken* tk_s = tk;

    int size = 0;
    _sbToken tkc;
    while ((tkc = *(tk ++)).type != _sbEnd) {
        st->tk = (_sbToken*)realloc(st->tk,(size + 1) * sizeof(_sbToken));
        assert(st->tk != nullptr);
        st->tk[size].tk = (char*)malloc(sizeof(char) * (strlen(tkc.tk) + 1)); // Duplicate token
        assert(st->tk[size].tk != nullptr);
        strcpy(st->tk[size].tk, tkc.tk);
        st->tk[size].type = tkc.type;
        st->tk[size].line = tkc.line;
        st->tk[size].pos = tkc.pos;
        st->tk[size++].column = tkc.column;
    }

    st->tk = (_sbToken*)realloc(st->tk,(size + 1) * sizeof(_sbToken));
    assert(st->tk != nullptr);
    st->tk[size].tk = nullptr; // End of token state
    st->tk[size].type = _sbEnd;

    st->size = size;
    st->position = 0;

    tk = tk_s; // Reset pointer of tokens to head;

    return st;
}

void destroy_tkstate(_sbTkState* tk) {
    for (int i = 0; i < tk->size; i++) {
        free(tk->tk[i].tk);
    }

    free(tk->tk);
    free(tk);
}

_sbToken* next(_sbTkState* parser) {
    if (parser->position >= parser->size) return nullptr;
    return &parser->tk[parser->position++];
}

_sbToken* peek(_sbTkState* parser) {
    if (parser->position >= parser->size) return nullptr;
    return &parser->tk[parser->position];
}

_sbToken* peek_next(_sbTkState* parser) {
    if (parser->position >= parser->size) return nullptr;
    return &parser->tk[parser->position + 1];
}

_sbToken* peek_ahead(Parser* parser, int offset) {
    int pos = parser->position + offset;
    if (pos >= parser->size) return nullptr;
    return &parser->tk[pos];
}

_sbToken* current_token(_sbTkState* parser) {
    if (parser->position >= parser->size) {
        return nullptr;
    }
    return &parser->tk[parser->position];
}

_sbToken* previous_token(_sbTkState* parser) {
    if (parser->position - 1 >= parser->size) {
        return nullptr;
    }
    return &parser->tk[parser->position - 1];
}

bool match_token(_sbTkState* parser, const char* expected) {
    if (parser->position >= parser->size) return false;
    return strcmp(parser->tk[parser->position].tk, expected) == 0;
}

bool check_ahead(Parser* parser, int offset, const char* expected) {
    int pos = parser->position + offset;
    if (pos >= parser->size) return false;
    return strcmp(parser->tk[pos].tk, expected) == 0;
}

ASTNode* create_ast_node(_sbNType type, Parser* parser) {
    ASTNode* node = calloc(1, sizeof(ASTNode));
    if (!node) return nullptr;
    node->type = type;
    
    // Set source location information
    if (parser && parser->position < parser->size) {
        _sbToken* token = &parser->tk[parser->position];
        node->source_line = token->line;
        node->source_column = token->pos;
    } else {
        node->source_line = 0;
        node->source_column = 0;
    }
    
    return node;
}

ASTNode* create_ast_node_with_token(_sbNType type, _sbToken* token) {
    ASTNode* node = calloc(1, sizeof(ASTNode));
    node->type = type;
    
    // Set source location information from specific token
    if (token) {
        node->source_line = token->line;
        node->source_column = token->pos;
    } else {
        node->source_line = 0;
        node->source_column = 0;
    }
    
    return node;
}

ASTNode* parse_program(Parser* parser) {
    reset_error();

    ASTNode* program = create_ast_node(_sbPROGRAM, parser);
    program->data.list.items = malloc(sizeof(ASTNode*));
    program->data.list.count = 0;

    while (parser->position < parser->size){
        ASTNode* stmt = parse_statement(parser);
        if (stmt) {
            program->data.list.items = realloc(program->data.list.items, (program->data.list.count + 1) * sizeof(ASTNode*) );
            program->data.list.items[program->data.list.count++] = stmt;
        } else {
            // Invalid token
            if (peek(parser)) {
                //fprintf(stderr,"-- Stopped: error appeared during processing\n");
                free_ast(stmt);
                free_ast(program);
                return nullptr;
            }
        }
    }

    if (syntaxErrorDetector) {
        free_ast(program);
        return nullptr;
    }

    return program;
}

ASTNode* parse_statement(Parser* parser) {
    _sbToken* token = peek(parser);
    if (!token) return nullptr;

    // check for structure instance
    if (token->type == _sbKey && check_ahead(parser, 1, "->")) {
        ASTNode* struct_inst = parse_struct_instantiation(parser);
        if (struct_inst) {
            if (!require_semicolon(parser)) {
                free_ast(struct_inst);
                return nullptr;
            }
            return struct_inst;
        }
    }

    // stmt_structure_def
    if (token->type == _sbStruct) {
        return parse_struct_definition(parser);
    }

    // stmt_func_def
    if (token->type == _sbFunction) {
        return parse_function_definition(parser);
    }

    // stmt_load
    if (token->type == _sbLoad) {
        return parse_load_statement(parser);
    }

    // stmt_global
    if (token->type == _sbGlobal) {
        return parse_global_statement(parser);
    }

    // stmt_global
    if (token->type == _sbGoto) {
        return parse_goto_statement(parser);
    }

    // stmt_if
    if (token->type == _sbIf) { // if (condition) { ... }
        return parse_if_else_statement(parser);
    }

    // for ( ... ; ... ; ... ) { ... }
    if (token->type == _sbFor) {
        return parse_for_statement(parser);
    }

    // while ( ... ) { ... }
    if (token->type == _sbWhile) {
        return parse_while_statement(parser);
    }

    // stmt_return
    if (token->type == _sbReturn) { // return ...;
        next(parser); // remove 'return'

        ASTNode* return_stmt = create_ast_node(_sbRETURN, parser);
        if (!match_token(parser, ";") && !match_token(parser, "}")) {
            return_stmt->data.return_stmt.value = parse_expression(parser);
        }

        if (!require_semicolon(parser)) {
            free_ast(return_stmt);
            return nullptr;
        }

        return return_stmt;
    }

    if (token->type == _sbContinue) {
        next(parser);

        ASTNode* continue_stmt = create_ast_node(_sbCONTINUE, parser);

        if (!require_semicolon(parser)) {
            return nullptr;
        }

        return continue_stmt;
    }

    if (token->type == _sbBreak) {
        next(parser);

        ASTNode* break_stmt = create_ast_node(_sbBREAK, parser);

        if (!require_semicolon(parser)) {
            return nullptr;
        }

        return break_stmt;
    }

    // process expression (included function_call)
    ASTNode* stmt = parse_assignment(parser);
    if (stmt) {
        if (stmt->type != _sbGOTO_DEF) {
            if (!require_semicolon(parser)) {
                free_ast(stmt);
                return nullptr;
            }
        }
    }

    return stmt;
}

ASTNode* parse_struct_definition(Parser* parser) {
    next(parser); // remove 'struct'
    
    ASTNode* struct_def = create_ast_node(_sbSTRUCT, parser);
    if (!struct_def) return nullptr;
    
    // parse structure name
    _sbToken* name_token = peek(parser);
    if (name_token && name_token->type == _sbKey) {
        next(parser);
        struct_def->data.struct_def.name = create_ast_node(_sbIDENTIFIER, parser);
        if (struct_def->data.struct_def.name) {
            struct_def->data.struct_def.name->data.str_value = _s_strdup(name_token->tk);
        }
    } else {
        syntaxError(parser, "expected struct name after 'struct'");
        free_ast(struct_def);
        return nullptr;
    }
    
    // parse member list
    if (match_token(parser, "{")) {
        bool enabled_is = false;
        if (incomplete_syntax)
            enabled_is = true;
        incomplete_syntax = true;
        _sbToken* open_brace = peek(parser);
        next(parser); // remove '{'
        
        ASTNode* members = create_ast_node(_sbMEMBER_LIST, parser);
        if (members) {
            members->data.list.items = calloc(1, sizeof(ASTNode*));
            members->data.list.count = 0;
            
            while (!match_token(parser, "}") && peek(parser)) {
                // parse members' identifier
                _sbToken* member_token = peek(parser);
                if (member_token && member_token->type == _sbKey) {
                    next(parser);
                    

                    ASTNode** new_items = realloc(members->data.list.items,
                                                                 (members->data.list.count + 1) * sizeof(ASTNode*));
                    if (new_items) {
                        members->data.list.items = new_items;
                    }
                    else {
                        // realloc failed
                        memoryError(parser, "memory allocation failed");
                        free_ast(members);
                        free_ast(struct_def);
                        return nullptr;
                    }
                    
                    ASTNode* member = create_ast_node(_sbIDENTIFIER, parser);
                    if (member) {
                        member->data.str_value = _s_strdup(member_token->tk);
                        members->data.list.items[members->data.list.count++] = member;
                    }
                    
                    // check for ','  or ';'
                    if (match_token(parser, ",") || match_token(parser, ";")) {
                        next(parser); // remove ',' or ';'
                    }
                    else if (!match_token(parser, "}")) {
                        syntaxError(parser, "expected ',' or ';' or '}' in struct member list");
                        struct_def->data.struct_def.members = members;
                        free_ast(struct_def);
                        return nullptr;
                    }
                }
                else if (!match_token(parser, "}")) {
                    syntaxError(parser, "expected member declaration in struct");
                    free_ast(members);
                    free_ast(struct_def);
                    return nullptr;
                }
            }
        }
        
        struct_def->data.struct_def.members = members;
        
        if (match_token(parser, "}")) {
            next(parser); // remove '}'
        }
        else {
            unclosed_delimiter(parser, "}", open_brace->line, open_brace->pos);
            free_ast(struct_def);
            return nullptr;
        }

        if (!enabled_is)
            incomplete_syntax = false;
    }
    else {
        expect_token(parser, "{");
        free_ast(struct_def);
        return nullptr;
    }
    
    // Selectable: ';' after structer definition
    if (match_token(parser, ";")) {
        next(parser);
    }
    
    return struct_def;
}

ASTNode* parse_struct_instantiation(Parser* parser) {
    // Assuming the identifier has been resolved, check if there is ->
    _sbToken* var_token = peek(parser);
    if (!var_token || var_token->type != _sbKey) {
        return nullptr;
    }
    
    // check if next token is ->
    if (!check_ahead(parser, 1, "->")) {
        // Not a structure instance
        return nullptr;
    }
    
    next(parser); // remove variable name
    
    // check and remove ->
    if (!match_token(parser, "->")) {
        syntaxError(parser, "expected '->' in struct instantiation");
        return nullptr;
    }
    next(parser); // remove '->'
    
    // get structure name
    _sbToken* struct_token = peek(parser);
    if (!struct_token || struct_token->type != _sbKey) {
        syntaxError(parser, "expected struct type name after '->'");
        return nullptr;
    }
    next(parser); // remove structure name
    
    // Create a structure instance node
    ASTNode* struct_inst = create_ast_node(_sbSTRUCT_INSTANTIATION, parser);
    if (!struct_inst) return nullptr;
    
    // Set variable name
    struct_inst->data.struct_inst.variable = create_ast_node(_sbIDENTIFIER, parser);
    if (struct_inst->data.struct_inst.variable) {
        struct_inst->data.struct_inst.variable->data.str_value = _s_strdup(var_token->tk);
    }
    
    // Set structure name
    struct_inst->data.struct_inst.struct_type = create_ast_node(_sbIDENTIFIER, parser);
    if (struct_inst->data.struct_inst.struct_type) {
        struct_inst->data.struct_inst.struct_type->data.str_value = _s_strdup(struct_token->tk);
    }
    
    return struct_inst;
}

ASTNode* parse_function_definition(Parser* parser) {
    next(parser); // remove 'function'

    ASTNode* func_def = create_ast_node(_sbFUNCTION, parser);
    if (!func_def) return nullptr;

    // parse function name
    _sbToken* name_token = peek(parser);
    if (name_token && name_token->type == _sbKey) {
        next(parser);
        func_def->data.function_def.name = create_ast_node(_sbIDENTIFIER, parser);
        if (func_def->data.function_def.name) {
            func_def->data.function_def.name->data.str_value = _s_strdup(name_token->tk);
        }
    }
    else {
        // Lack of function name
        syntaxError(parser, "expected function name after 'function'");
        free_ast(func_def);
        return nullptr;
    }

    // parse parameter list
    if (match_token(parser, "(")) {
        bool enabled_is = false;
        if (incomplete_syntax)
            enabled_is = true;
        incomplete_syntax = true;
        _sbToken* open_paren = peek(parser);
        next(parser); // remove '('

        ASTNode* params = create_ast_node(_sbPARAMETER_LIST, parser);
        if (params){
            params->data.list.items = malloc(sizeof(ASTNode*));
            params->data.list.count = 0;

            while (!match_token(parser, ")") && peek(parser)) {
                _sbToken* param_token = peek(parser);
                if (param_token && param_token->type == _sbKey) {
                    next(parser);

                    ASTNode** new_items = realloc(params->data.list.items,
                                                                 (params->data.list.count + 1) * sizeof(ASTNode*));
                    if (new_items) {
                        params->data.list.items = new_items;
                    }

                    ASTNode* param = create_ast_node(_sbIDENTIFIER, parser);
                    if (param) {
                        param->data.str_value = _s_strdup(param_token->tk);
                        params->data.list.items[params->data.list.count++] = param;
                    }

                    if (match_token(parser, ")")) {
                        next(parser); // remove ')'
                        break;
                    }

                    // check for ','
                    if (match_token(parser, ",")) {
                        next(parser); // remove ','
                    }
                    else {
                        unclosed_delimiter(parser, ")", open_paren->line, open_paren->pos);
                        free_ast(param);
                        free_ast(func_def);
                        return nullptr;
                    }
                }
            }
        }

        func_def->data.function_def.parameters = params;

        if (match_token(parser, ")")) {
            next(parser); // remove ')'
        }

        if (!enabled_is)
            incomplete_syntax = false;
    }
    else {
        // Lack of parameter list
        expect_token(parser, "(");
        free_ast(func_def);
        return nullptr;
    }

    // parse function body
    func_def->data.function_def.body = parse_block(parser);

    // Selectable: ';' after '}'
    if (match_token(parser, ";")) {
        next(parser);
    }

    return func_def;
}

ASTNode* parse_global_statement(Parser* parser) {
    next(parser); // remove 'global'

    ASTNode* global_stmt = create_ast_node(_sbGLOBAL, parser);

    // create variable list storage
    ASTNode* variables = create_ast_node(_sbGLOBAL_LIST, parser);
    variables->data.list.items = malloc(sizeof(ASTNode*));
    variables->data.list.count = 0;

    // parse variable list
    while (peek(parser)) {
        _sbToken* token = peek(parser);

        // check for variables
        if (token->type == _sbKey || token->type == _sbStr) {
            next(parser);
            ASTNode* variable = create_ast_node(_sbIDENTIFIER, parser);
            variable->data.str_value = _s_strdup(token->tk);
            variables->data.list.items = realloc(variables->data.list.items, sizeof(ASTNode*) * (variables->data.list.count + 1));
            variables->data.list.items[variables->data.list.count++] = variable;

            // check for more variables
            if (match_token(parser, ",")) {
                next(parser); // remove ','
                continue;
            }
        }

        _sbToken* token_next = peek_next(parser);

        if (!token_next) break;

        // where to end parser
        if (match_token(parser, ";")) {
            break;
        }
        /*if (match_token(parser, ";") || match_token(parser, "{") ||
            token->type == _sbIf || token->type == _sbFor ||
            token->type == _sbWhile || token->type == _sbReturn ||
            token->type == _sbLoad || token->type == _sbFunction) {
            break;
            }*/

        // invalid token
        require_semicolon(parser);
        free_ast(global_stmt);
        free_ast(variables);
        return nullptr;
    }

    if (variables->data.list.count == 0) {
        syntaxError(parser, "expected at least one module name after 'global'");
        free_ast(global_stmt);
        free_ast(variables);
        return nullptr;
    }

    global_stmt->data.load_stmt.modules = variables;

    // check & remove ';'
    if (!require_semicolon(parser)) {
        free_ast(global_stmt);
        free_ast(variables);
        return nullptr;
    }

    return global_stmt;
}

ASTNode* parse_goto_statement(Parser* parser) {
    next(parser); // remove 'global'

    ASTNode* goto_stmt = create_ast_node(_sbGOTO, parser);

    // create variable list storage

    // parse goto block name
    _sbToken* token = peek(parser);

    if (token->type == _sbKey || token->type == _sbStr) {
        next(parser);
        goto_stmt->data.str_value = _s_strdup(token->tk);
    }
    else {
        syntaxError(parser, "expected a block after 'goto'");
        free_ast(goto_stmt);
        return nullptr;
    }

    // check & remove ';'
    if (!require_semicolon(parser)) {
        free_ast(goto_stmt);
        return nullptr;
    }

    return goto_stmt;
}

ASTNode* parse_load_statement(Parser* parser) {
    next(parser); // remove 'load'
    
    ASTNode* load_stmt = create_ast_node(_sbLOAD, parser);
    
    // create module list storage
    ASTNode* modules = create_ast_node(_sbMODULE_LIST, parser);
    modules->data.list.items = malloc(sizeof(ASTNode*));
    modules->data.list.count = 0;
    
    // parse module list
    while (peek(parser)) {
        _sbToken* token = peek(parser);
        
        // check for modules
        if (token->type == _sbKey || token->type == _sbStr) {
            next(parser);
            ASTNode* module = create_ast_node(_sbIDENTIFIER, parser);
            module->data.str_value = _s_strdup(token->tk);
            modules->data.list.items = realloc(modules->data.list.items, sizeof(ASTNode*) * (modules->data.list.count + 1));
            modules->data.list.items[modules->data.list.count++] = module;
            
            // check for more modules
            if (match_token(parser, ",")) {
                next(parser); // remove ','
                continue;
            }
        }

        _sbToken* token_next = peek_next(parser);

        if (!token_next) break;

        // where to end parser
        if (match_token(parser, ";")) {
            break;
        }
        /*if (match_token(parser, ";") || match_token(parser, "{") ||
            token->type == _sbIf || token->type == _sbFor ||
            token->type == _sbWhile || token->type == _sbReturn ||
            token->type == _sbLoad || token->type == _sbFunction) {
            break;
            }*/
        
        // invalid token
        require_semicolon(parser);
        free_ast(load_stmt);
        free_ast(modules);
        return nullptr;
    }

    if (modules->data.list.count == 0) {
        syntaxError(parser, "expected at least one module name after 'load'");
        free_ast(load_stmt);
        free_ast(modules);
        return nullptr;
    }
    
    load_stmt->data.load_stmt.modules = modules;
    
    // check & remove ';'
    if (!require_semicolon(parser)) {
        free_ast(load_stmt);
        free_ast(modules);
        return nullptr;
    }
    
    return load_stmt;
}

ASTNode* parse_if_else_statement(Parser* parser) {
    next(parser); // remove 'if'

    ASTNode* if_stmt = create_ast_node(_sbIF, parser);

    if (match_token(parser, "(")) {
        bool enabled_is = false;
        if (incomplete_syntax)
            enabled_is = true;
        incomplete_syntax = true;
        _sbToken* open_paren = peek(parser);
        next(parser); // remove'('
        if_stmt->data.if_stmt.condition = parse_expression(parser);
        if (!if_stmt->data.if_stmt.condition) {
            // Lack of condition expression
            syntaxError(parser, "expected condition expression in if statement");
            free_ast(if_stmt);
            return nullptr;
        }
        if (match_token(parser, ")")) {
            next(parser); // remove ')'
        }
        else { // Lack of ')'
            unclosed_delimiter(parser, ")", open_paren->line, open_paren->pos);
            free_ast(if_stmt);
            return nullptr;
        }

        if (!enabled_is)
            incomplete_syntax = false;
    }
    else {
        // Lack of '('
        expect_token(parser, "(");
        free_ast(if_stmt);
        return nullptr;
    }

    if_stmt->data.if_stmt.then_branch = parse_block(parser);
    if (!if_stmt->data.if_stmt.then_branch) {
        // Lack of 'if' body
        syntaxError(parser, "expected statement block after if condition");
        free_ast(if_stmt);
        return nullptr;
    }

    // check for 'else'
    if (peek(parser) && peek(parser)->type == _sbElse) { // if (condition) { ... } else { ... }
        next(parser); // remove 'else'
        if_stmt->data.if_stmt.else_branch = parse_block(parser);
        if (!if_stmt->data.if_stmt.else_branch) {
            // Lack of 'else' body
            syntaxError(parser, "expected statement block after 'else'");
            free_ast(if_stmt);
            return nullptr;
        }
    } else {
        if_stmt->data.if_stmt.else_branch = nullptr;
    }

    return if_stmt;
}

ASTNode* parse_while_statement(Parser* parser) {
    next(parser); // remove 'while'
    
    ASTNode* while_stmt = create_ast_node(_sbWHILE, parser);
    
    if (match_token(parser, "(")) {
        bool enabled_is = false;
        if (incomplete_syntax)
            enabled_is = true;
        incomplete_syntax = true;
        _sbToken* open_paren = peek(parser);
        next(parser); // remove '('
        while_stmt->data.while_stmt.condition = parse_expression(parser);
        if (!while_stmt->data.while_stmt.condition) {
            // Lack of condition
            syntaxError(parser, "expected condition expression in while statement");
            free_ast(while_stmt);
            return nullptr;
        }

        if (match_token(parser, ")")) {
            next(parser); // remove ')'
        }
        else {
            unclosed_delimiter(parser, ")", open_paren->line, open_paren->pos);
            free_ast(while_stmt);
            return nullptr;
        }

        if (!enabled_is)
            incomplete_syntax = false;
    }
    
    // body
    while_stmt->data.while_stmt.body = parse_block(parser);
    if (!while_stmt->data.while_stmt.body) {
        // Lack of 'while' body
        syntaxError(parser, "expected loop body");
        free_ast(while_stmt);
        return nullptr;
    }
    
    return while_stmt;
}

ASTNode* parse_for_statement(Parser* parser) {
    next(parser); // remove 'for'
    
    ASTNode* for_stmt = create_ast_node(_sbFOR, parser);
    if (!for_stmt) return nullptr;
    
    if (match_token(parser, "(")) {
        bool enabled_is = false;
        if (incomplete_syntax)
            enabled_is = true;
        incomplete_syntax = true;
        _sbToken* open_paren = peek(parser);
        next(parser); // remove '('
        
        // initialization (segment A)
        if (!match_token(parser, ";")) {
            for_stmt->data.for_stmt.init = parse_assignment(parser);
        } else {
            expect_token(parser, ";");
            free_ast(for_stmt);
            return nullptr;
        }
        
        if (match_token(parser, ";")) {
            next(parser); // remove first ';'
        }
        
        // condition (segment B)
        if (!match_token(parser, ";")) {
            for_stmt->data.for_stmt.condition = parse_expression(parser);
        } else {
            expect_token(parser, ";");
            free_ast(for_stmt);
            return nullptr;
        }
        
        if (match_token(parser, ";")) {
            next(parser); // remove second ';'
        }
        
        // update (segment C)
        if (!match_token(parser, ")")) {
            for_stmt->data.for_stmt.update = parse_assignment(parser);
        } else {
            for_stmt->data.for_stmt.update = nullptr;
        }
        
        if (match_token(parser, ")")) {
            next(parser); // remove ')'
        }
        else {
            unclosed_delimiter(parser, ")", open_paren->line, open_paren->pos);
            free_ast(for_stmt);
            return nullptr;
        }

        if (!enabled_is)
            incomplete_syntax = false;
    }
    else {
        // Lack of '('
        expect_token(parser, "(");
        free_ast(for_stmt);
        return nullptr;
    }
    
    // body
    for_stmt->data.for_stmt.body = parse_block(parser);
    if (!for_stmt->data.for_stmt.body) {
        // Lack of 'for' body
        syntaxError(parser, "expected loop body");
        free_ast(for_stmt);
        return nullptr;
    }
    
    return for_stmt;
}

ASTNode* parse_assignment(Parser* parser) {
    int saved_pos = parser->position;

    ASTNode* left = parse_postfix(parser);

    if (left && match_token(parser, "=") && !match_token(parser, "==")) {
        next(parser); // remove '='

        ASTNode* right = parse_expression(parser);

        if (!right) {
            // Lack of expression of right: x = <lacked>
            syntaxError(parser, "expected expression after '='");
            free_ast(left);
            return nullptr;
        }

        ASTNode* assignment = create_ast_node(_sbASSIGNMENT, parser);
        if (assignment) {
            assignment->data.assignment.left = left;
            assignment->data.assignment.right = right;
        }

        return assignment;
    }

    if (left) free_ast(left);
    else return nullptr;

    parser->position = saved_pos;
    return parse_expression(parser);
}

// Statement block
ASTNode* parse_block(Parser* parser) {
    bool enabled_is = false;
    if (incomplete_syntax)
        enabled_is = true;
    incomplete_syntax = true;
    bool multiline = true;
    if (!match_token(parser, "{"))
        multiline = false;
      //return nullptr;

    _sbToken* open_brace = peek(parser);
    if (multiline)
      next(parser); // remove '{'

    ASTNode* block = create_ast_node(_sbBLOCK, parser);
    block->data.list.items = malloc(sizeof(ASTNode*));
    block->data.list.count = 0;
    if (multiline) {
        while (!match_token(parser, "}") && peek(parser)) {
            ASTNode* stmt = parse_statement(parser);
            if (stmt) {
                block->data.list.items = realloc(block->data.list.items, (block->data.list.count + 1) * sizeof(ASTNode*));
                block->data.list.items[block->data.list.count++] = stmt;
            }
            else {
                block->data.list.items = realloc(block->data.list.items, (block->data.list.count + 1) * sizeof(ASTNode*));
                block->data.list.items[block->data.list.count++] = create_ast_node(_sbNOTHING, parser);
            }
        }

        if (match_token(parser, "}")) {
            next(parser); // remove '}'
        }
        else {
            unclosed_delimiter(parser, "}", open_brace->line, open_brace->pos);
            free_ast(block);
            return nullptr;
        }
    }
    else {
        ASTNode* stmt = parse_statement(parser);
        if (stmt) {
            block->data.list.items = realloc(block->data.list.items, (block->data.list.count + 1) * sizeof(ASTNode*));
            block->data.list.items[block->data.list.count++] = stmt;
        }
        else {
            block->data.list.items = realloc(block->data.list.items, (block->data.list.count + 1) * sizeof(ASTNode*));
            block->data.list.items[block->data.list.count++] = create_ast_node(_sbNOTHING, parser);
        }
    }

    if (!enabled_is)
        incomplete_syntax = false;
    return block;
}

ASTNode* parse_expression(Parser* parser) {
    return parse_logical(parser);
}

ASTNode* parse_logical(Parser* parser) { // '&&' '||'
    ASTNode* left = parse_bitwise_or(parser);

    while (peek(parser) && (match_token(parser, "&&") || match_token(parser, "||"))) {
        _sbToken* op = next(parser);
        ASTNode* right = parse_bitwise_or(parser);

        if (!right) {
            syntaxError(parser, "expected expression after '%s'", op->tk);
            free_ast(left);
            free_ast(right);
            return nullptr;
        }

        ASTNode* node = create_ast_node(_sbBINARY_LITERAL, parser);
        node->data.binary_op.op = _s_strdup(op->tk);
        node->data.binary_op.left = left;
        node->data.binary_op.right = right;
        left = node;
    }

    return left;
}

ASTNode* parse_comparison(Parser* parser) {
    ASTNode* left = parse_shift(parser);
    
    while (peek(parser) && (match_token(parser, "==") || match_token(parser, "!=") || 
                           match_token(parser, "<") || match_token(parser, ">") ||
                           match_token(parser, "<=") || match_token(parser, ">=") ||
                           match_token(parser, "<>") || match_token(parser, "><") )) {
        _sbToken* op = next(parser);
        ASTNode* right = parse_shift(parser);

        if (!right) {
            syntaxError(parser, "expected expression after '%s'", op->tk);
            free_ast(left);
            free_ast(right);
            return nullptr;
        }
        
        ASTNode* node = create_ast_node(_sbBINARY_LITERAL, parser);
        node->data.binary_op.op = _s_strdup(op->tk);
        node->data.binary_op.left = left;
        node->data.binary_op.right = right;
        left = node;
    }
    
    return left;
}

// add / sub
ASTNode* parse_additive(Parser* parser) {
    ASTNode* left = parse_multiplicative(parser);
    
    while (peek(parser) && (match_token(parser, "+") || match_token(parser, "-"))) {
        _sbToken* op = next(parser);
        ASTNode* right = parse_multiplicative(parser);

        if (!right) {
            syntaxError(parser, "expected expression after '%s'", op->tk);
            free_ast(left);
            free_ast(right);
            return nullptr;
        }
        
        ASTNode* node = create_ast_node_with_token(_sbBINARY_LITERAL, op);
        node->data.binary_op.op = _s_strdup(op->tk);
        node->data.binary_op.left = left;
        node->data.binary_op.right = right;
        left = node;
    }
    
    return left;
}

// power
ASTNode* parse_power(Parser* parser) {
    ASTNode* left = parse_postfix(parser);

    if (peek(parser) && match_token(parser, "**")) {
        _sbToken* op = next(parser);
        ASTNode* right = parse_power(parser); // combine -> right

        if (!right) {
            syntaxError(parser, "expected expression after '**'");
            free_ast(left);
            free_ast(right);
            return nullptr;
        }

        ASTNode* node = create_ast_node(_sbBINARY_LITERAL, parser);
        if (node) {
            node->data.binary_op.op = _s_strdup(op->tk);
            node->data.binary_op.left = left;
            node->data.binary_op.right = right;
        }
        return node;
    }

    return left;
}

// times / divide
ASTNode* parse_multiplicative(Parser* parser) {
    ASTNode* left = parse_power(parser);
    
    while (peek(parser) && (match_token(parser, "*") || match_token(parser, "/") || match_token(parser, "%"))) {
        _sbToken* op = next(parser);
        ASTNode* right = parse_power(parser);

        if (!right) {
            syntaxError(parser, "expected expression after '%s'", op->tk);
            free_ast(left);
            free_ast(right);
            return nullptr;
        }
        
        ASTNode* node = create_ast_node_with_token(_sbBINARY_LITERAL, op);
        node->data.binary_op.op = _s_strdup(op->tk);
        node->data.binary_op.left = left;
        node->data.binary_op.right = right;
        left = node;
    }
    
    return left;
}

// Bit shift operations
ASTNode* parse_shift(Parser* parser) {
    ASTNode* left = parse_additive(parser);

    while (peek(parser) && (match_token(parser, "<<") || match_token(parser, ">>"))) {
        _sbToken* op = next(parser);
        ASTNode* right = parse_additive(parser);

        if (!right) {
            syntaxError(parser, "expected expression after '%s'", op->tk);
            free_ast(left);
            free_ast(right);
            return nullptr;
        }

        ASTNode* node = create_ast_node(_sbBINARY_LITERAL, parser);
        if (node) {
            node->data.binary_op.op = _s_strdup(op->tk);
            node->data.binary_op.left = left;
            node->data.binary_op.right = right;
            left = node;
        }
    }

    return left;
}

// Bitwise AND operation
ASTNode* parse_bitwise_and(Parser* parser) {
    ASTNode* left = parse_bitwise_xor(parser);

    // &: bitwise and, &&: logical and
    while (peek(parser) && match_token(parser, "&") && !check_ahead(parser, 1, "&")) {
        _sbToken* op = next(parser);
        ASTNode* right = parse_comparison(parser);

        if (!right) {
            syntaxError(parser, "expected expression after '&'");
            free_ast(left);
            free_ast(right);
            return nullptr;
        }

        ASTNode* node = create_ast_node(_sbBINARY_LITERAL, parser);
        if (node) {
            node->data.binary_op.op = _s_strdup(op->tk);
            node->data.binary_op.left = left;
            node->data.binary_op.right = right;
            left = node;
        }
    }

    return left;
}

// Bitwise OR operation
ASTNode* parse_bitwise_or(Parser* parser) {
    ASTNode* left = parse_bitwise_and(parser);

    // |: bitwise or, ||: logical or
    while (peek(parser) && match_token(parser, "|") && !check_ahead(parser, 1, "|")) {
        _sbToken* op = next(parser);
        ASTNode* right = parse_bitwise_and(parser);

        if (!right) {
            syntaxError(parser, "expected expression after '|'");
            free_ast(left);
            free_ast(right);
            return nullptr;
        }

        ASTNode* node = create_ast_node(_sbBINARY_LITERAL, parser);
        if (node) {
            node->data.binary_op.op = _s_strdup(op->tk);
            node->data.binary_op.left = left;
            node->data.binary_op.right = right;
            left = node;
        }
    }

    return left;
}

// Bitwise XOR operation
ASTNode* parse_bitwise_xor(Parser* parser) {
    ASTNode* left = parse_comparison(parser);

    while (peek(parser) && match_token(parser, "^")) {
        _sbToken* op = next(parser);
        ASTNode* right = parse_bitwise_and(parser);

        if (!right) {
            syntaxError(parser, "expected expression after '^'");
            free_ast(left);
            free_ast(right);
            return nullptr;
        }

        ASTNode* node = create_ast_node(_sbBINARY_LITERAL, parser);
        if (node) {
            node->data.binary_op.op = _s_strdup(op->tk);
            node->data.binary_op.left = left;
            node->data.binary_op.right = right;
            left = node;
        }
    }

    return left;
}

ASTNode* parse_postfix(Parser* parser) {
    ASTNode* node = parse_primary(parser);

    while (node && peek(parser)) {
        // structure member call
        if (match_token(parser, ".")) {
            next(parser); // remove '.'
            
            _sbToken* member_token = peek(parser);
            if (!member_token || member_token->type != _sbKey) {
                syntaxError(parser, "expected member name after '.'");
                free_ast(node);
                return nullptr;
            }
            
            next(parser); // remove member identifier
            
            ASTNode* member_access = create_ast_node(_sbMEMBER_ACCESS, parser);
            if (!member_access) break;
            
            member_access->data.member_access.object = node;
            member_access->data.member_access.member = create_ast_node(_sbIDENTIFIER, parser);
            if (member_access->data.member_access.member) {
                member_access->data.member_access.member->data.str_value = _s_strdup(member_token->tk);
            }
            
            node = member_access;
        }
        // function_call
        else if (match_token(parser, "(")) {
            bool enabled_is = false;
            if (incomplete_syntax)
                enabled_is = true;
            incomplete_syntax = true;
            _sbToken* open_paren = peek(parser);
            next(parser);
            
            ASTNode* func_call = create_ast_node(_sbFUNCTION_CALL, parser);
            if (!func_call) break;

            if (node->type != _sbIDENTIFIER) {
                syntaxError(parser, "function name must be an identifier");
                free_ast(node);
                free_ast(func_call);
                return nullptr;
            }
            
            func_call->data.function_call.function_name = node;
            
            ASTNode* args = create_ast_node(_sbARGUMENTS, parser);
            if (args) {
                args->data.list.items = calloc(1, sizeof(ASTNode*));
                args->data.list.count = 0;
                
                while (!match_token(parser, ")") && peek(parser)) {
                    if (args->data.list.count > 0) {
                        if (match_token(parser, ",")) {
                            next(parser);
                        } else if (!match_token(parser, ")")) {
                            syntaxError(parser, "expected ',' or ')' in argument list");
                            free_ast(args);
                            free_ast(func_call);  // This will free node as well
                            return nullptr;
                        }
                    }
                    

                    ASTNode** new_items = (ASTNode**)realloc(args->data.list.items,
                                                                 (args->data.list.count + 1) * sizeof(ASTNode*));
                    if (new_items) {
                        args->data.list.items = new_items;
                    }
                    
                    ASTNode* arg = parse_expression(parser);
                    if (arg) {
                        args->data.list.items[args->data.list.count++] = arg;
                    } else {
                        syntaxError(parser, "invalid argument expression");
                        free_ast(args);
                        free_ast(func_call);
                        //free_ast(node);
                        return nullptr;
                    }
                }
            }
            
            if (match_token(parser, ")")) {
                next(parser);
            } else {
                unclosed_delimiter(parser, ")", open_paren->line, open_paren->pos);
                free_ast(args);
                free_ast(func_call);  // This will free node as well
                return nullptr;
            }

            if (!enabled_is)
                incomplete_syntax = false;
            
            func_call->data.function_call.arguments = args;
            node = func_call;
        }
        // process for accessing list
        else if (match_token(parser, "[")) {
            bool enabled_is = false;
            if (incomplete_syntax)
                enabled_is = true;
            incomplete_syntax = true;
            _sbToken* open_bracket = peek(parser);
            next(parser); // remove '['

            ASTNode* list_access = create_ast_node(_sbLIST_ACCESS, parser);
            if (!list_access) break;

            list_access->data.list_access.list = node;
            list_access->data.list_access.index = parse_expression(parser);

            if (!list_access->data.list_access.index) {
                // Lack of index syntax: x<lacked>
                syntaxError(parser, "expected index expression");
                free_ast(list_access);  // This will free node as well
                return nullptr;
            }

            if (match_token(parser, "]")) {
                next(parser); // remove ']'
            }
            else {
                unclosed_delimiter(parser, "]", open_bracket->line, open_bracket->pos);
                free_ast(list_access);  // This will free node as well
                return nullptr;
            }

            if (!enabled_is)
                incomplete_syntax = false;

            node = list_access;
        }
        else {
            break;
        }
    }

    return node;
}

// Normal expressions: string, number, identifier, brackets, list
ASTNode* parse_primary(Parser* parser) {
    _sbToken* token = peek(parser);
    if (!token) return nullptr;

    // List expressions
    if (match_token(parser, "[")) {
        return parse_list(parser);
    }
    
    // Bracket expressions
    if (match_token(parser, "(")) {
        next(parser); // remove '('
        ASTNode* expr = parse_expression(parser);
        if (match_token(parser, ")")) {
            next(parser); // remove ')'
        }
        return expr;
    }

    // Unary '!': !...
    if (match_token(parser, "!")) {
        next(parser);
        ASTNode* node = create_ast_node(_sbUNARY_LITERAL, parser);
        node->data.unary_op.op = _s_strdup("!");
        node->data.unary_op.operand = parse_postfix(parser);
        return node;
    }

    // Unary '~': ~...
    if (match_token(parser, "~")) { // bitwise_not
        next(parser);
        ASTNode* node = create_ast_node(_sbUNARY_LITERAL, parser);
        node->data.unary_op.op = _s_strdup("~");
        node->data.unary_op.operand = parse_postfix(parser);
        return node;
    }
    
    // Unary '-': -xxx
    if (match_token(parser, "-")) {
        next(parser);
        ASTNode* node = create_ast_node(_sbUNARY_LITERAL, parser);
        node->data.unary_op.op = _s_strdup("-");
        node->data.unary_op.operand = parse_primary(parser);
        return node;
    }

    // Unary '+': +xxx
    if (match_token(parser, "+")) {
        next(parser);
        ASTNode* node = create_ast_node(_sbUNARY_LITERAL, parser);
        node->data.unary_op.op = _s_strdup("+");
        node->data.unary_op.operand = parse_primary(parser);
        return node;
    }
    
    // Number
    if (token->type == _sbNum) {
        next(parser);
        ASTNode* node = create_ast_node(_sbNUMBER_LITERAL, parser);
        node->data.num_value = atof(token->tk);
        return node;
    }
    
    // String
    if (token->type == _sbStr) {
        next(parser);
        ASTNode* node = create_ast_node(_sbSTRING_LITERAL, parser);
        node->data.str_value = _s_strdup(token->tk);
        return node;
    }
    
    // identifier
    if (token->type == _sbKey) {
        next(parser);
        ASTNode* node;

        if (match_token(parser, ":")) {
            next(parser); // goto definition

            node = create_ast_node(_sbGOTO_DEF, parser);
            node->data.str_value = _s_strdup(token->tk);
            return node;
        } // Not a goto definition

        node = create_ast_node(_sbIDENTIFIER, parser);
        node->data.str_value = _s_strdup(token->tk);
        return node;
    }
    
    return create_ast_node(_sbNOTHING, parser);
}

ASTNode* parse_list(Parser* parser) {
    bool enabled_is = false;
    if (incomplete_syntax)
        enabled_is = true;
    incomplete_syntax = true;
    _sbToken* open_bracket = peek(parser);
    next(parser); // remove '['
    
    ASTNode* list = create_ast_node(_sbLIST_LITERAL, parser);
    if (!list) return nullptr;

    list->data.list.items = malloc(sizeof(ASTNode*));
    list->data.list.count = 0;
    
    // parse items
    while (!match_token(parser, "]") && peek(parser)) {
        if (list->data.list.count > 0) {
            if (match_token(parser, ",")) {
                next(parser); // remove ','
            }
            else if (!match_token(parser, "]")) {
                // require ';'
                syntaxError(parser, "expected ',' or ']' in list");
                free_ast(list);
            }
        }
        

        ASTNode** new_items = realloc(list->data.list.items,
                                                     (list->data.list.count + 1) * sizeof(ASTNode*));
        if (new_items)
          list->data.list.items = new_items;
        
        ASTNode* item = parse_expression(parser);
        if (item) {
            list->data.list.items[list->data.list.count++] = item;
        }
    }
    
    if (match_token(parser, "]")) {
        next(parser); // remove ']'
    }
    else {
        unclosed_delimiter(parser, "]", open_bracket->line, open_bracket->column);
        free_ast(list);
        return nullptr;
    }

    if (!enabled_is)
        incomplete_syntax = false;
    
    return list;
}

void free_ast(ASTNode* node) {
    if (!node) return;
    
    switch (node->type) {
        case _sbBINARY_LITERAL:
            if (node->data.binary_op.op) {
                free(node->data.binary_op.op);
                node->data.binary_op.op = nullptr;
            }
            free_ast(node->data.binary_op.left);
            free_ast(node->data.binary_op.right);
            break;
        case _sbUNARY_LITERAL:
            if (node->data.unary_op.op) {
                free(node->data.unary_op.op);
                node->data.unary_op.op = nullptr;
            }
            free_ast(node->data.unary_op.operand);
            break;
        case _sbFUNCTION_CALL:
            free_ast(node->data.function_call.function_name);
            free_ast(node->data.function_call.arguments);
            break;
        case _sbFUNCTION:
            free_ast(node->data.function_def.name);
            free_ast(node->data.function_def.parameters);
            free_ast(node->data.function_def.body);
            break;
        case _sbSTRUCT:
            free_ast(node->data.struct_def.name);
            free_ast(node->data.struct_def.members);
            break;
        case _sbMEMBER_ACCESS:
            free_ast(node->data.member_access.object);
            free_ast(node->data.member_access.member);
            break;
        case _sbSTRUCT_INSTANTIATION:
            free_ast(node->data.struct_inst.variable);
            free_ast(node->data.struct_inst.struct_type);
            break;
        case _sbLIST_ACCESS:
            free_ast(node->data.list_access.list);
            free_ast(node->data.list_access.index);
            break;
        case _sbIF:
            free_ast(node->data.if_stmt.condition);
            free_ast(node->data.if_stmt.then_branch);
            free_ast(node->data.if_stmt.else_branch);
            break;
        case _sbWHILE:
            free_ast(node->data.while_stmt.condition);
            free_ast(node->data.while_stmt.body);
            break;
        case _sbFOR:
            free_ast(node->data.for_stmt.init);
            free_ast(node->data.for_stmt.condition);
            free_ast(node->data.for_stmt.update);
            free_ast(node->data.for_stmt.body);
            break;
        case _sbASSIGNMENT:
            free_ast(node->data.assignment.left);
            free_ast(node->data.assignment.right);
            break;
        case _sbRETURN:
            free_ast(node->data.return_stmt.value);
            break;
        case _sbLOAD:
            free_ast(node->data.load_stmt.modules);
            break;
        case _sbGLOBAL:
            free_ast(node->data.global_stmt.variables);
            break;
        case _sbSTRING_LITERAL:
        case _sbIDENTIFIER:
        case _sbGOTO_DEF:
        case _sbGOTO:
            if (node->data.str_value) {
                free(node->data.str_value);
                node->data.str_value = nullptr;
            }
            break;
        case _sbLIST_LITERAL:
        case _sbSTATEMENT:
        case _sbARGUMENTS:
        case _sbPARAMETER_LIST:
        case _sbMODULE_LIST:
        case _sbMEMBER_LIST:
        case _sbGLOBAL_LIST:
        case _sbBLOCK:
        case _sbPROGRAM:
            if (node->data.list.items) {
                for (int i = 0; i < node->data.list.count; i++) {
                    free_ast(node->data.list.items[i]);
                }
                free(node->data.list.items);
                node->data.list.items = nullptr;
            }
            break;
        case _sbNUMBER_LITERAL:
            break;

        default:
            break;
    }
    free(node);
}

#ifdef _SBL_AST_TEST

#include <mcheck.h>

void print_ast(ASTNode* node, int indent) {
    if (!node) return;
    
    for (int i = 0; i < indent; i++) printf("  ");
    
    switch (node->type) {
        case _sbPROGRAM:
            printf("PROGRAM:\n");
            for (int i = 0; i < node->data.list.count; i++) {
                print_ast(node->data.list.items[i], indent + 1);
            }
            break;
        case _sbNUMBER_LITERAL:
            printf("NUMBER: %lf\n", node->data.num_value);
            break;
        case _sbSTRING_LITERAL:
            printf("STRING: %s\n", node->data.str_value);
            break;
        case _sbIDENTIFIER:
            printf("IDENTIFIER: %s\n", node->data.str_value);
            break;
        case _sbBINARY_LITERAL:
            printf("BINARY_OP: %s\n", node->data.binary_op.op);
            print_ast(node->data.binary_op.left, indent + 1);
            print_ast(node->data.binary_op.right, indent + 1);
            break;
        case _sbUNARY_LITERAL:
            printf("UNARY_OP: %s\n", node->data.unary_op.op);
            print_ast(node->data.unary_op.operand, indent + 1);
            break;
        case _sbFUNCTION_CALL:
            printf("FUNCTION_CALL:\n");
            for (int i = 0; i < indent + 1; i++) printf("  ");
            printf("FUNCTION_NAME:\n");
            print_ast(node->data.function_call.function_name, indent + 2);
            for (int i = 0; i < indent + 1; i++) printf("  ");
            printf("ARGUMENTS:\n");
            if (node->data.function_call.arguments) {
                for (int i = 0; i < node->data.function_call.arguments->data.list.count; i++) {
                    print_ast(node->data.function_call.arguments->data.list.items[i], indent + 2);
                }
            }
            break;
        case _sbSTRUCT:
            printf("STRUCT_DEF:\n");
            for (int i = 0; i < indent + 1; i++) printf("  ");
            printf("NAME:\n");
            print_ast(node->data.struct_def.name, indent + 2);
            for (int i = 0; i < indent + 1; i++) printf("  ");
            printf("MEMBERS:\n");
            if (node->data.struct_def.members) {
                for (int i = 0; i < node->data.struct_def.members->data.list.count; i++) {
                    print_ast(node->data.struct_def.members->data.list.items[i], indent + 2);
                }
            }
            break;
        case _sbMEMBER_ACCESS:
            printf("MEMBER_ACCESS:\n");
            for (int i = 0; i < indent + 1; i++) printf("  ");
            printf("OBJECT:\n");
            print_ast(node->data.member_access.object, indent + 2);
            for (int i = 0; i < indent + 1; i++) printf("  ");
            printf("MEMBER:\n");
            print_ast(node->data.member_access.member, indent + 2);
            break;
        case _sbSTRUCT_INSTANTIATION:
            printf("STRUCT_INSTANTIATION:\n");
            for (int i = 0; i < indent + 1; i++) printf("  ");
            printf("VARIABLE:\n");
            print_ast(node->data.struct_inst.variable, indent + 2);
            for (int i = 0; i < indent + 1; i++) printf("  ");
            printf("STRUCT_TYPE:\n");
            print_ast(node->data.struct_inst.struct_type, indent + 2);
            break;
        case _sbLIST_LITERAL:
            printf("LIST:\n");
            for (int i = 0; i < node->data.list.count; i++) {
                print_ast(node->data.list.items[i], indent + 1);
            }
            break;
        case _sbFUNCTION:
            printf("FUNCTION_DEF:\n");
            for (int i = 0; i < indent + 1; i++) printf("  ");
            printf("NAME:\n");
            print_ast(node->data.function_def.name, indent + 2);
            for (int i = 0; i < indent + 1; i++) printf("  ");
            printf("PARAMETERS:\n");
            if (node->data.function_def.parameters) {
                for (int i = 0; i < node->data.function_def.parameters->data.list.count; i++) {
                    print_ast(node->data.function_def.parameters->data.list.items[i], indent + 2);
                }
            }
            for (int i = 0; i < indent + 1; i++) printf("  ");
            printf("BODY:\n");
            print_ast(node->data.function_def.body, indent + 2);
            break;
        case _sbLIST_ACCESS:
            printf("LIST_ACCESS:\n");
            for (int i = 0; i < indent + 1; i++) printf("  ");
            printf("LIST:\n");
            print_ast(node->data.list_access.list, indent + 2);
            for (int i = 0; i < indent + 1; i++) printf("  ");
            printf("INDEX:\n");
            print_ast(node->data.list_access.index, indent + 2);
            break;
        case _sbIF:
            printf("IF_STATEMENT:\n");
            for (int i = 0; i < indent + 1; i++) printf("  ");
            printf("CONDITION:\n");
            print_ast(node->data.if_stmt.condition, indent + 2);
            for (int i = 0; i < indent + 1; i++) printf("  ");
            printf("THEN:\n");
            print_ast(node->data.if_stmt.then_branch, indent + 2);
            if (node->data.if_stmt.else_branch) {
                for (int i = 0; i < indent + 1; i++) printf("  ");
                printf("ELSE:\n");
                print_ast(node->data.if_stmt.else_branch, indent + 2);
            }
            break;
        case _sbASSIGNMENT:
            printf("ASSIGNMENT:\n");
            for (int i = 0; i < indent + 1; i++) printf("  ");
            printf("LEFT:\n");
            print_ast(node->data.assignment.left, indent + 2);
            for (int i = 0; i < indent + 1; i++) printf("  ");
            printf("RIGHT:\n");
            print_ast(node->data.assignment.right, indent + 2);
            break;
        case _sbFOR:
            printf("FOR_STATEMENT:\n");
            for (int i = 0; i < indent + 1; i++) printf("  ");
            printf("INIT:\n");
            if (node->data.for_stmt.init) {
                print_ast(node->data.for_stmt.init, indent + 2);
            } else {
                for (int i = 0; i < indent + 2; i++) printf("  ");
                printf("(empty)\n");
            }
            for (int i = 0; i < indent + 1; i++) printf("  ");
            printf("CONDITION:\n");
            if (node->data.for_stmt.condition) {
                print_ast(node->data.for_stmt.condition, indent + 2);
            } else {
                for (int i = 0; i < indent + 2; i++) printf("  ");
                printf("(empty)\n");
            }
            for (int i = 0; i < indent + 1; i++) printf("  ");
            printf("UPDATE:\n");
            if (node->data.for_stmt.update) {
                print_ast(node->data.for_stmt.update, indent + 2);
            } else {
                for (int i = 0; i < indent + 2; i++) printf("  ");
                printf("(empty)\n");
            }
            for (int i = 0; i < indent + 1; i++) printf("  ");
            printf("BODY:\n");
            print_ast(node->data.for_stmt.body, indent + 2);
            break;
        case _sbWHILE:
            printf("WHILE_STATEMENT:\n");
            for (int i = 0; i < indent + 1; i++) printf("  ");
            printf("CONDITION:\n");
            print_ast(node->data.while_stmt.condition, indent + 2);
            for (int i = 0; i < indent + 1; i++) printf("  ");
            printf("BODY:\n");
            print_ast(node->data.while_stmt.body, indent + 2);
            break;
        case _sbRETURN:
            printf("RETURN:\n");
            print_ast(node->data.return_stmt.value, indent + 1);
            break;
        case _sbLOAD:
            printf("LOAD_STATEMENT:\n");
            for (int i = 0; i < indent + 1; i++) printf("  ");
            printf("MODULES:\n");
            if (node->data.load_stmt.modules) {
                for (int i = 0; i < node->data.load_stmt.modules->data.list.count; i++) {
                    print_ast(node->data.load_stmt.modules->data.list.items[i], indent + 2);
                }
            }
            break;
        case _sbGLOBAL:
            printf("GLOBAL_STATEMENT:\n");
            for (int i = 0; i < indent + 1; i++) printf("  ");
            printf("GLOBALS:\n");
            if (node->data.global_stmt.variables) {
                for (int i = 0; i < node->data.global_stmt.variables->data.list.count; i++) {
                    print_ast(node->data.global_stmt.variables->data.list.items[i], indent + 2);
                }
            }
            break;
        case _sbBLOCK:
            printf("BLOCK:\n");
            for (int i = 0; i < node->data.list.count; i++) {
                print_ast(node->data.list.items[i], indent + 1);
            }
            break;
        case _sbNOTHING:
            printf("NOTHING\n");
        case _sbCONTINUE:
            printf("CONTINUE\n");
        case _sbBREAK:
            printf("BREAK\n");
        default:
            printf("UNKNOWN NODE TYPE\n");
    }
}

#ifdef _SBL_PARSER_TEST_MAIN
int main() {
    setenv("MALLOC_TRACE", "output", 1);
    mtrace();
    const char* _s = "load module1, module2;\n"
                     "function a(argument1, argument2){\n"
                     "    variable1 = argument1;\n"
                     "    while(variable1 <= argument2) {\n"
                     "        if(!(argument2 % argument1 == 2)) {\n"
                     "            variable1 = variable1 + 2;\n"
                     "        }\n"
                     "        else {\n"
                     "            variable1 = variable1 + 3;\n"
                     "        }\n"
                     "    }\n"
                     "    for(i = 0; i < 10; i = i + 1){\n"
                     "        variable1 = variable1 + i;\n"
                     "    }\n"
                     "}\n"
                     "struct Structure1{\n"
                     "    member1, member2,\n"
                     "    member3, member4\n"
                     "}\n"
                     "variable2 -> Structure1;\n"
                     "variable2.member1 = 2;\n"
                     "b = !a(1, 2);\n"
                     "c = ~(25 ^ (12 ** 5));\n"
                     "global c;";
    _sbToken* _t_tk = _sbLexer(_s);

    _sbTkState* _t_st = create_tkstate(_t_tk);
    _sbToken* _t_st_s = _t_st->tk;

    _sbToken t;
    int i = 0;
    while((t = *(_t_st->tk++)).tk != nullptr){
        fprintf(stdout, "[%d]: {token: %s, line: %d, column: %d, position: %d, type: %d}\n", i++, t.tk, t.line, t.column, t.pos, t.type);
    }

    _t_st->tk = _t_st_s;

    printf("=== Parsing tokens ===\n");
    ASTNode* ast = parse_program(_t_st);

    printf("\n=== Generated AST ===\n");
    print_ast(ast, 0);

    printf("\n=== Freeing AST ===\n");
    free_ast(ast);
    printf("AST freed successfully!\n");

    _t_st->tk = _t_st_s;

    destroy_tkstate(_t_st);
    freeTkList(_t_tk);

}

#endif
#endif