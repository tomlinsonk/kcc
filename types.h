#ifndef TYPES
#define TYPES



enum TypeKind {
    PRIMITIVE_TYPE,
    POINTER_TYPE,
    ARRAY_TYPE,
    FUNCTION_TYPE
};


static const char *primitive_type_names[] = {"error", "null", "int", "char"};


enum PrimitiveType {
    ERROR_PRIMITIVE,
    NULL_PRIMITIVE,
    INT_PRIMITIVE,
    CHAR_PRIMITIVE
};

struct PointerType {
    struct Type *to_type;
};

struct ArrayType {
    struct Type *of_type;
};

struct FunctionType {
    struct Type *return_type;
    struct Argument *first_arg;
    int n_args;
};

struct Argument {
    struct Argument *next_arg;
    struct Type *type;
};

struct Type {
    enum TypeKind kind;
    union {
        enum PrimitiveType primitive;
        struct PointerType pointer;
        struct ArrayType array;
        struct FunctionType function;
    };
};

static struct Type INT_TYPE = {.kind=PRIMITIVE_TYPE, .primitive=INT_PRIMITIVE};
static struct Type CHAR_TYPE = {.kind=PRIMITIVE_TYPE, .primitive=CHAR_PRIMITIVE};
static struct Type NULL_TYPE = {.kind=PRIMITIVE_TYPE, .primitive=NULL_PRIMITIVE};
static struct Type ERROR_TYPE = {.kind=PRIMITIVE_TYPE, .primitive=ERROR_PRIMITIVE};


void print_type(struct Type *type);

int types_match(struct Type *t1, struct Type *t2);

#endif
