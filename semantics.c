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
#include "symbols.h"

int num_type_errors = 0;


/**
 * Determine the type of a declaration specifier
 * @param node the declaration specifier node
 * @return a pointer to the type corresponding to the declaration specifier
 */
struct Type *get_declaration_specifier_type(struct SyntaxNode *node) {
    if (node != NULL) {
        switch (node->token->kind) {
        case INT_KEYWORD_TOKEN:
            return &INT_TYPE;    
        case CHAR_KEYWORD_TOKEN:
            return &CHAR_TYPE;
        default:
            break;
        }
    }

    return &ERROR_TYPE;
}


/**
 * Copy a identifier's string name into a new string.
 * Returns null if the syntax node doesn't contain an identifier
 * @param node the node whose token is an identifier
 * @return a pointer to a new string storing the token's name, or null
 */
char *copy_identifier_name(struct SyntaxNode *node) {
    char *new_string = NULL;
    int i;

    if (node != NULL && node->syntax == DECLARATOR_SYNTAX && node->token->kind == IDENTIFIER_TOKEN) {
        new_string = malloc((node->token->length + 1) * sizeof(char));

        // Copy identifier name into new string, add null terminator
        for (i = 0; i < node->token->length; i++) {
            new_string[i] = node->token->string_value[i];
        }
        new_string[i] = '\0';
    }

    return new_string;
}



/**
 * Recursively typecheck a syntax tree. Annotates the nodes with their types.
 * Also logs the number of type errors encountered in num_type_errors
 * @param root a pointer to the root node of the tree to typecheck
 * @param symbol_table a pointer to the current scope's symbol table
 */
void typecheck(struct SyntaxNode *root, struct SymbolTable *symbol_table) {
    struct SyntaxNode *cur_node;

    switch (root->syntax){
    case ASSIGNMENT_EXPRESSION_SYNTAX:
        typecheck_assignment_expression(root, symbol_table);
        break;
    case BINARY_EXPRESSION_SYNTAX:
        typecheck_binary_expression(root, symbol_table);
        break;
    case UNARY_EXPRESSION_SYNTAX:
        typecheck_unary_expression(root, symbol_table);
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
        typecheck(root->first_child, symbol_table);
        root->type = root->first_child->type;
        break;
    case DECLARATION_SYNTAX:
        bind_declaration(root->first_child, root->last_child, symbol_table);
        break;
    case DECLARATION_LIST_SYNTAX:
    case STATEMENT_LIST_SYNTAX:
    case COMPOUND_STATEMENT_SYNTAX:
        root->type = &VOID_TYPE;
        cur_node = root->first_child;
        while (cur_node != NULL) {
            typecheck(cur_node, symbol_table);
            cur_node = cur_node->next;
        }
        break;
    case EXPRESSION_STATEMENT_SYNTAX:
        root->type = &VOID_TYPE;
        typecheck(root->first_child, symbol_table);
    default:
        root->type = &VOID_TYPE;
        break;
    }
}

/**
 * Verify that a binary expression operates on valid types
 * @param root a pointer to the binary expression node  
 * @param symbol_table a pointer to the current scope's symbol table
 */
void typecheck_binary_expression(struct SyntaxNode *root, struct SymbolTable *symbol_table) {
    typecheck(root->first_child, symbol_table);
    typecheck(root->last_child, symbol_table);

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
 * @param symbol_table a pointer to the current scope's symbol table
 */
void typecheck_assignment_expression(struct SyntaxNode *root, struct SymbolTable *symbol_table) {
    typecheck(root->first_child, symbol_table);
    typecheck(root->last_child, symbol_table);

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
 * @param symbol_table a pointer to the current scope's symbol table
 */
void typecheck_unary_expression(struct SyntaxNode *root, struct SymbolTable *symbol_table) {
    typecheck(root->first_child, symbol_table);

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


/**
 * Store a declared variable in the symbol table
 * @param specifier_node a pointer to the declaration specifier node
 * @param declarator_node a pointer to the declarator node
 * @param symbol_table a pointer to the current scope's symbol table
 */
void bind_declaration(struct SyntaxNode *specifier_node, struct SyntaxNode *declarator_node, struct SymbolTable *symbol_table) {
    struct Type *type = get_declaration_specifier_type(specifier_node);
    char *name = copy_identifier_name(declarator_node);

    if (type != &ERROR_TYPE && name != NULL) {
        add_symbol(symbol_table, new_symbol(name, type));
        declarator_node->type = type;
    } else {
        num_type_errors++;
        printf(BOLD_RED "Error: cannot declare variable of type ");
        print_type(type);
        printf(" named %s\n" RESET, name);
    }
 
    
}


