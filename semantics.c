/**
 * @file semantics.c
 * @author Kiran Tomlinson
 * @date 28 May 2023
 * @brief Checks a syntax tree for sematic correctness (e.g., typechecking)  
 * 
 * This file provides functions to typecheck a syntax tree. Eventually, will 
 * also check symbol definitions and scopes. 
 */

#define BOLD_RED    "\033[1m\033[31m"
#define RESET       "\033[0m"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "semantics.h"
#include "parser.h"
#include "lexer.h"
#include "types.h"

int num_type_errors = 0;

/**
 * Recursively typecheck a syntax tree. Annotates the nodes with their types.
 * Also logs the number of type errors encountered in num_type_errors
 * @param root a pointer to the root node of the tree to typecheck
 */
void typecheck(struct SyntaxNode *root) {
    struct SyntaxNode *cur_node;

    switch (root->syntax){
    case ASSIGNMENT_EXPRESSION_SYNTAX:
        typecheck_assignment_expression(root);
        break;
    case BINARY_EXPRESSION_SYNTAX:
        typecheck_binary_expression(root);
        break;
    case UNARY_EXPRESSION_SYNTAX:
        typecheck_unary_expression(root);
        break;
    case INT_CONSTANT_SYNTAX:
        root->type = &INT_TYPE;
        break;
    case STRING_SYNTAX:
        root->type = &CHAR_ARRAY_TYPE;
        break;
    case IDENTIFIER_SYNTAX:
        // TEMPORARY: TODO: symbol tables
        root->type = &INT_IDENTIFIER_TYPE;
        break;
    case PARENTHESIZED_EXPRESSION_SYNTAX:
        typecheck(root->first_child);
        root->type = root->first_child->type;
        break;
    case DECLARATION_SYNTAX:
    // TODO: typecheck declarations
    case DECLARATION_LIST_SYNTAX:
    case STATEMENT_LIST_SYNTAX:
    case COMPOUND_STATEMENT_SYNTAX:
        root->type = &VOID_TYPE;
        cur_node = root->first_child;
        while (cur_node != NULL) {
            typecheck(cur_node);
            cur_node = cur_node->next;
        }
        break;
    case EXPRESSION_STATEMENT_SYNTAX:
        root->type = &VOID_TYPE;
        typecheck(root->first_child);
    default:
        root->type = &VOID_TYPE;
        break;
    }
}

/**
 * Verify that a binary expression operates on valid types
 * @param root a pointer to the binary expression node  
 */
void typecheck_binary_expression(struct SyntaxNode *root) {
    typecheck(root->first_child);
    typecheck(root->last_child);

    root->type = root->first_child->type;

    // TODO: method to check if binary operator is applicable to types
    if (types_match(root->first_child->type, root->last_child->type)) {
        root->type = root->first_child->type;
    } else {
        root->type = &ERROR_TYPE;

        printf(BOLD_RED "Error: binary operator %s is not applicable to types ",  token_kind_names[root->token->kind]);
        print_type(root->first_child->type);
        printf(" and ");
        print_type(root->last_child->type);
        printf("\n" RESET);

        num_type_errors++;
    }
}

/**
 * Verify that an assignment expression has valid types
 * @param root a pointer to the assignment expression node  
 */
void typecheck_assignment_expression(struct SyntaxNode *root) {
    typecheck(root->first_child);
    typecheck(root->last_child);

    if (root->first_child->syntax != IDENTIFIER_SYNTAX) {
        printf(BOLD_RED "Error: can only assign to identifier, not %s\n" RESET,  syntax_names[root->first_child->syntax]);
        num_type_errors++;
    }

    // TODO: method to check if assignment operator is applicable
    if (types_match(root->first_child->type, root->last_child->type)) {
        root->type = root->first_child->type;
    } else {
        root->type = &ERROR_TYPE;

        printf(BOLD_RED "Error: assignment operator %s is not applicable to types ",  token_kind_names[root->token->kind]);
        print_type(root->first_child->type);
        printf(" and ");
        print_type(root->last_child->type);
        printf("\n" RESET);

        num_type_errors++;
    }
}

/**
 * Verify that a unary expression has a valid type
 * @param root a pointer to the unary expression node  
 */
void typecheck_unary_expression(struct SyntaxNode *root) {
    typecheck(root->first_child);

    // TODO: method to check if unary operator is applicable to type
    if (types_match(root->first_child->type, &INT_TYPE)) {
        root->type = root->first_child->type;
    } else {
        root->type = &ERROR_TYPE;
        
        printf(BOLD_RED "Error: unary operator %s is not applicable to type ",  token_kind_names[root->token->kind]);
        print_type(root->first_child->type);
        printf("\n" RESET);

        num_type_errors++;
    }
}

