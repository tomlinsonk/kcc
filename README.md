# The KiT C Compiler
This is a (WIP) C compiler designed for a specific 65C02 target: the KiT. The primary goal of this project is to learn how compilers work and get practice with C.
It would be great to be able to write KiT programs in C and then compile them to 65C02 assembly, but I don't anticipate being able to write a complete C compiler.
I'll just keep expanding it until I'm happy with the state of the compiler.

#### ~~Milestone 1: basic program~~ Completed!

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
6. ~~Emitting 6502 assembly~~
    - ~~Stack allocation~~
    - ~~Integer constants~~
    - ~~Assignment~~
    - ~~Addition~~
    - ~~Return~~


#### ~~Milestone 2: Collatz~~ Completed!
Once I can compile and run the barebones program above, I want to focus on adding control flow. The next program I'll work towards compiling counts the length of the Collatz sequence starting at 27 ($1b), which should return 111 ($6f):
```C
int main() {
    int n;
    int i;

    n = 27;
    i = 0;

    while (n != 1) {
        i++;
        if (n << 15 == 0) {       // avoiding modulus
            n = n >> 1;           // avoiding division
        } else {
            n = n + n + n + 1;    // avoiding multiplication
        }
    }

    return i;
}
```

Steps towards Milestone 2. Need to be able to emit:
1. ~~Comparisons == and !=~~
2. ~~Unary operators ++ and --~~
3. ~~Bitshifts >> and <<~~
    - ~~one byte >>~~
    - ~~two byte >>~~
    - ~~one byte <<~~
    - ~~two byte <<~~
4. ~~If () else  conditionals~~
    - ~~Parsing~~
    - ~~Emitting~~
5. ~~While loops~~
    - ~~Parsing~~
    - ~~Emitting~~

#### ~~Milestone 2.5: Arithmetic (Collatz, but elegant)~~ Completed!
The next incremental step for the compiler is to flesh out its arithmetic abilities.
In particular, I want to add -, *, /, and %. This will allow me to compile the 
Collatz program, but without all the hacky modifications:

```C
int main() {
    int n;
    int i;

    n = 27;
    i = 0;

    while (n != 1) {
        i++;
        if (n % 2 == 0) {       
            n = n / 2;  
        } else {
            n = 3 * n + 1;
        }
    }

    return i;
}
```
Steps towards Milestone 2.5:
1. ~~Subtraction~~
2. ~~Multiplication~~
3. ~~Integer division~~
    - ~~8 bit unsigned~~
    - ~~8 bit signed~~
    - ~~16 bit unsigned~~
    - ~~16 bit signed~~
4. ~~Modulus~~

#### Milestone 3: Sorting
Time to go big! I want to compile insertion sort:
```C

void print_arr(int arr[], int n) {
    for (int i = 0; i < n; i++) {
        putchar('0' + i);
        putchar(' ');
    }
    putchar('\n')
}

void insertion_sort(int arr[], int n) {
    int tmp;
    int j;

    for (int i = 1; i < n; i++) {
        j = i;
        while (j > 0 && arr[j-1] > arr[j]) {
            tmp = arr[j];
            arr[j] = arr[j-1];
            arr[j-1] = tmp;
            j--;
        }
    }
}

int main() {
    int arr[10];
    int n;

    n = 10;

    for (int i = 0; i < n; i++) {
        arr[i] = (7 * i) % 10;
    }

    print_arr(arr, n);
    insertion_sort(arr, n);
    print_arr(arr, n);
}
```

Steps towards Milestone 3:
- ~~Static arrays~~
    -~~Declaration~~
    - ~~Indexing~~
        - ~~Assigning to arrays~~
        - ~~Retrieving from arrays~~
- ~~For loops~~
- ~~Comparisons: `<`, `>`, `<=`, `>=`~~
- ~~Binary logical operators: `&&`, `||`, with short-circuiting~~
- ~~Unary logical operator: `!` ~~
- Function calling
- Function parameters
- char literals
- `putchar`


## Design details
Recall, the 6502 is little-endian (lower order bytes in smaller address)

### Types
- `char`: 8-bit unsigned integer
- `int`: 16-bit signed integer   
- ...

Expressions automatically convert between `char` and `int` (taking the bottom byte or extending with a zero byte). 


### Function calling
#### Stack 
The CPU stack is only 256 bytes, so C function calling is implemented in a software stack with top at $5fff. This stack is referred to as the *C stack*. Two zero page pointers are used to store the stack pointer (sp) and frame pointer (fp). 

#### Stack frames
From top to bottom, a stack frame consists of:
1. arguments (if any) 1, 2, ..., *n*
2. caller frame pointer high byte
3. caller frame pointer low byte
4. return address - 1 low byte
5. return address - 1 high byte
6. callee locals

The first five bullets are set by the caller. Note that 1 is subtracted from the return address so that an `rts` can be used to jump to the correct return address.

The frame pointer fp points to the current frame, starting at item 6 (callee locals). This is used to have a consistent offset to access locals. The current frame pointer is set by the callee when it is called.  

#### Register use
The A, X, and Y registers may all be overwritten by the callee.

#### Returning
The callee places its return value in the following locations:
- `char`: A register
- `int`: A register (low byte), X register (high byte) 

The callee then pops its locals off the C stack, pops the return address - 1 off the stack, restores the caller's frame pointer (popping it), and jumps to the return address. The caller must then pop the arguments off the stack. 

### Entry and exit
Every program must have function `int main()`. To enter the program, the executable performs the call `main()`. After this call completes, the executable issues a `brk` to return to the system monitor. The return code of the program is then accessible in A (low byte), X (high byte). 

### Expressions
For simplicity, all expressions are evaluated using the stack. Operands are pushed onto the stack (lhs then rhs) and then popped when an operator is evaluated. The result is then pushed back onto the stack, allowing easy evaluation of nested expressions.


## Supported features
- `int` and `char` types
- Assignment expressions (using `=`)
- Binary operators: `+`, `-`, `==`, `!=`, `>>`, `<<`, `*`, `/`, `%`, `>`, `<`, `<=`, `>=`, `&&`, `||` 
- Unary operators: `+`, `-`, `!`,
- Postfix operators: `++`, `--`
- Return statement
- While loops
- For loops
- If and else-if
- Arrays

## Todo
- Functions: parameters, function calling
- Binary operators:  `^`, `|`, `&`, 
- Unary operators: `*`, `~`, `++`, `--`
- Assigment operators `*=`, `+=`, `-=`, `/=`, `%=`, `<<=`, `>>=`, `&=`, `|=`, `^=`
- Do-while loops
- Switch statements
- Labeled statements
- Initializers
- Pointers
- Structs
- Enums
- Unions
- Floats
- Strings

