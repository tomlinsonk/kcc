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
5. ~~ Typechecking function return type~~ 
6. Generating 6502 assembly
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
            n = n >> 1;
        } else {
            n = n + n + n + 1;    // avoiding multiplication
        }
    }
    return i;
}
```