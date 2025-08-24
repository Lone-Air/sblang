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

#define VM_INITIAL_STACK_SIZE 1024      /* Initial operand stack size */
#define VM_INITIAL_CALL_STACK_SIZE 256  /* Initial call stack depth */

/* Value type enumeration */
typedef enum {
    VAL_NULL,               /* Null value */
    VAL_NUMBER,             /* Number type */
    VAL_STRING,             /* String type */
    VAL_BOOL,               /* Boolean type */
    VAL_FUNCTION,           /* Function type */
    VAL_NATIVE,             /* Native function type */
    VAL_STRUCT,             /* Struct definition type */
    VAL_STRUCT_INSTANCE,    /* Struct instance type */
    VAL_LIST                /* List type */
} ValueType;

/* Forward declarations */
typedef struct Value Value;
typedef struct VM VM;

/* Native function pointer type */
typedef Value (*NativeFunction)(VM* vm, Value* args, int arg_count);

/* Function structure */
typedef struct {
    char* name;             /* Function name */
    size_t start_addr;      /* Function start address */
    size_t param_count;     /* Parameter count */
    Value* locals;          /* Local variables array */
    size_t local_count;     /* Local variable count */
} Function;

/* Struct definition */
typedef struct {
    char* name;             /* Struct name */
    char** members;         /* Member names array */
    size_t member_count;    /* Member count */
} Struct;

/* Struct instance */
typedef struct {
    Struct* struct_def;     /* Struct definition */
    Value* members;         /* Member values array */
} StructInstance;

/* List structure */
typedef struct {
    Value* items;           /* Elements array */
    size_t count;           /* Current element count */
    size_t capacity;        /* Capacity */
} List;

/* GC object header for tracking allocated objects */
typedef struct GCObject {
    struct GCObject* next;      /* Next object in linked list */
    bool marked;                /* Mark flag for GC */
    ValueType type;             /* Type of the object */
    void* data;                 /* Pointer to the actual data */
} GCObject;

/* Value structure - supports multiple data types */
struct Value {
    ValueType type;         /* Value type */
    union {
        double number;              /* Numeric value */
        char* string;               /* String value */
        bool boolean;               /* Boolean value */
        Function* function;         /* Function pointer */
        NativeFunction native;      /* Native function pointer */
        Struct* struct_def;         /* Struct definition */
        StructInstance* instance;   /* Struct instance */
        List* list;                 /* List pointer */
    } as;
};

/* Call frame structure */
typedef struct {
    Function* function;     /* Current function */
    size_t return_addr;     /* Return address */
    Value* locals;          /* Local variables */
    size_t local_count;     /* Local variable count */
    size_t stack_base;      /* Stack base address */
} CallFrame;

/* Variable structure */
typedef struct {
    char* name;             /* Variable name */
    Value value;            /* Variable value */
} Variable;

/* Variable table */
typedef struct {
    Variable* vars;         /* Variables array */
    size_t count;           /* Variable count */
    size_t capacity;        /* Capacity */
} VariableTable;

/* VM error type enumeration */
typedef enum {
    VM_OK,                  /* No error */
    VM_RUNTIME_ERROR,       /* Runtime error */
    VM_STACK_OVERFLOW,      /* Stack overflow */
    VM_STACK_UNDERFLOW,     /* Stack underflow */
    VM_UNDEFINED_VARIABLE,  /* Undefined variable */
    VM_TYPE_ERROR,          /* Type error */
    VM_DIVISION_BY_ZERO,    /* Division by zero */
    VM_INDEX_OUT_OF_BOUNDS, /* Index out of bounds */
    VM_UNDEFINED_FUNCTION,  /* Undefined function */
    VM_ARGUMENT_MISMATCH,   /* Argument mismatch */
    VM_LOAD_ERROR,          /* Load error */
    VM_MEMORY_ERROR,        /* Memory error */
    VM_INVALID_OPCODE,      /* Invalid opcode */
    VM_UNDEFINED_MEMBER,    /* Undefined member */
    VM_NOT_A_STRUCT         /* Not a struct type */
} VMError;

/* Loaded shared library information */
typedef struct {
    void* handle;                   /* Dynamic library handle */
    char* name;                     /* Library name */
} LoadedLibrary;

/* Main VM structure */
struct VM {
    /* Instruction related */
    Instruction* instructions;      /* Instructions array */
    size_t instruction_count;       /* Instruction count */
    size_t pc;                      /* Program counter */

    /* Operand stack */
    Value* stack;                   /* Dynamic operand stack */
    size_t stack_top;               /* Stack top pointer */
    size_t stack_capacity;          /* Operand stack capacity */

    /* Call stack */
    CallFrame* call_stack;          /* Dynamic call stack */
    size_t call_depth;              /* Call depth */
    size_t call_capacity;           /* Call stack capacity */

    /* Variable table */
    VariableTable globals;          /* Global variable table */
    VariableTable* locals;          /* Current local variable table */

    /* Functions and structs */
    Function* functions;            /* Function table */
    size_t function_count;          /* Function count */
    Struct* structs;                /* Struct table */
    size_t struct_count;            /* Struct count */

    /* Loaded shared libraries */
    LoadedLibrary* loaded_libs;    /* Loaded shared libraries array */
    size_t loaded_lib_count;        /* Loaded shared libraries count */

    /* Error handling */
    VMError last_error;             /* Last error type */
    char* error_message;            /* Error message */

    /* Runtime status */
    bool running;                   /* Running flag */
    bool gc_enabled;                /* Garbage collection flag */
    
    /* Garbage collection */
    GCObject* gc_objects;           /* Linked list of all allocated objects */
    size_t gc_object_count;         /* Number of allocated objects */
    size_t gc_threshold;            /* GC trigger threshold */
    size_t gc_bytes_allocated;      /* Total bytes allocated */

    int end_pc;
};

/* Native function binding structure */
typedef struct {
    char* name;             /* Function name */
    NativeFunction func;    /* Function pointer */
} NativeBinding;

/* ========== VM Management Functions ========== */

/* Create VM instance */
extern VM* create_vm();

/* Destroy VM instance, free all resources */
extern void destroy_vm(VM* vm);

/* ========== Stack Operation Functions ========== */

/* Push value onto stack */
extern void vm_push(VM* vm, Value value);

/* Pop value from stack top */
extern Value vm_pop(VM* vm);

/* Peek at stack top element (without popping) */
extern Value vm_peek(VM* vm, int distance);

/* ========== Bytecode Loading Functions ========== */

/* Load bytecode from bytecode generator */
extern bool vm_load_bytecode(VM* vm, BytecodeGenerator* gen);

/* Load bytecode from file */
extern bool vm_load_from_file(VM* vm, const char* filename);

/* ========== Execution Functions ========== */

/* Execute bytecode */
extern VMError vm_execute(VM* vm);

/* Execute single instruction */
extern VMError vm_execute_instruction(VM* vm);

/* Call function */
extern VMError vm_call_function(VM* vm, Function* func, int arg_count);

/* ========== Native Function Management ========== */

/* Register native function */
extern void vm_register_native(VM* vm, const char* name, NativeFunction func);

/* Push value to VM stack from external source */
extern void vm_push_external(VM* vm, Value value);

/* ========== Variable Management Functions ========== */

/* Get variable value */
extern Value* vm_get_variable(VM* vm, const char* name);

/* Set variable value */
extern bool vm_set_variable(VM* vm, const char* name, Value value);

/* Define global variable */
extern bool vm_define_global(VM* vm, const char* name, Value value);

/* ========== Value Creation Functions ========== */

/* Create null value */
extern Value create_null();

/* Create numeric value */
extern Value create_number(double num);

/* Create string value */
extern Value create_string(VM* vm, const char* str);

/* Create boolean value */
extern Value create_bool(bool b);

/* Create function value */
extern Value create_function(Function* func);

/* Create native function value */
extern Value create_native(NativeFunction func);

/* Create list */
extern Value create_list(VM* vm);

/* ========== Value Operation Functions ========== */

/* Check if value is truthy */
extern bool is_truthy(Value value);

/* Check if two values are equal */
extern bool values_equal(Value a, Value b);

/* Copy value (deep copy) */
extern Value copy_value(VM* vm, Value value);

/* Free memory occupied by value (GC-aware) */
extern void free_value_gc(VM* vm, Value value);

/* Free memory occupied by value */
extern void free_value(Value value);

/* ========== Error Handling Functions ========== */

/* Set VM error */
extern void vm_error(VM* vm, VMError error, const char* format, ...);

/* Get error type string */
extern const char* vm_error_string(VMError error);

/* Print stack contents (for debugging) */
extern void vm_print_stack(VM* vm);

/* Print error information */
extern void vm_print_error(VM* vm);

/* ========== Garbage Collection Functions ========== */

/* Perform garbage collection */
extern void vm_gc_collect(VM* vm);

/* Mark value (GC phase one) */
extern void vm_gc_mark(VM* vm, Value value);

/* Sweep unmarked objects (GC phase two) */
extern void vm_gc_sweep(VM* vm);

/* Allocate memory with GC tracking */
extern void* gc_alloc(VM* vm, size_t size, ValueType type);

/* Free GC object */
extern void gc_free_object(VM* vm, GCObject* obj);

/* Mark all reachable objects from roots */
extern void vm_gc_mark_roots(VM* vm);

/* Check if pointer is GC-managed */
extern bool is_gc_managed(VM* vm, void* ptr);

#endif