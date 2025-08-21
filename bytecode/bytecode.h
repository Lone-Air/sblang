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

/* 动态数组结构，用于存储可变长度的数据 */
typedef struct {
    void** items;       /* 数组元素指针 */
    size_t count;       /* 当前元素数量 */
    size_t capacity;    /* 数组容量 */
} DynArray;

/* 字节码操作码枚举 */
typedef enum {
    OP_NOP = 0x00,      /* 空操作 */
    
    /* 栈操作指令 */
    OP_PUSH_NUM,        /* 将数字压入栈 */
    OP_PUSH_STR,        /* 将字符串压入栈 */
    OP_PUSH_IDENT,      /* 将标识符压入栈 */
    OP_PUSH_TRUE,       /* 将true压入栈 */
    OP_PUSH_FALSE,      /* 将false压入栈 */
    OP_PUSH_NULL,       /* 将null压入栈 */
    
    OP_POP,             /* 弹出栈顶元素 */
    OP_DUP,             /* 复制栈顶元素 */
    OP_SWAP,            /* 交换栈顶两个元素 */
    
    /* 算术运算指令 */
    OP_ADD,             /* 加法 */
    OP_SUB,             /* 减法 */
    OP_MUL,             /* 乘法 */
    OP_DIV,             /* 除法 */
    OP_MOD,             /* 取模 */
    OP_POW,             /* 幂运算 */
    
    /* 位运算指令 */
    OP_BIT_AND,         /* 按位与 */
    OP_BIT_OR,          /* 按位或 */
    OP_BIT_XOR,         /* 按位异或 */
    OP_BIT_NOT,         /* 按位取反 */
    OP_BIT_LSHIFT,      /* 左移 */
    OP_BIT_RSHIFT,      /* 右移 */
    
    /* 逻辑运算指令 */
    OP_LOGIC_AND,       /* 逻辑与 */
    OP_LOGIC_OR,        /* 逻辑或 */
    OP_LOGIC_NOT,       /* 逻辑非 */
    
    /* 比较运算指令 */
    OP_EQ,              /* 等于 */
    OP_NEQ,             /* 不等于 */
    OP_LT,              /* 小于 */
    OP_GT,              /* 大于 */
    OP_LEQ,             /* 小于等于 */
    OP_GEQ,             /* 大于等于 */
    
    /* 变量操作指令 */
    OP_ASSIGN,          /* 赋值 */
    OP_LOAD_VAR,        /* 加载变量 */
    OP_STORE_VAR,       /* 存储变量 */
    OP_LOAD_GLOBAL,     /* 加载全局变量 */
    OP_STORE_GLOBAL,    /* 存储全局变量 */
    
    /* 控制流指令 */
    OP_JUMP,            /* 无条件跳转 */
    OP_JUMP_IF_FALSE,   /* 条件为假时跳转 */
    OP_JUMP_IF_TRUE,    /* 条件为真时跳转 */
    
    /* 函数相关指令 */
    OP_CALL,            /* 函数调用 */
    OP_RETURN,          /* 函数返回 */
    OP_FUNC_START,      /* 函数开始标记 */
    OP_FUNC_END,        /* 函数结束标记 */
    
    /* 代码块指令 */
    OP_BLOCK_START,     /* 代码块开始 */
    OP_BLOCK_END,       /* 代码块结束 */
    
    /* 模块指令 */
    OP_LOAD_MODULE,     /* 加载模块 */
    
    /* 结构体指令 */
    OP_STRUCT_DEF,      /* 定义结构体 */
    OP_STRUCT_NEW,      /* 创建结构体实例 */
    OP_MEMBER_ACCESS,   /* 访问成员 */
    OP_MEMBER_STORE,    /* 存储成员 */
    
    /* 列表操作指令 */
    OP_LIST_NEW,        /* 创建新列表 */
    OP_LIST_ACCESS,     /* 访问列表元素 */
    OP_LIST_STORE,      /* 存储列表元素 */
    OP_LIST_PUSH,       /* 向列表添加元素 */
    
    OP_HALT             /* 停止执行 */
} OpCode;

/* 字节码指令结构 */
typedef struct {
    OpCode opcode;              /* 操作码 */
    union {
        double num_value;       /* 数字操作数 */
        char* str_value;        /* 字符串操作数 */
        int int_value;          /* 整数操作数（用于跳转地址等） */
        size_t size_value;      /* size_t类型操作数 */
    } operand;
} Instruction;

/* 函数信息结构 */
typedef struct {
    char* name;                 /* 函数名称 */
    size_t start_addr;          /* 函数起始地址 */
    size_t param_count;         /* 参数数量 */
} FunctionInfo;

/* 结构体信息 */
typedef struct {
    char* name;                 /* 结构体名称 */
    DynArray* members;          /* 成员列表 */
} StructInfo;

/* 字节码生成器主结构 */
typedef struct {
    DynArray* instructions;     /* 指令数组 */
    DynArray* constants;        /* 常量池 */
    DynArray* functions;        /* 函数表 */
    DynArray* structs;          /* 结构体表 */
    DynArray* globals;          /* 全局变量表 */
    size_t current_addr;        /* 当前指令地址 */
} BytecodeGenerator;

/* ========== 字节码生成器管理函数 ========== */

/* 创建字节码生成器 */
extern BytecodeGenerator* create_bytecode_generator();

/* 销毁字节码生成器，释放所有资源 */
extern void destroy_bytecode_generator(BytecodeGenerator* gen);

/* ========== 指令发射函数 ========== */

/* 发射基本指令（无操作数） */
extern void emit_instruction(BytecodeGenerator* gen, OpCode opcode);

/* 发射带数字操作数的指令 */
extern void emit_instruction_with_num(BytecodeGenerator* gen, OpCode opcode, double value);

/* 发射带字符串操作数的指令 */
extern void emit_instruction_with_str(BytecodeGenerator* gen, OpCode opcode, const char* value);

/* 发射带整数操作数的指令 */
extern void emit_instruction_with_int(BytecodeGenerator* gen, OpCode opcode, int value);

/* 发射跳转占位符，返回地址用于后续回填 */
extern void emit_jump_placeholder(BytecodeGenerator* gen, OpCode opcode, size_t* addr_ref);

/* 回填跳转地址 */
extern void patch_jump(BytecodeGenerator* gen, size_t addr_ref);

/* ========== 常量和符号管理函数 ========== */

/* 添加数字常量到常量池 */
extern int add_constant(BytecodeGenerator* gen, double value);

/* 添加字符串常量到常量池 */
extern int add_string_constant(BytecodeGenerator* gen, const char* str);

/* 注册全局变量 */
extern int register_global(BytecodeGenerator* gen, const char* name);

/* 检查是否为全局变量 */
extern int checkfor_global(BytecodeGenerator* gen, const char* name);

/* 注册函数 */
extern int register_function(BytecodeGenerator* gen, const char* name, size_t param_count);

/* 注册结构体 */
extern int register_struct(BytecodeGenerator* gen, const char* name, DynArray* members);

/* ========== AST到字节码的生成函数 ========== */

/* 生成整个程序的字节码 */
extern bool generate_bytecode(BytecodeGenerator* gen, ASTNode* ast);

/* 生成表达式的字节码 */
extern bool generate_expression(BytecodeGenerator* gen, ASTNode* node);

/* 生成语句的字节码 */
extern bool generate_statement(BytecodeGenerator* gen, ASTNode* node);

/* 生成代码块的字节码 */
extern bool generate_block(BytecodeGenerator* gen, ASTNode* node);

/* 生成函数定义的字节码 */
extern bool generate_function(BytecodeGenerator* gen, ASTNode* node);

/* 生成结构体定义的字节码 */
extern bool generate_struct(BytecodeGenerator* gen, ASTNode* node);

/* ========== 字节码输入输出函数 ========== */

/* 打印字节码（用于调试） */
extern void print_bytecode(BytecodeGenerator* gen);

/* 保存字节码到文件 */
extern bool save_bytecode(BytecodeGenerator* gen, const char* filename);

/* 从文件加载字节码 */
extern BytecodeGenerator* load_bytecode(const char* filename);

#endif