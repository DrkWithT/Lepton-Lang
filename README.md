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
 8. Add objects.
    - Add parsing for `new` expressions that may use traits. Support exotic objects with `new {...}`. **OK**
    - Add BCC support for new expressions. **OK**
    - Add helper methods for context to support object opcodes.
    - Add dispatch table entries of and implement handlers for opcodes in VM.
    - Add `println` function.
    - Test.
 9. Add module support.
    - Add parsing support for modules which combines all their ASTs into 1 TU & stores sources to IDs... Each module knows its source.
      - Also support `module "name";` decls.
    - Refactor compilation logic for modules:
      - Fix AST unit to support source map and source IDs per AST node.
      - Fix BCC to handle source mappings.
 10. Add GC.
 11. Add polyfills for `List`.
 12. Test further on larger scripts.

### Other Notes
The first few commits are larger since I forgot to create the repo initially & I had to compensate with retroactive commits. No AI was used.
