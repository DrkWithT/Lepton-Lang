## README

### Brief
My experiment of a more expression-oriented, terse, and dynamic scripting language. Written in C23 with extensions dialect and might be used by myself if it matures enough.

### Requirements
 - Recent Clang / GCC compiler version
 - CMake 3.31
 - Ninja build tool (CMake presets need this)

### To Do's
 1. Test parser. **WIP**
 2. Add bytecode types.
    - Register-based opcodes. **OK**
    - Create Shapes and ICs. **OK**
 3. Add generic "pools" for objects. **OK**
 4. Make bytecode compiler & test it. **WIP**
 5. Make driver encapsulation for program. **OK**
 6. Make VM using Shapes and ICs.
    - Refactor things to use a unified "driver" interface. **OK**
    - Add `assert(src_value, msg)` native to driver. **OK**
 7. Refactor in upvalues.
    - Make `upvalue`. **OK**
    - Each closure (each function is one) captures open upvalues at 1st. **OK**
    - Fix bcc and bytecode to support upvalues: remove var_scope and add upvalue set/get/close. **OK**
    - Fix context and then dispatcher to support new behavior. Add call frames. **OK**
    - Before a capturing function returns, it closes upvalues: the reference-holding `value` is moved into a concrete `value` per upvalue. **OK**
    - Test VM on toy scripts.
 8. Add GC.
 9. Test further on larger scripts.

### Other Notes
The first few commits are larger since I forgot to create the repo initially & I had to compensate with retroactive commits. No AI was used.
