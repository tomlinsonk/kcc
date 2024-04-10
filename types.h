/**
 * @file types.h
 * @author Kiran Tomlinson
 * @date 29 May 2023
 * @brief Header: provides Type, related enums, static types and related declarations 
 */

#ifndef TYPES
#define TYPES


enum TypeKind {
    PRIMITIVE_TYPE,
    POINTER_TYPE,
    ARRAY_TYPE,
    FUNCTION_TYPE
};

static const char *primitive_type_names[] = {"error", "void", "int", "char"};

enum PrimitiveType {
    ERROR_PRIMITIVE,
    VOID_PRIMITIVE,
    INT_PRIMITIVE,
    CHAR_PRIMITIVE
};

struct Type {
    enum TypeKind kind;
    union {
        enum PrimitiveType primitive;
        struct Type *to_type;
        struct Type *of_type;
        struct Type *return_type;
    };
    struct Type *next_param_type;
    int size;
};

static struct Type INT_TYPE = {.kind=PRIMITIVE_TYPE, .primitive=INT_PRIMITIVE, .size=2};
static struct Type CHAR_TYPE = {.kind=PRIMITIVE_TYPE, .primitive=CHAR_PRIMITIVE, .size=1};
static struct Type VOID_TYPE = {.kind=PRIMITIVE_TYPE, .primitive=VOID_PRIMITIVE, .size=-1};
static struct Type ERROR_TYPE = {.kind=PRIMITIVE_TYPE, .primitive=ERROR_PRIMITIVE, .size=-1};

static struct Type CHAR_ARRAY_TYPE = {.kind=ARRAY_TYPE, .of_type=&CHAR_TYPE};
static struct Type INT_ARRAY_TYPE = {.kind=ARRAY_TYPE, .of_type=&INT_TYPE};
static struct Type MAIN_FUNCTION_TYPE = {.kind=FUNCTION_TYPE, .return_type=&INT_TYPE};


void print_type(struct Type *type);

struct Type *new_type(enum TypeKind kind);
int types_match(struct Type *t1, struct Type *t2);
int can_autoconvert(struct Type *from_type, struct Type *to_type);
struct Type *get_binary_expression_type(struct Type *t1, struct Type *t2);
int is_numeric(struct Type *t);
int get_memory_size(struct Type *t);

#endif
