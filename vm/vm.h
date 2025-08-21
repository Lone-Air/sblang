/*
 * SB - Language
 * By Laman28
 * Virtual Machine
 * Not welcome to use /XD
 */

#ifndef _SBL_VM
#define _SBL_VM

#include "../bytecode/bytecode.h"
#include <stdbool.h>

#define VM_STACK_SIZE 1024      /* 操作数栈大小 */
#define VM_CALL_STACK_SIZE 256  /* 调用栈深度限制 */
#define VM_MAX_GLOBALS 256      /* 最大全局变量数量 */

/* 值类型枚举 */
typedef enum {
    VAL_NULL,               /* 空值 */
    VAL_NUMBER,             /* 数字类型 */
    VAL_STRING,             /* 字符串类型 */
    VAL_BOOL,               /* 布尔类型 */
    VAL_FUNCTION,           /* 函数类型 */
    VAL_NATIVE,             /* 原生函数类型 */
    VAL_STRUCT,             /* 结构体定义类型 */
    VAL_STRUCT_INSTANCE,    /* 结构体实例类型 */
    VAL_LIST                /* 列表类型 */
} ValueType;

/* 前向声明 */
typedef struct Value Value;
typedef struct VM VM;

/* 原生函数指针类型 */
typedef Value (*NativeFunction)(VM* vm, Value* args, int arg_count);

/* 函数结构 */
typedef struct {
    char* name;             /* 函数名 */
    size_t start_addr;      /* 函数起始地址 */
    size_t param_count;     /* 参数数量 */
    Value* locals;          /* 局部变量数组 */
    size_t local_count;     /* 局部变量数量 */
} Function;

/* 结构体定义 */
typedef struct {
    char* name;             /* 结构体名称 */
    char** members;         /* 成员名称数组 */
    size_t member_count;    /* 成员数量 */
} Struct;

/* 结构体实例 */
typedef struct {
    Struct* struct_def;     /* 结构体定义 */
    Value* members;         /* 成员值数组 */
} StructInstance;

/* 列表结构 */
typedef struct {
    Value* items;           /* 元素数组 */
    size_t count;           /* 当前元素数量 */
    size_t capacity;        /* 容量 */
} List;

/* 值结构体 - 支持多种数据类型 */
struct Value {
    ValueType type;         /* 值的类型 */
    union {
        double number;              /* 数字值 */
        char* string;               /* 字符串值 */
        bool boolean;               /* 布尔值 */
        Function* function;         /* 函数指针 */
        NativeFunction native;      /* 原生函数指针 */
        Struct* struct_def;         /* 结构体定义 */
        StructInstance* instance;   /* 结构体实例 */
        List* list;                 /* 列表指针 */
    } as;
};

/* 调用帧结构 */
typedef struct {
    Function* function;     /* 当前函数 */
    size_t return_addr;     /* 返回地址 */
    Value* locals;          /* 局部变量 */
    size_t local_count;     /* 局部变量数量 */
    size_t stack_base;      /* 栈基址 */
} CallFrame;

/* 变量结构 */
typedef struct {
    char* name;             /* 变量名 */
    Value value;            /* 变量值 */
} Variable;

/* 变量表 */
typedef struct {
    Variable* vars;         /* 变量数组 */
    size_t count;           /* 变量数量 */
    size_t capacity;        /* 容量 */
} VariableTable;

/* VM错误类型枚举 */
typedef enum {
    VM_OK,                  /* 无错误 */
    VM_RUNTIME_ERROR,       /* 运行时错误 */
    VM_STACK_OVERFLOW,      /* 栈溢出 */
    VM_STACK_UNDERFLOW,     /* 栈下溢 */
    VM_UNDEFINED_VARIABLE,  /* 未定义变量 */
    VM_TYPE_ERROR,          /* 类型错误 */
    VM_DIVISION_BY_ZERO,    /* 除零错误 */
    VM_INDEX_OUT_OF_BOUNDS, /* 索引越界 */
    VM_UNDEFINED_FUNCTION,  /* 未定义函数 */
    VM_ARGUMENT_MISMATCH,   /* 参数不匹配 */
    VM_LOAD_ERROR,          /* 加载错误 */
    VM_MEMORY_ERROR,        /* 内存错误 */
    VM_INVALID_OPCODE,      /* 无效操作码 */
    VM_UNDEFINED_MEMBER,    /* 未定义成员 */
    VM_NOT_A_STRUCT         /* 非结构体类型 */
} VMError;

/* 加载的共享库信息 */
typedef struct {
    void* handle;                   /* 动态库句柄 */
    char* name;                     /* 库名称 */
} LoadedLibrary;

/* 虚拟机主结构 */
struct VM {
    /* 指令相关 */
    Instruction* instructions;      /* 指令数组 */
    size_t instruction_count;       /* 指令数量 */
    size_t pc;                      /* 程序计数器 */
    
    /* 操作数栈 */
    Value stack[VM_STACK_SIZE];     /* 操作数栈 */
    size_t stack_top;               /* 栈顶指针 */
    
    /* 调用栈 */
    CallFrame call_stack[VM_CALL_STACK_SIZE];  /* 调用栈 */
    size_t call_depth;              /* 调用深度 */
    
    /* 变量表 */
    VariableTable globals;          /* 全局变量表 */
    VariableTable* locals;          /* 当前局部变量表 */
    
    /* 函数和结构体 */
    Function* functions;            /* 函数表 */
    size_t function_count;          /* 函数数量 */
    Struct* structs;                /* 结构体表 */
    size_t struct_count;            /* 结构体数量 */
    
    /* 加载的共享库 */
    LoadedLibrary* loaded_libs;    /* 已加载的共享库数组 */
    size_t loaded_lib_count;        /* 已加载的共享库数量 */
    
    /* 错误处理 */
    VMError last_error;             /* 最后的错误类型 */
    char* error_message;            /* 错误信息 */
    
    /* 运行状态 */
    bool running;                   /* 运行标志 */
    bool gc_enabled;                /* 垃圾回收标志 */
};

/* 原生函数绑定结构 */
typedef struct {
    char* name;             /* 函数名 */
    NativeFunction func;    /* 函数指针 */
} NativeBinding;

/* ========== VM管理函数 ========== */

/* 创建VM实例 */
extern VM* create_vm();

/* 销毁VM实例，释放所有资源 */
extern void destroy_vm(VM* vm);

/* ========== 栈操作函数 ========== */

/* 将值压入栈 */
extern void vm_push(VM* vm, Value value);

/* 从栈顶弹出值 */
extern Value vm_pop(VM* vm);

/* 查看栈顶元素（不弹出） */
extern Value vm_peek(VM* vm, int distance);

/* ========== 字节码加载函数 ========== */

/* 从字节码生成器加载字节码 */
extern bool vm_load_bytecode(VM* vm, BytecodeGenerator* gen);

/* 从文件加载字节码 */
extern bool vm_load_from_file(VM* vm, const char* filename);

/* ========== 执行函数 ========== */

/* 执行字节码 */
extern VMError vm_execute(VM* vm);

/* 执行单条指令 */
extern VMError vm_execute_instruction(VM* vm);

/* 调用函数 */
extern VMError vm_call_function(VM* vm, Function* func, int arg_count);

/* ========== 原生函数管理 ========== */

/* 注册原生函数 */
extern void vm_register_native(VM* vm, const char* name, NativeFunction func);

/* 从外部向VM栈推送值 */
extern void vm_push_external(VM* vm, Value value);

/* ========== 变量管理函数 ========== */

/* 获取变量值 */
extern Value* vm_get_variable(VM* vm, const char* name);

/* 设置变量值 */
extern bool vm_set_variable(VM* vm, const char* name, Value value);

/* 定义全局变量 */
extern bool vm_define_global(VM* vm, const char* name, Value value);

/* ========== 值创建函数 ========== */

/* 创建空值 */
extern Value create_null();

/* 创建数字值 */
extern Value create_number(double num);

/* 创建字符串值 */
extern Value create_string(const char* str);

/* 创建布尔值 */
extern Value create_bool(bool b);

/* 创建函数值 */
extern Value create_function(Function* func);

/* 创建原生函数值 */
extern Value create_native(NativeFunction func);

/* 创建列表 */
extern Value create_list();

/* ========== 值操作函数 ========== */

/* 判断值是否为真 */
extern bool is_truthy(Value value);

/* 判断两个值是否相等 */
extern bool values_equal(Value a, Value b);

/* 复制值（深拷贝） */
extern Value copy_value(Value value);

/* 释放值所占用的内存 */
extern void free_value(Value value);

/* ========== 错误处理函数 ========== */

/* 设置VM错误 */
extern void vm_error(VM* vm, VMError error, const char* format, ...);

/* 获取错误类型字符串 */
extern const char* vm_error_string(VMError error);

/* 打印栈内容（调试用） */
extern void vm_print_stack(VM* vm);

/* 打印错误信息 */
extern void vm_print_error(VM* vm);

/* ========== 垃圾回收函数 ========== */

/* 执行垃圾回收 */
extern void vm_gc_collect(VM* vm);

/* 标记值（GC第一阶段） */
extern void vm_gc_mark(VM* vm, Value value);

/* 清除未标记的对象（GC第二阶段） */
extern void vm_gc_sweep(VM* vm);

#endif