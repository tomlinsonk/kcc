# The KiT C Compiler
This is a (WIP) C compiler designed for a specific 65C02 target: the KiT. The primary goal of this project is to learn how compilers work and get practice with C.
It would be great to be able to write KiT programs in C and then compile then to 65C02 assembly, but I don't anticipate being able to write a complete C compiler.
I'll just keep expanding it until I'm happy with the state of the compiler.

#### Milestone 1
As a small end-to-end goal, I want to be able to compile and run the following C program on the KiT:

```C
int main() {
    int x;
    int y;

    x = 42;
    y = 77;
    return x + y;
}
```
This will require a few basic features:
1. ~~Parsing declarations (skip initializer for now)~~
2. ~~SymbolTable and identifier binding~~
3. ~~Parsing function definitions (skip parameters for now) and return statements~~ 
4. ~~Binding function names~~ 
5. ~~Typechecking function return type~~ 
6. Emitting 6502 assembly
    - Heap and stack allocation
    - Integer constants
    - Assignment
    - Addition
    - Return


#### Milestone 2
Once I can compile and run the barebones program above, I want to focus on adding control flow. The next program I'll work towards compiling counts the length of the Collatz sequence starting at 27, which should return 111:
```C
int main() {
    int n = 27;
    int i = 0;

    while (n != 1) {
        i++;
        if (n % 2 == 0) {
            n = n >> 1;           // avoiding division
        } else {
            n = n + n + n + 1;    // avoiding multiplication
        }
    }

    return i;
}
```

## Design details

### Types
- `char`: 8-bit unsigned integer
- `int`: 16-bit signed integer   
- ...

### Register layout
Values are loaded in the following locations:
#### First value
- `char`: A register
- `int`: A register (low byte), X register (high byte) 

### Function calling
#### Stack 
The CPU stack is only 256 bytes, so C function calling is implemented in a software stack with top at $5fff. This stack is referred to as the *C stack*. 

#### Stack frames
From top to bottom, a stack frame consists of:
- arguments (if any) 1, 2, ..., *n*
- return address - 1 high byte
- return address - 1 low byte
- callee locals

The first three bullets are set by the caller. Note that 1 is subtracted from the return address so that an `rts` can be used to jump to the correct return address.

#### Register use
The A, X, Y and registers may all be overwritten by the callee.

#### Returning
The callee places its return value in the following locations:
- `char`: A register
- `int`: A register (low byte), X register (high byte) 

The callee then pops its locals off the C stack, pops the return address - 1 off the stack, and jumps to the return address. The caller must then pop the arguments off the stack

### Entry and exit
Every program must have function `int main()`. To enter the program, the executable performs the call `main()`. After this call completes, the executable issues a `brk` to return to the system monitor. The return code of the program is then accessible in A (low byte), X (high byte). 

### Expressions
For simplicity, all experessions are evaluated using the stack. Operands are pushed onto the stack (lhs then rhs) and then popped when an operator is evaluated. The result is then pushed back onto the stack, allowing easy evaluation of nested expressions.