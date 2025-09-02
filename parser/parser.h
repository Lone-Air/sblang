/*
 * SB - Language
 * By Laman28
 * Compiler - AST - Construction
 * Not welcome to use /XD
 */

#ifndef _SBL_AST
#define _SBL_AST

#include "../lexer/lexer.h"

typedef enum _sbNType{
    _sbPROGRAM, _sbSTATEMENT_LIST, _sbSTATEMENT, _sbBLOCK,
    _sbIF, _sbRETURN, _sbWHILE, _sbFOR, _sbLOAD, _sbFUNCTION, _sbSTRUCT, _sbMEMBER_ACCESS, _sbSTRUCT_INSTANTIATION, _sbGLOBAL,
    _sbFUNCTION_CALL, _sbIDENTIFIER, _sbARGUMENTS, _sbMODULE_LIST, _sbPARAMETER_LIST, _sbMEMBER_LIST, _sbGLOBAL_LIST,
    _sbNUMBER_LITERAL, _sbSTRING_LITERAL, _sbLIST_LITERAL,
    _sbBINARY_LITERAL, _sbUNARY_LITERAL, _sbNOTHING, _sbCONTINUE, _sbBREAK,
    _sbASSIGNMENT, _sbLIST_ACCESS
}_sbNType;

typedef struct ASTNode {
    _sbNType type;
    int source_line;        /* Source line number */
    int source_column;      /* Source column number */
    union {
        double num_value;
        char* str_value;
        struct { // <left> <op> <right>
            char* op;
            struct ASTNode* left;
            struct ASTNode* right;
        } binary_op;
        struct { // <op> <operand>
            char* op;
            struct ASTNode* operand;
        } unary_op;
        struct { // <function_name>(<argument>, ...)
            struct ASTNode* function_name;
            struct ASTNode* arguments;
        } function_call;
        struct {
            struct ASTNode* name; // function <name>( ... ){ ... }
            struct ASTNode* parameters; // function name(<parameter>, ...){ ... }
            struct ASTNode* body; // function name( ... ) { <body> }
        } function_def;
        struct {
            struct ASTNode* name;     // struct <name> { ... }
            struct ASTNode* members;  // struct name { <member>, ... }
        } struct_def;
        struct {
            struct ASTNode* variable;    // <variable> = struct_type
            struct ASTNode* struct_type; // variable = <struct_type>
        } struct_inst;
        struct { // <object>.<member>
            struct ASTNode* object;
            struct ASTNode* member;
        } member_access;
        struct {
            struct ASTNode* list; // <list>[x]
            struct ASTNode* index; // list[<index>]
        } list_access;
        struct { // if (<condition>) { <then_branch> }
            struct ASTNode* condition;
            struct ASTNode* then_branch;
            struct ASTNode* else_branch; // if (<condition>) { <then> }  else { <else_branch> }
        } if_stmt;
        struct { // while ( condition ) { <body> }
            struct ASTNode* condition;
            struct ASTNode* body;
        } while_stmt;
        struct {
            struct ASTNode* init;      // for ( this ; xxx ; xxx)
            struct ASTNode* condition;  // for ( xxx ; this ; xxx )
            struct ASTNode* update;     // for ( xxx ; xxx ; this )
            struct ASTNode* body; // for ( ... ) { <body> }
        } for_stmt;
        struct { // return <value>
            struct ASTNode* value;
        } return_stmt;
        struct { // `load <module>, ...;` or `load <module>;`
            struct ASTNode* modules;
        } load_stmt;
        struct { // `global <variable>, ...;` or `global <variable>;`
            struct ASTNode* variables;
        } global_stmt;
        struct {
            struct ASTNode** items;
            int count;
        } list; // argument list, list, module list...
        struct { // <left> = <right>
            struct ASTNode* left;   // variable
            struct ASTNode* right;  // expr
        } assignment;
    } data;
} ASTNode;

typedef struct _sbTkState {
    _sbToken* tk;
    int size;
    int position;
}_sbTkState; // alias: Parser

typedef _sbTkState Parser;

extern char* _s_strdup(const char* str); // Need free

extern _sbTkState* create_tkstate(_sbToken* tk);
extern void destroy_tkstate(_sbTkState* tk);

extern _sbToken* current_token(_sbTkState* parser);
extern _sbToken* previous_token(_sbTkState* parser);
extern _sbToken* next(_sbTkState* parser);
extern _sbToken* peek(_sbTkState* parser);
extern _sbToken* peek_next(_sbTkState* parser);
extern _sbToken* peek_ahead(Parser* parser, int offset);
extern bool match_token(_sbTkState* parser, const char* expected);
extern bool check_ahead(Parser* parser, int offset, const char* expected);

extern ASTNode* create_ast_node(_sbNType type, Parser* parser);
extern ASTNode* create_ast_node_with_token(_sbNType type, _sbToken* token);
extern void free_ast(ASTNode* node);

extern ASTNode* parse_primary(Parser* parser);
extern ASTNode* parse_multiplicative(Parser* parser);
extern ASTNode* parse_bitwise_and(Parser* parser);
extern ASTNode* parse_bitwise_or(Parser* parser);
extern ASTNode* parse_bitwise_xor(Parser* parser);
extern ASTNode* parse_shift(Parser* parser);
extern ASTNode* parse_power(Parser* parser);
extern ASTNode* parse_additive(Parser* parser);
extern ASTNode* parse_comparison(Parser* parser);
extern ASTNode* parse_logical(Parser* parser);
extern ASTNode* parse_expression(Parser* parser);
extern ASTNode* parse_block(Parser* parser);
extern ASTNode* parse_assignment(Parser* parser);
extern ASTNode* parse_for_statement(Parser* parser);
extern ASTNode* parse_while_statement(Parser* parser);
extern ASTNode* parse_if_else_statement(Parser* parser);
extern ASTNode* parse_load_statement(Parser* parser);
extern ASTNode* parse_global_statement(Parser* parser);
extern ASTNode* parse_postfix(Parser* parser);
extern ASTNode* parse_list(Parser* parser);
extern ASTNode* parse_function_definition(Parser* parser);
extern ASTNode* parse_struct_definition(Parser* parser);
extern ASTNode* parse_struct_instantiation(Parser* parser);
extern ASTNode* parse_statement(Parser* parser);
extern ASTNode* parse_program(Parser* parser);

#endif
