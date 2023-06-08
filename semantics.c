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

int num_semantic_errors = 0;


/**
 * Determine the type of a declaration specifier
 * @param node the declaration specifier node
 * @return a pointer to the type corresponding to the declaration specifier
 */
struct Type *get_declaration_specifier_type(struct SyntaxNode *node) {
    if (node != NULL && node->token != NULL) {
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

    if (node != NULL && node->token != NULL && node->token->kind == IDENTIFIER_TOKEN) {
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
 * When node is inside the function body (in the compound statement subtree),
 * find the return type of the function by tracing up the syntax tree until reaching
 * the function definition
 * 
 * @param node a SyntaxNode inside a function definition
 * @return the return type of the enclosing function
 */
struct Type *get_enclosing_function_return_type(struct SyntaxNode *node) {
    while (node->syntax != FUNCTION_DEFINITION_SYNTAX)
        node = node->parent;

    return node->type->return_type;
}



/**
 * Recursively typecheck a syntax tree. Annotates the nodes with their types.
 * Also logs the number of type errors encountered in num_type_errors
 * @param root a pointer to the root node of the tree to typecheck
 * @param symbol_table a pointer to the current scope's symbol table
 */
void typecheck(struct SyntaxNode *root, struct SymbolTable *symbol_table) {
    struct SyntaxNode *cur_node;
    struct SymbolTable *new_table;

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
    case RETURN_STATEMENT_SYNTAX:
        typecheck_return_statement(root, symbol_table);
        break;
    case INT_CONSTANT_SYNTAX:
        root->type = &INT_TYPE;
        break;
    case STRING_SYNTAX:
        root->type = &CHAR_ARRAY_TYPE;
        break;
    case IDENTIFIER_SYNTAX:
        bind_identifier(root, symbol_table);
        break;
    case PARENTHESIZED_EXPRESSION_SYNTAX:
        typecheck(root->first_child, symbol_table);
        root->type = root->first_child->type;
        break;
    case DECLARATION_SYNTAX:
        bind_declaration(root->first_child, root->last_child, symbol_table);
        break;
    case FUNCTION_DEFINITION_SYNTAX:
        bind_function_definition(root, symbol_table);
        typecheck(root->last_child, symbol_table);
        break;
    case COMPOUND_STATEMENT_SYNTAX:
        // Create a new scope!
        symbol_table = new_subtable(symbol_table);
        // Fall through to typechexking list of children
    case TRANSLATION_UNIT_SYNTAX:
    case DECLARATION_LIST_SYNTAX:
    case STATEMENT_LIST_SYNTAX:
        cur_node = root->first_child;
        while (cur_node != NULL) {
            typecheck(cur_node, symbol_table);
            cur_node = cur_node->next;
        }
        break;
    case EXPRESSION_STATEMENT_SYNTAX:
        if (root->first_child != NULL)
            typecheck(root->first_child, symbol_table);
        break;
    default:
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

        num_semantic_errors++;
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
        num_semantic_errors++;
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

        num_semantic_errors++;
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

        num_semantic_errors++;
    }
}

/**
 * Verify that a return statement returns the correct type
 * @param root a pointer to the return statement node 
 * @param symbol_table a pointer to the current scope's symbol table
 */
void typecheck_return_statement(struct SyntaxNode *root, struct SymbolTable *symbol_table) {
    struct Type *return_type = get_enclosing_function_return_type(root);
    typecheck(root->first_child, symbol_table);

    if (!types_match(return_type, root->first_child->type)) {
        printf(BOLD_RED "Error: trying to return ");
        print_type(root->first_child->type);
        printf(", but declared return type is ");
        print_type(return_type);
        printf("\n" RESET);
        num_semantic_errors++;
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
        // Use get_symbol_in_table rather than get_symbol to allow shadowing
        if (get_symbol_in_table(symbol_table, name) == NULL) {
            add_symbol(symbol_table, new_symbol(name, type));
            declarator_node->type = type;
        } else {
            num_semantic_errors++;
            printf(BOLD_RED "Error: duplicate identifier %s\n" RESET, name);
        }
    } else {
        num_semantic_errors++;
        printf(BOLD_RED "Error: cannot declare variable of type ");
        print_type(type);
        printf(" named %s\n" RESET, name);
    }
}


/**
 * Look up an identifier in the symbol table, ensure it exists, and store its type in the tree
 * @param identifier_node a pointer to the identifier node
 * @param symbol_table a pointer to the current scope's symbol table
 */
void bind_identifier(struct SyntaxNode *identifier_node, struct SymbolTable *symbol_table) {
    char *name = copy_identifier_name(identifier_node);
    struct Symbol *symbol = get_symbol(symbol_table, name);

    if (symbol != NULL) {
        identifier_node->symbol = symbol;
        identifier_node->type = symbol->type;
    } else {
        num_semantic_errors++;
        identifier_node->type = &ERROR_TYPE;
        printf(BOLD_RED "Error: unknown identifier %s\n" RESET, name);
    }

    free(name);
}



/**
 * Store a defined function in the symbol table
 * @param node a pointer to the function definition node
 * @param symbol_table a pointer to the current scope's symbol table
 */
void bind_function_definition(struct SyntaxNode *node, struct SymbolTable *symbol_table) {
    char *name = copy_identifier_name(node->first_child->next);
    // char *name = NULL;

    struct Type *type = new_type(FUNCTION_TYPE);
    type->return_type = get_declaration_specifier_type(node->first_child);

    // TODO: process parameter list, add to parameter type list

    if (type->return_type != &ERROR_TYPE && name != NULL) {
        if (get_symbol_in_table(symbol_table, name) == NULL) {
            add_symbol(symbol_table, new_symbol(name, type));
            node->type = type;
        } else {
            num_semantic_errors++;
            printf(BOLD_RED "Error: duplicate identifier %s\n" RESET, name);
        }
    } else {
        num_semantic_errors++;
        printf(BOLD_RED "Error: cannot declare function with return type ");
        print_type(type->return_type);
        printf(" named %s\n" RESET, name);
    }

    
}
