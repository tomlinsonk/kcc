/**
 * @file types.c
 * @author Kiran Tomlinson
 * @date 28 May 2023
 * @brief Provides type representations and functions  
 * 
 * This file provides an implementation of types, type comparisons, and 
 * helper methods (like printing). Compound types like arrays of pointer to
 * char are represented as a linked chain of Type nodes. 
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "types.h"

/**
 * Print a type nicely, recursively; for instance:
 * function returning pointer to array of char
 * @param type a pointer to the type to print
 */
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

/**
 * Check if two types are the same, recursively
 * @param t1 pointer to first type
 * @param t2 pointer to second type
 * @return 1 if t1 matches t2, 0 otherwise
 */
int types_match(struct Type *t1, struct Type *t2) {
    if (t1 == t2)
        return 1;
    else if (t1 == NULL || t2 == NULL || t1->kind != t2->kind)
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


/**
 * Allocate a new type of the given kind
 * @param kind the kind of type (pointer, array, primitive, function, ) 
 * @return a pointer to the new type
 */
struct Type *new_type(enum TypeKind kind) {
    struct Type *type = malloc(sizeof(type));
    type->kind = kind;

    return type;
} 