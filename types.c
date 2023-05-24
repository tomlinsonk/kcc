#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "types.h"



// struct Type CHAR_TYPE;
// CHAR_TYPE.kind = PRIMITIVE_TYPE;
// CHAR_TYPE.primitive = _PRIMITIVE;

// struct Type NULL_TYPE;
// INULL_TYPENT_TYPE.kind = PRIMITIVE_TYPE;
// NULL_TYPE.primitive = NULL_PRIMITIVE;



void print_type(struct Type *type) {
    if (type == NULL) {
        printf("null");
        return;
    }

    switch (type->kind) {
    case PRIMITIVE_TYPE:
        printf("%s", primitive_type_names[type->primitive]);
        break;
     case ARRAY_TYPE:
        printf("array of ");
        print_type(type->array.of_type);
        break;
     case POINTER_TYPE:
        printf("pointer to ");
        print_type(type->pointer.to_type);
        break;
     case FUNCTION_TYPE:
        printf("function: (...) -> ");
        print_type(type->function.return_type);
        break;
    default:
        break;
    }
}

int types_match(struct Type *t1, struct Type *t2) {
    // TODO
    return 1;
}