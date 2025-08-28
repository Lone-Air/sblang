/*
 * SB - Language
 * By Laman28
 * Compiler - Bytecode Generation
 * Not welcome to use /XD
 */

#ifndef _SBL_BYTECODE
#define _SBL_BYTECODE

#include "../parser/parser.h"
#include "../dynarray/dynarray.h"
#include <stdint.h>

/* Bytecode file magic value */
#define SBL_BYTECODE_MAGIC 0x53424C42  /* "SBLB" in ASCII */

/* Dynamic array structure for storing variable-length data */
typedef struct {
    void** items;       /* Array element pointers */
    size_t count;       /* Current number of elements */
    size_t capacity;    /* Array capacity */
} DynArray;

/* Bytecode opcode enumeration */
typedef enum {
    OP_NOP = 0x00,      /* No operation */

    /* Stack operation instructions */
    OP_PUSH_NUM,        /* Push number onto stack */
    OP_PUSH_STR,        /* Push string onto stack */
    OP_PUSH_IDENT,      /* Push identifier onto stack */
    OP_PUSH_TRUE,       /* Push true onto stack */
    OP_PUSH_FALSE,      /* Push false onto stack */
    OP_PUSH_NULL,       /* Push null onto stack */

    OP_POP,             /* Pop top element from stack */
    OP_DUP,             /* Duplicate top element */
    OP_SWAP,            /* Swap top two elements */

    /* Arithmetic operation instructions */
    OP_ADD,             /* Addition */
    OP_SUB,             /* Subtraction */
    OP_MUL,             /* Multiplication */
    OP_DIV,             /* Division */
    OP_MOD,             /* Modulo */
    OP_POW,             /* Exponentiation */

    /* Bitwise operation instructions */
    OP_BIT_AND,         /* Bitwise AND */
    OP_BIT_OR,          /* Bitwise OR */
    OP_BIT_XOR,         /* Bitwise XOR */
    OP_BIT_NOT,         /* Bitwise NOT */
    OP_BIT_LSHIFT,      /* Left shift */
    OP_BIT_RSHIFT,      /* Right shift */

    /* Logical operation instructions */
    OP_LOGIC_AND,       /* Logical AND */
    OP_LOGIC_OR,        /* Logical OR */
    OP_LOGIC_NOT,       /* Logical NOT */

    /* Comparison operation instructions */
    OP_EQ,              /* Equal */
    OP_NEQ,             /* Not equal */
    OP_LT,              /* Less than */
    OP_GT,              /* Greater than */
    OP_LEQ,             /* Less than or equal */
    OP_GEQ,             /* Greater than or equal */

    /* Variable operation instructions */
    OP_ASSIGN,          /* Assignment */
    OP_LOAD_VAR,        /* Load variable */
    OP_STORE_VAR,       /* Store variable */
    OP_LOAD_GLOBAL,     /* Load global variable */
    OP_STORE_GLOBAL,    /* Store global variable */

    /* Control flow instructions */
    OP_JUMP,            /* Unconditional jump */
    OP_JUMP_IF_FALSE,   /* Jump if condition is false */
    OP_JUMP_IF_TRUE,    /* Jump if condition is true */

    /* Function-related instructions */
    OP_CALL,            /* Function call */
    OP_RETURN,          /* Function return */
    OP_FUNC_START,      /* Function start marker */
    OP_FUNC_END,        /* Function end marker */

    /* Code block instructions */
    OP_BLOCK_START,     /* Code block start */
    OP_BLOCK_END,       /* Code block end */

    /* Module instructions */
    OP_LOAD_MODULE,     /* Load module */

    /* Structure instructions */
    OP_STRUCT_DEF,      /* Define structure */
    OP_STRUCT_NEW,      /* Create structure instance */
    OP_MEMBER_ACCESS,   /* Member access */
    OP_MEMBER_STORE,    /* Member store */

    /* List operation instructions */
    OP_LIST_NEW,        /* Create new list */
    OP_LIST_ACCESS,     /* Access list element */
    OP_LIST_STORE,      /* Store list element */
    OP_LIST_PUSH,       /* Push element to list */

    OP_HALT             /* Halt execution */
} OpCode;

/* Bytecode instruction structure */
typedef struct {
    OpCode opcode;              /* Opcode */
    union {
        double num_value;       /* Numeric operand */
        char* str_value;        /* String operand */
        int int_value;          /* Integer operand (for jump addresses, etc.) */
        size_t size_value;      /* size_t type operand */
    } operand;
    int source_line;            /* Source line number for error reporting */
    int source_column;          /* Source column number for error reporting */
} Instruction;

/* Function information structure */
typedef struct {
    char* name;                 /* Function name */
    size_t start_addr;          /* Function start address */
    size_t param_count;         /* Parameter count */
} FunctionInfo;

/* Structure information */
typedef struct {
    char* name;                 /* Structure name */
    DynArray* members;          /* Member list */
} StructInfo;

/* Bytecode generator main structure */
typedef struct {
    DynArray* instructions;     /* Instruction array */
    DynArray* constants;        /* Constant pool */
    DynArray* functions;        /* Function table */
    DynArray* structs;          /* Structure table */
    DynArray* globals;          /* Global variable table */
    size_t current_addr;        /* Current instruction address */
    Parser* parser;             /* Parser reference for source line tracking */
    int current_source_line;    /* Current source line for instruction generation */
    int current_source_column;  /* Current source column for instruction generation */
} BytecodeGenerator;

/* ========== Bytecode Generator Management Functions ========== */

/* Create bytecode generator */
extern BytecodeGenerator* create_bytecode_generator();

/* Destroy bytecode generator, release all resources */
extern void destroy_bytecode_generator(BytecodeGenerator* gen);

/* Set parser reference for source tracking */
extern void bytecode_set_parser(BytecodeGenerator* gen, Parser* parser);

/* Update current source position from AST node */
extern void bytecode_set_source_pos(BytecodeGenerator* gen, int line, int column);

/* ========== Instruction Emission Functions ========== */

/* Emit basic instruction (no operand) */
extern void emit_instruction(BytecodeGenerator* gen, OpCode opcode);

/* Emit instruction with numeric operand */
extern void emit_instruction_with_num(BytecodeGenerator* gen, OpCode opcode, double value);

/* Emit instruction with string operand */
extern void emit_instruction_with_str(BytecodeGenerator* gen, OpCode opcode, const char* value);

/* Emit instruction with integer operand */
extern void emit_instruction_with_int(BytecodeGenerator* gen, OpCode opcode, int value);

/* Emit jump placeholder, return address for later patching */
extern void emit_jump_placeholder(BytecodeGenerator* gen, OpCode opcode, size_t* addr_ref);

/* Patch jump address */
extern void patch_jump(BytecodeGenerator* gen, size_t addr_ref);

/* ========== Constant and Symbol Management Functions ========== */

/* Add numeric constant to constant pool */
extern int add_constant(BytecodeGenerator* gen, double value);

/* Add string constant to constant pool */
extern int add_string_constant(BytecodeGenerator* gen, const char* str);

/* Register global variable */
extern int register_global(BytecodeGenerator* gen, const char* name);

/* Check if it is a global variable */
extern int checkfor_global(BytecodeGenerator* gen, const char* name);

/* Register function */
extern int register_function(BytecodeGenerator* gen, const char* name, size_t param_count);

/* Register structure */
extern int register_struct(BytecodeGenerator* gen, const char* name, DynArray* members);

/* ========== AST to Bytecode Generation Functions ========== */

/* Generate bytecode for the entire program */
extern bool generate_bytecode(BytecodeGenerator* gen, ASTNode* ast);

/* Generate bytecode for expression */
extern bool generate_expression(BytecodeGenerator* gen, ASTNode* node);

/* Generate bytecode for statement */
extern bool generate_statement(BytecodeGenerator* gen, ASTNode* node);

/* Generate bytecode for code block */
extern bool generate_block(BytecodeGenerator* gen, ASTNode* node);

/* Generate bytecode for function definition */
extern bool generate_function(BytecodeGenerator* gen, ASTNode* node);

/* Generate bytecode for structure definition */
extern bool generate_struct(BytecodeGenerator* gen, ASTNode* node);

/* ========== Bytecode Input/Output Functions ========== */

/* Print bytecode (for debugging) */
extern void print_bytecode(BytecodeGenerator* gen);

/* Save bytecode to file */
extern bool save_bytecode(BytecodeGenerator* gen, const char* filename);

/* Load bytecode from file */
extern BytecodeGenerator* load_bytecode(const char* filename);

/* Check if a file is a valid bytecode file by checking magic value */
extern bool is_valid_bytecode_file(const char* filename);

/* Get an element from a dynamic array */
extern void* dynarray_get(DynArray* arr, size_t index);

#endif