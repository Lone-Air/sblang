/*
 * SB - Language
 * By Laman28
 * Compiler - Bytecode Generation
 * Not welcome to use /XD
 */

#include "bytecode.h"
#include "../error/error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* ========== Dynamic Array Helper Functions ========== */

/* Create a dynamic array */
static DynArray* create_dynarray() {
    DynArray* arr = (DynArray*)malloc(sizeof(DynArray));
    if (!arr) return nullptr;

    arr->items = nullptr;
    arr->count = 0;
    arr->capacity = 0;
    return arr;
}

/* Destroy a dynamic array */
static void destroy_dynarray(DynArray* arr) {
    if (!arr) return;
    if (arr->items) {
        free(arr->items);
        arr->items = nullptr;
    }
    free(arr);
}

/* Add an element to a dynamic array */
static bool dynarray_push(DynArray* arr, void* item) {
    if (!arr) return false;

    /* Check if expansion is needed */
    if (arr->count >= arr->capacity) {
        size_t new_capacity = arr->capacity == 0 ? 8 : arr->capacity * 2;
        void** new_items = (void**)realloc(arr->items, new_capacity * sizeof(void*));
        if (!new_items) return false;

        arr->items = new_items;
        arr->capacity = new_capacity;
    }

    arr->items[arr->count++] = item;
    return true;
}

/* Get an element from a dynamic array */
static void* dynarray_get(DynArray* arr, size_t index) {
    if (!arr || index >= arr->count) return nullptr;
    return arr->items[index];
}

/* ========== Bytecode Generator Management ========== */

/**
 * Create a bytecode generator
 * Initializes all necessary data structures
 */
BytecodeGenerator* create_bytecode_generator() {
    BytecodeGenerator* gen = (BytecodeGenerator*)malloc(sizeof(BytecodeGenerator));
    if (!gen) return nullptr;

    /* Initialize various tables */
    gen->instructions = create_dynarray();  /* Instruction table */
    gen->constants = create_dynarray();     /* Constant pool */
    gen->functions = create_dynarray();     /* Function table */
    gen->structs = create_dynarray();       /* Struct table */
    gen->globals = create_dynarray();       /* Global variable table */
    gen->current_addr = 0;                  /* Start current address from 0 */

    /* Check if memory allocation was successful */
    if (!gen->instructions || !gen->constants || !gen->functions ||
        !gen->structs || !gen->globals) {
        destroy_bytecode_generator(gen);
        return nullptr;
    }

    return gen;
}

/**
 * Destroy a bytecode generator
 * Frees all allocated memory, including string operands
 */
void destroy_bytecode_generator(BytecodeGenerator* gen) {
    if (!gen) return;

    /* Free instruction array and its string operands */
    if (gen->instructions) {
        for (size_t i = 0; i < gen->instructions->count; i++) {
            Instruction* inst = (Instruction*)dynarray_get(gen->instructions, i);
            if (inst) {
                /* Free string-type operands */
                if (inst->opcode == OP_PUSH_STR || inst->opcode == OP_PUSH_IDENT ||
                    inst->opcode == OP_LOAD_VAR || inst->opcode == OP_STORE_VAR ||
                    inst->opcode == OP_LOAD_MODULE || inst->opcode == OP_FUNC_START ||
                    inst->opcode == OP_STRUCT_DEF || inst->opcode == OP_STRUCT_NEW ||
                    inst->opcode == OP_MEMBER_ACCESS || inst->opcode == OP_MEMBER_STORE ||
                    inst->opcode == OP_LOAD_GLOBAL || inst->opcode == OP_STORE_GLOBAL) {
                    if (inst->operand.str_value) free(inst->operand.str_value);
                }
                free(inst);
                inst = nullptr;
            }
        }
        destroy_dynarray(gen->instructions);
    }
    
    if (gen->constants) {
        for (size_t i = 0; i < gen->constants->count; i++) {
            void* constant = dynarray_get(gen->constants, i);
            if (constant) free(constant);
        }
        destroy_dynarray(gen->constants);
    }
    
    if (gen->functions) {
        for (size_t i = 0; i < gen->functions->count; i++) {
            FunctionInfo* func = (FunctionInfo*)dynarray_get(gen->functions, i);
            if (func) {
                if (func->name) free(func->name);
                free(func);
            }
        }
        destroy_dynarray(gen->functions);
    }
    
    if (gen->structs) {
        for (size_t i = 0; i < gen->structs->count; i++) {
            StructInfo* st = (StructInfo*)dynarray_get(gen->structs, i);
            if (st) {
                if (st->name) free(st->name);
                if (st->members) {
                    for (size_t j = 0; j < st->members->count; j++) {
                        char* member = (char*)dynarray_get(st->members, j);
                        if (member) free(member);
                    }
                    destroy_dynarray(st->members);
                }
                free(st);
            }
        }
        destroy_dynarray(gen->structs);
    }
    
    if (gen->globals) {
        for (size_t i = 0; i < gen->globals->count; i++) {
            char* global = (char*)dynarray_get(gen->globals, i);
            if (global) free(global);
        }
        destroy_dynarray(gen->globals);
    }
    
    free(gen);
}

void emit_instruction(BytecodeGenerator* gen, OpCode opcode) {
    if (!gen) return;
    
    Instruction* inst = (Instruction*)malloc(sizeof(Instruction));
    if (!inst) return;
    
    inst->opcode = opcode;
    inst->operand.int_value = 0;
    
    dynarray_push(gen->instructions, inst);
    gen->current_addr++;
}

void emit_instruction_with_num(BytecodeGenerator* gen, OpCode opcode, double value) {
    if (!gen) return;
    
    Instruction* inst = (Instruction*)malloc(sizeof(Instruction));
    if (!inst) return;
    
    inst->opcode = opcode;
    inst->operand.num_value = value;
    
    dynarray_push(gen->instructions, inst);
    gen->current_addr++;
}

void emit_instruction_with_str(BytecodeGenerator* gen, OpCode opcode, const char* value) {
    if (!gen || !value) return;
    
    Instruction* inst = (Instruction*)malloc(sizeof(Instruction));
    if (!inst) return;
    
    inst->opcode = opcode;
    inst->operand.str_value = _s_strdup(value);
    
    dynarray_push(gen->instructions, inst);
    gen->current_addr++;
}

void emit_instruction_with_int(BytecodeGenerator* gen, OpCode opcode, int value) {
    if (!gen) return;
    
    Instruction* inst = (Instruction*)malloc(sizeof(Instruction));
    if (!inst) return;
    
    inst->opcode = opcode;
    inst->operand.int_value = value;
    
    dynarray_push(gen->instructions, inst);
    gen->current_addr++;
}

void emit_jump_placeholder(BytecodeGenerator* gen, OpCode opcode, size_t* addr_ref) {
    if (!gen || !addr_ref) return;
    
    *addr_ref = gen->current_addr;
    emit_instruction_with_int(gen, opcode, -1);
}

void patch_jump(BytecodeGenerator* gen, size_t addr_ref) {
    if (!gen || addr_ref >= gen->instructions->count) return;
    
    Instruction* inst = (Instruction*)dynarray_get(gen->instructions, addr_ref);
    if (inst) {
        inst->operand.int_value = (int)gen->current_addr;
    }
}

int add_constant(BytecodeGenerator* gen, double value) {
    if (!gen) return -1;
    
    double* constant = (double*)malloc(sizeof(double));
    if (!constant) return -1;
    
    *constant = value;
    dynarray_push(gen->constants, constant);
    return (int)(gen->constants->count - 1);
}

int add_string_constant(BytecodeGenerator* gen, const char* str) {
    if (!gen || !str) return -1;
    
    char* constant = _s_strdup(str);
    if (!constant) return -1;
    
    dynarray_push(gen->constants, constant);
    return (int)(gen->constants->count - 1);
}

int register_global(BytecodeGenerator* gen, const char* name) {
    if (!gen || !name) return -1;
    
    for (size_t i = 0; i < gen->globals->count; i++) {
        char* global = (char*)dynarray_get(gen->globals, i);
        if (global && strcmp(global, name) == 0) {
            return (int)i;
        }
    }
    
    char* global = _s_strdup(name);
    if (!global) return -1;
    
    dynarray_push(gen->globals, global);
    return (int)(gen->globals->count - 1);
}

int checkfor_global(BytecodeGenerator* gen, const char* name) {
    if (!gen || !name) return -1;

    for (size_t i = 0; i < gen->globals->count; i++) {
        char* global = (char*)dynarray_get(gen->globals, i);
        if (global && strcmp(global, name) == 0) {
            return 1;
        }
    }

    return -1;
}

int register_function(BytecodeGenerator* gen, const char* name, size_t param_count) {
    if (!gen || !name) return -1;
    
    FunctionInfo* func = (FunctionInfo*)malloc(sizeof(FunctionInfo));
    if (!func) return -1;
    
    func->name = _s_strdup(name);
    func->start_addr = gen->current_addr;
    func->param_count = param_count;
    
    dynarray_push(gen->functions, func);
    return (int)(gen->functions->count - 1);
}

int register_struct(BytecodeGenerator* gen, const char* name, DynArray* members) {
    if (!gen || !name) return -1;
    
    StructInfo* st = (StructInfo*)malloc(sizeof(StructInfo));
    if (!st) return -1;
    
    st->name = _s_strdup(name);
    st->members = members;
    
    dynarray_push(gen->structs, st);
    return (int)(gen->structs->count - 1);
}

bool generate_expression(BytecodeGenerator* gen, ASTNode* node) {
    if (!gen || !node) return false;
    
    switch (node->type) {
        case _sbNUMBER_LITERAL:
            emit_instruction_with_num(gen, OP_PUSH_NUM, node->data.num_value);
            return true;
            
        case _sbSTRING_LITERAL:
            emit_instruction_with_str(gen, OP_PUSH_STR, node->data.str_value);
            return true;
            
        case _sbIDENTIFIER:
            emit_instruction_with_str(gen, OP_LOAD_VAR, node->data.str_value);
            return true;
            
        case _sbLIST_LITERAL:
            emit_instruction_with_int(gen, OP_LIST_NEW, node->data.list.count);
            for (int i = node->data.list.count - 1; i >= 0; i--) {
                if (!generate_expression(gen, node->data.list.items[i])) return false;
                emit_instruction(gen, OP_LIST_PUSH);
            }
            return true;
            
        case _sbBINARY_LITERAL: {
            if (!generate_expression(gen, node->data.binary_op.left)) return false;
            if (!generate_expression(gen, node->data.binary_op.right)) return false;
            
            const char* op = node->data.binary_op.op;
            if (strcmp(op, "+") == 0) emit_instruction(gen, OP_ADD);
            else if (strcmp(op, "-") == 0) emit_instruction(gen, OP_SUB);
            else if (strcmp(op, "*") == 0) emit_instruction(gen, OP_MUL);
            else if (strcmp(op, "/") == 0) emit_instruction(gen, OP_DIV);
            else if (strcmp(op, "%") == 0) emit_instruction(gen, OP_MOD);
            else if (strcmp(op, "**") == 0) emit_instruction(gen, OP_POW);
            else if (strcmp(op, "&") == 0) emit_instruction(gen, OP_BIT_AND);
            else if (strcmp(op, "|") == 0) emit_instruction(gen, OP_BIT_OR);
            else if (strcmp(op, "^") == 0) emit_instruction(gen, OP_BIT_XOR);
            else if (strcmp(op, "<<") == 0) emit_instruction(gen, OP_BIT_LSHIFT);
            else if (strcmp(op, ">>") == 0) emit_instruction(gen, OP_BIT_RSHIFT);
            else if (strcmp(op, "&&") == 0) emit_instruction(gen, OP_LOGIC_AND);
            else if (strcmp(op, "||") == 0) emit_instruction(gen, OP_LOGIC_OR);
            else if (strcmp(op, "==") == 0) emit_instruction(gen, OP_EQ);
            else if (strcmp(op, "!=") == 0) emit_instruction(gen, OP_NEQ);
            else if (strcmp(op, "<") == 0) emit_instruction(gen, OP_LT);
            else if (strcmp(op, ">") == 0) emit_instruction(gen, OP_GT);
            else if (strcmp(op, "<=") == 0) emit_instruction(gen, OP_LEQ);
            else if (strcmp(op, ">=") == 0) emit_instruction(gen, OP_GEQ);
            else {
                bytecode_error("Unknown binary operator: %s", op);
                return false;
            }
            return true;
        }
        
        case _sbUNARY_LITERAL: {
            if (!generate_expression(gen, node->data.unary_op.operand)) return false;
            
            const char* op = node->data.unary_op.op;
            if (strcmp(op, "!") == 0) emit_instruction(gen, OP_LOGIC_NOT);
            else if (strcmp(op, "~") == 0) emit_instruction(gen, OP_BIT_NOT);
            else if (strcmp(op, "-") == 0) {
                emit_instruction_with_num(gen, OP_PUSH_NUM, -1);
                emit_instruction(gen, OP_MUL);
            }
            else {
                bytecode_error("Unknown unary operator: %s", op);
                return false;
            }
            return true;
        }
        
        case _sbFUNCTION_CALL: {
            if (node->data.function_call.function_name->type == _sbIDENTIFIER) {
                emit_instruction_with_str(gen, OP_PUSH_IDENT, 
                    node->data.function_call.function_name->data.str_value);
            } else {
                if (!generate_expression(gen, node->data.function_call.function_name)) return false;
            }
            
            if (node->data.function_call.arguments) {
                ASTNode* args = node->data.function_call.arguments;
                if (args->type == _sbARGUMENTS) {
                    for (int i = 0; i < args->data.list.count; i++) {
                        if (!generate_expression(gen, args->data.list.items[i])) return false;
                    }
                    emit_instruction_with_int(gen, OP_CALL, args->data.list.count);
                }
            } else {
                emit_instruction_with_int(gen, OP_CALL, 0);
            }
            
            return true;
        }
        
        case _sbMEMBER_ACCESS: {
            if (!generate_expression(gen, node->data.member_access.object)) return false;
            if (node->data.member_access.member->type == _sbIDENTIFIER) {
                emit_instruction_with_str(gen, OP_MEMBER_ACCESS, 
                    node->data.member_access.member->data.str_value);
            } else {
                bytecode_error("Invalid member access");
                return false;
            }
            return true;
        }
        
        case _sbLIST_ACCESS: {
            if (!generate_expression(gen, node->data.list_access.list)) return false;
            if (!generate_expression(gen, node->data.list_access.index)) return false;
            emit_instruction(gen, OP_LIST_ACCESS);
            return true;
        }
        
        default:
            bytecode_error("Unexpected node type in expression: %d", node->type);
            return false;
    }
}

bool generate_statement(BytecodeGenerator* gen, ASTNode* node) {
    if (!gen || !node) return false;
    
    switch (node->type) {
        case _sbASSIGNMENT: {
            if (!generate_expression(gen, node->data.assignment.right)) return false;
            
            ASTNode* left = node->data.assignment.left;
            if (left->type == _sbIDENTIFIER) {
                if (checkfor_global(gen, left->data.str_value) == -1)
                    emit_instruction_with_str(gen, OP_STORE_VAR, left->data.str_value);
                else
                    emit_instruction_with_str(gen, OP_STORE_GLOBAL, left->data.str_value);
            } else if (left->type == _sbMEMBER_ACCESS) {
                if (!generate_expression(gen, left->data.member_access.object)) return false;
                emit_instruction_with_str(gen, OP_MEMBER_STORE, 
                    left->data.member_access.member->data.str_value);
            } else if (left->type == _sbLIST_ACCESS) {
                if (!generate_expression(gen, left->data.list_access.list)) return false;
                if (!generate_expression(gen, left->data.list_access.index)) return false;
                emit_instruction(gen, OP_LIST_STORE);
            } else {
                bytecode_error("Invalid assignment target");
                return false;
            }
            return true;
        }
        
        case _sbIF: {
            if (!generate_expression(gen, node->data.if_stmt.condition)) return false;
            
            size_t jump_false_addr;
            emit_jump_placeholder(gen, OP_JUMP_IF_FALSE, &jump_false_addr);
            
            if (!generate_statement(gen, node->data.if_stmt.then_branch)) return false;
            
            if (node->data.if_stmt.else_branch) {
                size_t jump_end_addr;
                emit_jump_placeholder(gen, OP_JUMP, &jump_end_addr);
                patch_jump(gen, jump_false_addr);
                
                if (!generate_statement(gen, node->data.if_stmt.else_branch)) return false;
                patch_jump(gen, jump_end_addr);
            } else {
                patch_jump(gen, jump_false_addr);
            }
            return true;
        }
        
        case _sbWHILE: {
            size_t loop_start = gen->current_addr;
            
            if (!generate_expression(gen, node->data.while_stmt.condition)) return false;
            
            size_t jump_false_addr;
            emit_jump_placeholder(gen, OP_JUMP_IF_FALSE, &jump_false_addr);
            
            if (!generate_statement(gen, node->data.while_stmt.body)) return false;
            
            emit_instruction_with_int(gen, OP_JUMP, (int)loop_start);
            patch_jump(gen, jump_false_addr);
            return true;
        }
        
        case _sbFOR: {
            if (node->data.for_stmt.init) {
                if (!generate_statement(gen, node->data.for_stmt.init)) return false;
            }
            
            size_t loop_start = gen->current_addr;
            
            if (node->data.for_stmt.condition) {
                if (!generate_expression(gen, node->data.for_stmt.condition)) return false;
                
                size_t jump_false_addr;
                emit_jump_placeholder(gen, OP_JUMP_IF_FALSE, &jump_false_addr);
                
                if (!generate_statement(gen, node->data.for_stmt.body)) return false;
                
                if (node->data.for_stmt.update) {
                    if (!generate_statement(gen, node->data.for_stmt.update)) return false;
                }
                
                emit_instruction_with_int(gen, OP_JUMP, (int)loop_start);
                patch_jump(gen, jump_false_addr);
            } else {
                if (!generate_statement(gen, node->data.for_stmt.body)) return false;
                
                if (node->data.for_stmt.update) {
                    if (!generate_statement(gen, node->data.for_stmt.update)) return false;
                }
                
                emit_instruction_with_int(gen, OP_JUMP, (int)loop_start);
            }
            return true;
        }
        
        case _sbRETURN: {
            if (node->data.return_stmt.value) {
                if (!generate_expression(gen, node->data.return_stmt.value)) return false;
            } else {
                emit_instruction(gen, OP_PUSH_NULL);
            }
            emit_instruction(gen, OP_RETURN);
            return true;
        }
        
        case _sbBLOCK:
            return generate_block(gen, node);
            
        case _sbFUNCTION:
            return generate_function(gen, node);
            
        case _sbSTRUCT:
            return generate_struct(gen, node);
            
        case _sbLOAD: {
            ASTNode* modules = node->data.load_stmt.modules;
            if (modules && modules->type == _sbMODULE_LIST) {
                for (int i = 0; i < modules->data.list.count; i++) {
                    ASTNode* module = modules->data.list.items[i];
                    if (module->type == _sbIDENTIFIER) {
                        emit_instruction_with_str(gen, OP_LOAD_MODULE, module->data.str_value);
                    }
                }
            }
            return true;
        }
        
        case _sbGLOBAL: {
            ASTNode* vars = node->data.global_stmt.variables;
            if (vars && vars->type == _sbGLOBAL_LIST) {
                for (int i = 0; i < vars->data.list.count; i++) {
                    ASTNode* var = vars->data.list.items[i];
                    if (var->type == _sbIDENTIFIER) {
                        int idx = register_global(gen, var->data.str_value);
                        if (idx < 0) return false;
                    }
                }
            }
            return true;
        }
        
        case _sbSTRUCT_INSTANTIATION: {
            ASTNode* var = node->data.struct_inst.variable;
            ASTNode* struct_type = node->data.struct_inst.struct_type;
            
            if (var->type == _sbIDENTIFIER && struct_type->type == _sbIDENTIFIER) {
                emit_instruction_with_str(gen, OP_STRUCT_NEW, struct_type->data.str_value);
                emit_instruction_with_str(gen, OP_STORE_VAR, var->data.str_value);
            } else {
                bytecode_error("Invalid struct instantiation");
                return false;
            }
            return true;
        }
        
        case _sbFUNCTION_CALL:
            return generate_expression(gen, node);
        
        default:
            if (node->type >= _sbNUMBER_LITERAL && node->type <= _sbLIST_ACCESS) {
                return generate_expression(gen, node);
            }
            bytecode_error("Unexpected node type in statement: %d", node->type);
            return false;
    }
}

bool generate_block(BytecodeGenerator* gen, ASTNode* node) {
    if (!gen || !node || node->type != _sbBLOCK) return false;
    
    emit_instruction(gen, OP_BLOCK_START);
    
    if (node->data.list.items) {
        for (int i = 0; i < node->data.list.count; i++) {
            if (!generate_statement(gen, node->data.list.items[i])) return false;
        }
    }
    
    emit_instruction(gen, OP_BLOCK_END);
    return true;
}

bool generate_function(BytecodeGenerator* gen, ASTNode* node) {
    if (!gen || !node || node->type != _sbFUNCTION) return false;
    
    ASTNode* name = node->data.function_def.name;
    ASTNode* params = node->data.function_def.parameters;
    ASTNode* body = node->data.function_def.body;
    
    if (!name || name->type != _sbIDENTIFIER) {
        bytecode_error("Invalid function name");
        return false;
    }
    
    size_t param_count = 0;
    if (params && params->type == _sbPARAMETER_LIST) {
        param_count = params->data.list.count;
    }
    
    emit_instruction_with_str(gen, OP_FUNC_START, name->data.str_value);
    
    // Register function after OP_FUNC_START so start_addr points to the first instruction of the function body
    int func_idx = register_function(gen, name->data.str_value, param_count);
    if (func_idx < 0) return false;
    
    if (params && params->type == _sbPARAMETER_LIST) {
        for (int i = params->data.list.count - 1; i >= 0; i--) {
            ASTNode* param = params->data.list.items[i];
            if (param->type == _sbIDENTIFIER) {
                emit_instruction_with_str(gen, OP_STORE_VAR, param->data.str_value);
            }
        }
    }
    
    if (!generate_statement(gen, body)) return false;
    
    emit_instruction(gen, OP_PUSH_NULL);
    emit_instruction(gen, OP_RETURN);
    emit_instruction(gen, OP_FUNC_END);
    
    return true;
}

bool generate_struct(BytecodeGenerator* gen, ASTNode* node) {
    if (!gen || !node || node->type != _sbSTRUCT) return false;
    
    ASTNode* name = node->data.struct_def.name;
    ASTNode* members = node->data.struct_def.members;
    
    if (!name || name->type != _sbIDENTIFIER) {
        bytecode_error("Invalid struct name");
        return false;
    }
    
    DynArray* member_list = create_dynarray();
    if (!member_list) return false;
    
    if (members && members->type == _sbMEMBER_LIST) {
        for (int i = 0; i < members->data.list.count; i++) {
            ASTNode* member = members->data.list.items[i];
            if (member->type == _sbIDENTIFIER) {
                char* member_name = _s_strdup(member->data.str_value);
                dynarray_push(member_list, member_name);
            }
        }
    }
    
    int struct_idx = register_struct(gen, name->data.str_value, member_list);
    if (struct_idx < 0) {
        destroy_dynarray(member_list);
        return false;
    }
    
    emit_instruction_with_str(gen, OP_STRUCT_DEF, name->data.str_value);
    
    return true;
}

bool generate_bytecode(BytecodeGenerator* gen, ASTNode* ast) {
    if (!gen || !ast) return false;
    
    if (ast->type != _sbPROGRAM) {
        bytecode_error("Root node must be PROGRAM");
        return false;
    }
    
    if (ast->data.list.items) {
        for (int i = 0; i < ast->data.list.count; i++) {
            if (!generate_statement(gen, ast->data.list.items[i])) {
                return false;
            }
        }
    }
    
    emit_instruction(gen, OP_HALT);
    return true;
}

void print_bytecode(BytecodeGenerator* gen) {
    if (!gen || !gen->instructions) return;
    
    printf("\n=== Generated Bytecode ===\n");
    printf("Total instructions: %zu\n\n", gen->instructions->count);
    
    for (size_t i = 0; i < gen->instructions->count; i++) {
        Instruction* inst = (Instruction*)dynarray_get(gen->instructions, i);
        if (!inst) continue;
        
        printf("%04zu: ", i);
        
        switch (inst->opcode) {
            case OP_NOP: printf("NOP"); break;
            case OP_PUSH_NUM: printf("PUSH_NUM %.6f", inst->operand.num_value); break;
            case OP_PUSH_STR: printf("PUSH_STR \"%s\"", inst->operand.str_value); break;
            case OP_PUSH_IDENT: printf("PUSH_IDENT %s", inst->operand.str_value); break;
            case OP_PUSH_TRUE: printf("PUSH_TRUE"); break;
            case OP_PUSH_FALSE: printf("PUSH_FALSE"); break;
            case OP_PUSH_NULL: printf("PUSH_nullptr"); break;
            case OP_POP: printf("POP"); break;
            case OP_DUP: printf("DUP"); break;
            case OP_SWAP: printf("SWAP"); break;
            case OP_ADD: printf("ADD"); break;
            case OP_SUB: printf("SUB"); break;
            case OP_MUL: printf("MUL"); break;
            case OP_DIV: printf("DIV"); break;
            case OP_MOD: printf("MOD"); break;
            case OP_POW: printf("POW"); break;
            case OP_BIT_AND: printf("BIT_AND"); break;
            case OP_BIT_OR: printf("BIT_OR"); break;
            case OP_BIT_XOR: printf("BIT_XOR"); break;
            case OP_BIT_NOT: printf("BIT_NOT"); break;
            case OP_BIT_LSHIFT: printf("BIT_LSHIFT"); break;
            case OP_BIT_RSHIFT: printf("BIT_RSHIFT"); break;
            case OP_LOGIC_AND: printf("LOGIC_AND"); break;
            case OP_LOGIC_OR: printf("LOGIC_OR"); break;
            case OP_LOGIC_NOT: printf("LOGIC_NOT"); break;
            case OP_EQ: printf("EQ"); break;
            case OP_NEQ: printf("NEQ"); break;
            case OP_LT: printf("LT"); break;
            case OP_GT: printf("GT"); break;
            case OP_LEQ: printf("LEQ"); break;
            case OP_GEQ: printf("GEQ"); break;
            case OP_ASSIGN: printf("ASSIGN"); break;
            case OP_LOAD_VAR: printf("LOAD_VAR %s", inst->operand.str_value); break;
            case OP_STORE_VAR: printf("STORE_VAR %s", inst->operand.str_value); break;
            case OP_LOAD_GLOBAL: printf("LOAD_GLOBAL %s", inst->operand.str_value); break;
            case OP_STORE_GLOBAL: printf("STORE_GLOBAL %s", inst->operand.str_value); break;
            case OP_JUMP: printf("JUMP %d", inst->operand.int_value); break;
            case OP_JUMP_IF_FALSE: printf("JUMP_IF_FALSE %d", inst->operand.int_value); break;
            case OP_JUMP_IF_TRUE: printf("JUMP_IF_TRUE %d", inst->operand.int_value); break;
            case OP_CALL: printf("CALL %d", inst->operand.int_value); break;
            case OP_RETURN: printf("RETURN"); break;
            case OP_FUNC_START: printf("FUNC_START %s", inst->operand.str_value); break;
            case OP_FUNC_END: printf("FUNC_END"); break;
            case OP_BLOCK_START: printf("BLOCK_START"); break;
            case OP_BLOCK_END: printf("BLOCK_END"); break;
            case OP_LOAD_MODULE: printf("LOAD_MODULE %s", inst->operand.str_value); break;
            case OP_STRUCT_DEF: printf("STRUCT_DEF %s", inst->operand.str_value); break;
            case OP_STRUCT_NEW: printf("STRUCT_NEW %s", inst->operand.str_value); break;
            case OP_MEMBER_ACCESS: printf("MEMBER_ACCESS %s", inst->operand.str_value); break;
            case OP_MEMBER_STORE: printf("MEMBER_STORE %s", inst->operand.str_value); break;
            case OP_LIST_NEW: printf("LIST_NEW %d", inst->operand.int_value); break;
            case OP_LIST_ACCESS: printf("LIST_ACCESS"); break;
            case OP_LIST_STORE: printf("LIST_STORE"); break;
            case OP_LIST_PUSH: printf("LIST_PUSH"); break;
            case OP_HALT: printf("HALT"); break;
            default: printf("UNKNOWN_OP %d", inst->opcode); break;
        }
        printf("\n");
    }
    
    if (gen->functions->count > 0) {
        printf("\n=== Functions ===\n");
        for (size_t i = 0; i < gen->functions->count; i++) {
            FunctionInfo* func = (FunctionInfo*)dynarray_get(gen->functions, i);
            if (func) {
                printf("  %s (params: %zu, addr: %zu)\n", 
                    func->name, func->param_count, func->start_addr);
            }
        }
    }
    
    if (gen->structs->count > 0) {
        printf("\n=== Structs ===\n");
        for (size_t i = 0; i < gen->structs->count; i++) {
            StructInfo* st = (StructInfo*)dynarray_get(gen->structs, i);
            if (st) {
                printf("  %s { ", st->name);
                for (size_t j = 0; j < st->members->count; j++) {
                    char* member = (char*)dynarray_get(st->members, j);
                    if (member) printf("%s ", member);
                }
                printf("}\n");
            }
        }
    }
    
    if (gen->globals->count > 0) {
        printf("\n=== Globals ===\n");
        for (size_t i = 0; i < gen->globals->count; i++) {
            char* global = (char*)dynarray_get(gen->globals, i);
            if (global) printf("  %s\n", global);
        }
    }
}

bool save_bytecode(BytecodeGenerator* gen, const char* filename) {
    if (!gen || !filename) return false;
    
    FILE* file = fopen(filename, "wb");
    if (!file) return false;
    
    // Save instructions
    size_t count = gen->instructions->count;
    fwrite(&count, sizeof(size_t), 1, file);
    
    for (size_t i = 0; i < count; i++) {
        Instruction* inst = (Instruction*)dynarray_get(gen->instructions, i);
        if (inst) {
            fwrite(&inst->opcode, sizeof(OpCode), 1, file);
            
            switch (inst->opcode) {
                case OP_PUSH_NUM:
                    fwrite(&inst->operand.num_value, sizeof(double), 1, file);
                    break;
                case OP_PUSH_STR:
                case OP_PUSH_IDENT:
                case OP_LOAD_VAR:
                case OP_STORE_VAR:
                case OP_LOAD_GLOBAL:
                case OP_STORE_GLOBAL:
                case OP_LOAD_MODULE:
                case OP_FUNC_START:
                case OP_STRUCT_DEF:
                case OP_STRUCT_NEW:
                case OP_MEMBER_ACCESS:
                case OP_MEMBER_STORE: {
                    size_t len = strlen(inst->operand.str_value) + 1;
                    fwrite(&len, sizeof(size_t), 1, file);
                    fwrite(inst->operand.str_value, 1, len, file);
                    break;
                }
                case OP_JUMP:
                case OP_JUMP_IF_FALSE:
                case OP_JUMP_IF_TRUE:
                case OP_CALL:
                case OP_LIST_NEW:
                    fwrite(&inst->operand.int_value, sizeof(int), 1, file);
                    break;
                default:
                    break;
            }
        }
    }
    
    // Save functions metadata
    count = gen->functions->count;
    fwrite(&count, sizeof(size_t), 1, file);
    for (size_t i = 0; i < count; i++) {
        FunctionInfo* func = (FunctionInfo*)dynarray_get(gen->functions, i);
        if (func) {
            size_t len = strlen(func->name) + 1;
            fwrite(&len, sizeof(size_t), 1, file);
            fwrite(func->name, 1, len, file);
            fwrite(&func->start_addr, sizeof(size_t), 1, file);
            fwrite(&func->param_count, sizeof(size_t), 1, file);
        }
    }
    
    // Save structs metadata
    count = gen->structs->count;
    fwrite(&count, sizeof(size_t), 1, file);
    for (size_t i = 0; i < count; i++) {
        StructInfo* st = (StructInfo*)dynarray_get(gen->structs, i);
        if (st) {
            size_t len = strlen(st->name) + 1;
            fwrite(&len, sizeof(size_t), 1, file);
            fwrite(st->name, 1, len, file);
            
            size_t member_count = st->members->count;
            fwrite(&member_count, sizeof(size_t), 1, file);
            for (size_t j = 0; j < member_count; j++) {
                char* member = (char*)dynarray_get(st->members, j);
                if (member) {
                    len = strlen(member) + 1;
                    fwrite(&len, sizeof(size_t), 1, file);
                    fwrite(member, 1, len, file);
                }
            }
        }
    }
    
    // Save globals metadata
    count = gen->globals->count;
    fwrite(&count, sizeof(size_t), 1, file);
    for (size_t i = 0; i < count; i++) {
        char* global = (char*)dynarray_get(gen->globals, i);
        if (global) {
            size_t len = strlen(global) + 1;
            fwrite(&len, sizeof(size_t), 1, file);
            fwrite(global, 1, len, file);
        }
    }
    
    fclose(file);
    return true;
}

BytecodeGenerator* load_bytecode(const char* filename) {
    if (!filename) return nullptr;
    
    FILE* file = fopen(filename, "rb");
    if (!file) return nullptr;
    
    BytecodeGenerator* gen = create_bytecode_generator();
    if (!gen) {
        fclose(file);
        return nullptr;
    }
    
    // Load instructions
    size_t count;
    if (fread(&count, sizeof(size_t), 1, file) != 1) {
        destroy_bytecode_generator(gen);
        fclose(file);
        return nullptr;
    }
    
    for (size_t i = 0; i < count; i++) {
        OpCode opcode;
        if (fread(&opcode, sizeof(OpCode), 1, file) != 1) break;
        
        Instruction* inst = (Instruction*)malloc(sizeof(Instruction));
        if (!inst) break;
        
        inst->opcode = opcode;
        
        switch (opcode) {
            case OP_PUSH_NUM:
                fread(&inst->operand.num_value, sizeof(double), 1, file);
                break;
            case OP_PUSH_STR:
            case OP_PUSH_IDENT:
            case OP_LOAD_VAR:
            case OP_STORE_VAR:
            case OP_LOAD_GLOBAL:
            case OP_STORE_GLOBAL:
            case OP_LOAD_MODULE:
            case OP_FUNC_START:
            case OP_STRUCT_DEF:
            case OP_STRUCT_NEW:
            case OP_MEMBER_ACCESS:
            case OP_MEMBER_STORE: {
                size_t len;
                fread(&len, sizeof(size_t), 1, file);
                inst->operand.str_value = (char*)malloc(len);
                fread(inst->operand.str_value, 1, len, file);
                break;
            }
            case OP_JUMP:
            case OP_JUMP_IF_FALSE:
            case OP_JUMP_IF_TRUE:
            case OP_CALL:
            case OP_LIST_NEW:
                fread(&inst->operand.int_value, sizeof(int), 1, file);
                break;
            default:
                inst->operand.int_value = 0;
                break;
        }
        
        dynarray_push(gen->instructions, inst);
        gen->current_addr++;
    }

    // Load functions metadata
    if (fread(&count, sizeof(size_t), 1, file) == 1) {
        for (size_t i = 0; i < count; i++) {
            size_t len;
            if (fread(&len, sizeof(size_t), 1, file) != 1) break;
            
            FunctionInfo* func = (FunctionInfo*)malloc(sizeof(FunctionInfo));
            if (!func) break;
            
            func->name = (char*)malloc(len);
            if (fread(func->name, 1, len, file) != len) {
                free(func->name);
                free(func);
                break;
            }
            
            if (fread(&func->start_addr, sizeof(size_t), 1, file) != 1 ||
                fread(&func->param_count, sizeof(size_t), 1, file) != 1) {
                free(func->name);
                free(func);
                break;
            }
            
            dynarray_push(gen->functions, func);
        }
    }
    
    // Load structs metadata
    if (fread(&count, sizeof(size_t), 1, file) == 1) {
        for (size_t i = 0; i < count; i++) {
            size_t len;
            if (fread(&len, sizeof(size_t), 1, file) != 1) break;
            
            StructInfo* st = (StructInfo*)malloc(sizeof(StructInfo));
            if (!st) break;
            
            st->name = (char*)malloc(len);
            if (fread(st->name, 1, len, file) != len) {
                free(st->name);
                free(st);
                break;
            }
            
            st->members = create_dynarray();
            if (!st->members) {
                free(st->name);
                free(st);
                break;
            }
            
            size_t member_count;
            if (fread(&member_count, sizeof(size_t), 1, file) != 1) {
                destroy_dynarray(st->members);
                free(st->name);
                free(st);
                break;
            }
            
            bool member_failed = false;
            for (size_t j = 0; j < member_count; j++) {
                if (fread(&len, sizeof(size_t), 1, file) != 1) {
                    member_failed = true;
                    break;
                }
                
                char* member = (char*)malloc(len);
                if (!member || fread(member, 1, len, file) != len) {
                    if (member) free(member);
                    member_failed = true;
                    break;
                }
                
                dynarray_push(st->members, member);
            }
            
            if (member_failed) {
                for (size_t k = 0; k < st->members->count; k++) {
                    char* member = (char*)dynarray_get(st->members, k);
                    if (member) free(member);
                }
                destroy_dynarray(st->members);
                free(st->name);
                free(st);
                break;
            }
            
            dynarray_push(gen->structs, st);
        }
    }
    
    // Load globals metadata
    if (fread(&count, sizeof(size_t), 1, file) == 1) {
        for (size_t i = 0; i < count; i++) {
            size_t len;
            if (fread(&len, sizeof(size_t), 1, file) != 1) break;
            
            char* global = (char*)malloc(len);
            if (!global || fread(global, 1, len, file) != len) {
                if (global) free(global);
                break;
            }
            
            dynarray_push(gen->globals, global);
        }
    }

    fclose(file);
    return gen;
}