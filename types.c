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
        printf("array[%i] of ", type->size);
        print_type(type->of_type);
        break;
     case POINTER_TYPE:
        printf("pointer to ");
        print_type(type->to_type);
        break;
     case FUNCTION_TYPE:
        // TODO: also print parameter types
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
        // TODO: also check parameter types
        return types_match(t1->return_type, t2->return_type);
    }
}

/**
 * Check if a type is numeric. So far, only char and int.
 * @param t the type to check
 * @return 1 if the type is numeric, else 0
*/
int is_numeric(struct Type *t) {
    return (t->kind == PRIMITIVE_TYPE) && (t->primitive == INT_PRIMITIVE || t->primitive == CHAR_PRIMITIVE);
}


/**
 * Allocate a new type of the given kind
 * @param kind the kind of type (pointer, array, primitive, function, ) 
 * @return a pointer to the new type
 */
struct Type *new_type(enum TypeKind kind) {
    struct Type *type = malloc(sizeof(struct Type));
    type->kind = kind;

    return type;
} 


/**
 * Check if we can automatically convert one type into another (e.g., for binary expressions).
 * Assumes from_type and to_type are different (i.e., can't "autoconvert" from int to int);
 * the caller is responsible for ensuring this holds.
 * @param from_type the type we want to convert from
 * @param to_type the type we want to convert to
 * @return 1 if we can convert from_type to to_type, otherwise 0
*/
int can_autoconvert(struct Type *from_type, struct Type *to_type) {
    if (from_type != NULL && to_type != NULL) {
        switch (from_type->kind) {
        case PRIMITIVE_TYPE:
            if ((from_type->primitive == INT_PRIMITIVE && to_type->primitive == CHAR_PRIMITIVE) 
            || (from_type->primitive == CHAR_PRIMITIVE && to_type->primitive == INT_PRIMITIVE))
                return 1;
        default:
            break;
        }
    }

    return 0;
}


/**
 * Find the type of a binary expression with operands of two (possibly different) type.
 * The type of the expression is the type of the operand with higher rank. 
 * For now, ranks are simple:
 * char < int 
 * @param t1 the type of the first operand
 * @param t2 the tpye of the second operand
 * @return the type of the expression t1 [op] t2
*/
struct Type *get_binary_expression_type(struct Type *t1, struct Type *t2) {
    if (!(is_numeric(t1) && is_numeric(t2)))
        return &ERROR_TYPE;

    if (t1->primitive == INT_PRIMITIVE || t2->primitive == INT_PRIMITIVE)
        return &INT_TYPE;
    else
        return &CHAR_TYPE; 
}


/**
 * Compute the total size of a type. For arrays, this is the length * the size
 * of each eleement.
 * @param t the type whose size to compute
 * @return the number of bytes t occupies in memory
*/
int get_memory_size(struct Type *t) {
    if (t->kind == ARRAY_TYPE) {
        return t->size * get_memory_size(t->of_type);
    } else {
        return t->size;
    }
}