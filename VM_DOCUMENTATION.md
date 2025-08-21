/*
 * SB Language - 字节码生成器和虚拟机文档
 * 
 * 系统架构概述
 * =============
 * 
 * SB语言编译和执行流程：
 * 1. 词法分析（Lexer）: 将源代码转换为token序列
 * 2. 语法分析（Parser）: 将token序列转换为抽象语法树（AST）
 * 3. 字节码生成（Bytecode Generator）: 将AST转换为字节码指令
 * 4. 虚拟机执行（VM）: 执行字节码指令
 * 
 * =====================================
 * 字节码生成器（Bytecode Generator）
 * =====================================
 * 
 * 核心组件：
 * ---------
 * 1. 指令数组（instructions）: 存储生成的字节码指令
 * 2. 常量池（constants）: 存储程序中的常量值
 * 3. 函数表（functions）: 存储函数定义信息
 * 4. 结构体表（structs）: 存储结构体定义
 * 5. 全局变量表（globals）: 记录全局变量名称
 * 
 * 主要功能：
 * ---------
 * - AST遍历和指令生成
 * - 跳转地址的占位和回填
 * - 符号表管理（函数、结构体、全局变量）
 * - 字节码序列化和反序列化
 * 
 * 指令格式：
 * ---------
 * 每条指令由操作码（OpCode）和可选的操作数组成
 * 操作数类型包括：数字、字符串、整数（跳转地址）
 * 
 * =====================================
 * 虚拟机（Virtual Machine）
 * =====================================
 * 
 * 核心组件：
 * ---------
 * 1. 操作数栈（stack）: 用于计算和传递值，大小为1024
 * 2. 调用栈（call_stack）: 管理函数调用，深度限制256
 * 3. 程序计数器（pc）: 指向当前执行的指令
 * 4. 变量表：
 *    - 全局变量表（globals）
 *    - 局部变量表（locals）
 * 5. 函数和结构体定义表
 * 
 * 值类型系统：
 * -----------
 * - VAL_NULL: 空值
 * - VAL_NUMBER: 数字（double）
 * - VAL_STRING: 字符串
 * - VAL_BOOL: 布尔值
 * - VAL_FUNCTION: 函数
 * - VAL_NATIVE: 原生函数
 * - VAL_STRUCT: 结构体定义
 * - VAL_STRUCT_INSTANCE: 结构体实例
 * - VAL_LIST: 列表
 * 
 * 指令执行：
 * ---------
 * VM通过解释执行字节码指令，支持：
 * - 算术运算：ADD, SUB, MUL, DIV, MOD, POW
 * - 位运算：AND, OR, XOR, NOT, LSHIFT, RSHIFT
 * - 逻辑运算：AND, OR, NOT
 * - 比较运算：EQ, NEQ, LT, GT, LEQ, GEQ
 * - 控制流：JUMP, JUMP_IF_FALSE, JUMP_IF_TRUE
 * - 函数调用：CALL, RETURN
 * - 变量操作：LOAD_VAR, STORE_VAR
 * - 结构体操作：STRUCT_DEF, STRUCT_NEW, MEMBER_ACCESS, MEMBER_STORE
 * - 列表操作：LIST_NEW, LIST_ACCESS, LIST_STORE, LIST_PUSH
 * - 模块加载：LOAD_MODULE
 * 
 * 模块系统：
 * ---------
 * - 支持动态加载.sb文件
 * - 模块在同一VM上下文中执行
 * - 继承主程序的栈和变量环境
 * - 完整的编译流程：源码 -> AST -> 字节码 -> 执行
 * 
 * 原生函数接口：
 * ------------
 * - 支持注册C函数作为原生函数
 * - 原生函数签名：Value (*)(VM*, Value*, int)
 * - 内置函数：print, input, tonumber, tostring
 * 
 * 错误处理：
 * ---------
 * VM支持多种错误类型：
 * - 运行时错误（RuntimeError）
 * - 栈溢出/下溢（StackOverflow/Underflow）
 * - 未定义变量/函数（UndefinedVariable/Function）
 * - 类型错误（TypeError）
 * - 除零错误（DivisionByZero）
 * - 索引越界（IndexOutOfBounds）
 * - 加载错误（LoadError）
 * - 内存错误（MemoryError）
 * 
 * 内存管理：
 * ---------
 * - 所有动态分配的内存都有对应的释放机制
 * - 字符串值采用深拷贝策略
 * - VM销毁时自动清理所有资源
 * - 支持垃圾回收框架（预留接口）
 * 
 * =====================================
 * 使用示例
 * =====================================
 * 
 * 1. 编译源码到字节码：
 *    ```c
 *    _sbToken* tokens = _sbLexer(source);
 *    Parser* parser = create_tkstate(tokens);
 *    ASTNode* ast = parse_program(parser);
 *    BytecodeGenerator* gen = create_bytecode_generator();
 *    generate_bytecode(gen, ast);
 *    ```
 * 
 * 2. 执行字节码：
 *    ```c
 *    VM* vm = create_vm();
 *    vm_register_native(vm, "print", native_print);
 *    vm_load_bytecode(vm, gen);
 *    vm_execute(vm);
 *    destroy_vm(vm);
 *    ```
 * 
 * 3. 保存和加载字节码：
 *    ```c
 *    save_bytecode(gen, "program.sbc");
 *    BytecodeGenerator* loaded = load_bytecode("program.sbc");
 *    ```
 * 
 * =====================================
 * 性能和限制
 * =====================================
 * 
 * - 操作数栈大小：1024个值
 * - 调用栈深度：256层
 * - 最大全局变量数：256个
 * - 动态数组初始容量：8，按需翻倍扩展
 * - 字节码文件格式：二进制，平台相关
 * 
 * =====================================
 * 未来扩展
 * =====================================
 * 
 * - 完善垃圾回收机制
 * - 添加JIT编译支持
 * - 优化指令集设计
 * - 支持调试器接口
 * - 添加更多内置函数和类型
 * - 实现字节码验证器
 * - 支持多线程执行
 * 
 */