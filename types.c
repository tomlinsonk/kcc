#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "types.h"


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
        print_type(type->of_type);
        break;
     case POINTER_TYPE:
        printf("pointer to ");
        print_type(type->to_type);
        break;
     case FUNCTION_TYPE:
        printf("function returning ");
        print_type(type->return_type);
        break;
    default:
        break;
    }
}

int types_match(struct Type *t1, struct Type *t2) {
    if (t1 == t2)
        return 1;
    else if (t1->kind != t2->kind)
        return 0;

    switch (t1->kind) {
    case PRIMITIVE_TYPE:
        return t1->primitive == t2->primitive;
    case ARRAY_TYPE:
        return types_match(t1->of_type, t2->of_type);
    case POINTER_TYPE:
        return types_match(t1->to_type, t2->to_type);
    case FUNCTION_TYPE:
        return types_match(t1->return_type, t2->return_type);
    }
}