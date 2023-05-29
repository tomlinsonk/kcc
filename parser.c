/**
 * @file parser.c
 * @author Kiran Tomlinson
 * @date 28 May 2023
 * @brief Syntax tree parsing  
 * 
 * This file provides the functions needed to go from a list of tokens to a full syntax tree.
 * Uses a recursive descent parser.
 */


#define BOLD_RED    "\033[1m\033[31m"
#define RESET       "\033[0m"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "lexer.h"
#include "parser.h"
#include "types.h"

struct Token *cur_token;
int num_parser_errors = 0;
char *input;

 
/**
 * Print a syntax tree *beautifully*
 * @param root the root node of the tree to print
 */
void pretty_print(struct SyntaxNode *root) {
    char* indent = "";
    pretty_print_helper(root, indent, 0, 1);
}

/**
 * Recursive helper method for pretty_print
 * @param root the root node of the subtree to print
 * @param indent the current string to print before root
 * @param indent_size the current length of the indent string
 * @param is_last true if this node is that last child of its parent
 */
void pretty_print_helper(struct SyntaxNode *root, char* indent, int indent_size, int is_last) {
    char* marker = is_last ? "└──" : "├──";
    char* new_indent = malloc((indent_size + 4) * sizeof(char));;
    struct SyntaxNode *cur_child;

    strcpy(new_indent, indent);
    strcat(new_indent, is_last ? "   " : "│  ");

    printf("%s", indent);
    printf("%s", marker);
    printf("%s", syntax_names[root->syntax]);

    switch (root->syntax) {
    case INT_CONSTANT_SYNTAX:
        printf(" %i", root->token->int_value);
        break;
    case BINARY_EXPRESSION_SYNTAX:
    case UNARY_EXPRESSION_SYNTAX:
    case ASSIGNMENT_EXPRESSION_SYNTAX:
    case DECLARATION_SPECIFIER_SYNTAX:
        printf(" %s", token_kind_names[root->token->kind]);
        break;
    case IDENTIFIER_SYNTAX:
    case DECLARATOR_SYNTAX:
        printf(" %.*s" , root->token->length, root->token->string_value);
        break;
    default:
        break;
    }

    printf(" ");
    print_type(root->type);
    printf("\n");

    cur_child = root->first_child;
    while (cur_child != NULL) {
        pretty_print_helper(cur_child, new_indent, indent_size + 4, cur_child->next == NULL);
        cur_child = cur_child->next;
    }

    free(new_indent);
}

/**
 * Recursively free a syntax tree
 * @param root the root node of the syntax subtree to tree
 */
void free_tree(struct SyntaxNode *root) {
    struct SyntaxNode *cur_node, *next_node;

    if (root != NULL) {
        cur_node = root->first_child;
        while (cur_node != NULL) {
            next_node = cur_node->next;
            free_tree(cur_node);
            cur_node = next_node;
        }

        free(root);
    }
}

/**
 *  
 */
int get_binary_operator_priority(enum TokenKind token_kind) {
    switch (token_kind) {
        case STAR_TOKEN:
        case SLASH_TOKEN:
        case PERCENT_TOKEN:
            return 10;
        case PLUS_TOKEN:
        case MINUS_TOKEN:
            return 9;
        case DOUBLE_LESS_TOKEN:
        case DOUBLE_GREATER_TOKEN:
            return 8;
        case LESS_TOKEN:
        case LESS_EQUALS_TOKEN:
        case GREATER_TOKEN:
        case GREATER_EQUALS_TOKEN:
            return 7;
        case DOUBLE_EQUALS_TOKEN:
        case BANG_EQUALS_TOKEN:
            return 6;
        case AMPERSAND_TOKEN:
            return 5;
        case CARET_TOKEN:
            return 4;
        case PIPE_TOKEN:
            return 3;
        case DOUBLE_AMPERSAND_TOKEN:
            return 2;
        case DOUBLE_PIPE_TOKEN:
            return 1;
        default:
            break;
    }

    return 0;
}

int is_assignment_operator(struct Token *token) {
    if (token == NULL) 
        return 0;

    switch (token->kind) {
    case EQUALS_TOKEN:
    case STAR_EQUALS_TOKEN:
    case SLASH_EQUALS_TOKEN:
    case PERCENT_EQUALS_TOKEN:
    case PLUS_EQUALS_TOKEN:
    case MINUS_EQUALS_TOKEN:
    case DOUBLE_LESS_EQUALS_TOKEN:
    case DOUBLE_GREATER_EQUALS_TOKEN:
    case AMPERSAND_EQUALS_TOKEN:
    case CARET_EQUALS_TOKEN:
    case PIPE_EQUALS_TOKEN:
        return 1;
    default:
        return 0;
    }
}

int is_declaration_specifier(struct Token *token) {
    if (token == NULL) 
        return 0;

    switch (token->kind) {
    case INT_KEYWORD_TOKEN:
    case CHAR_KEYWORD_TOKEN:
        return 1;
    default:    
        return 0;
    }
}

enum SyntaxKind get_primary_expression_kind(struct Token *token) {
    switch (token->kind) {
    case OPEN_PAREN_TOKEN:
        return PARENTHESIZED_EXPRESSION_SYNTAX;
    case INT_CONSTANT_TOKEN:
        return INT_CONSTANT_SYNTAX;
    case STRING_TOKEN:
        return STRING_SYNTAX;
    case IDENTIFIER_TOKEN:
        return IDENTIFIER_SYNTAX;
    default:
        return ERROR_SYNTAX;
    }
}


struct SyntaxNode *new_syntax_node(enum SyntaxKind syntax, struct Token *token) {
    struct SyntaxNode *node = malloc(sizeof(struct SyntaxNode));
    node->syntax = syntax;
    node->token = token;
    node->first_child = node->last_child = node->next = NULL;
    node->type = NULL;
    
    return node;
}

void add_syntax_child(struct SyntaxNode *parent, struct SyntaxNode *child) {
    if (parent->first_child == NULL){
        parent->first_child = parent->last_child = child;
    } else {
        parent->last_child->next = child;
        parent->last_child = child;
    }
}


/**
 * Check that the current token matches the expected kind (error otherwise)
 * Also, progress to the next token.
*/
void match(enum TokenKind token_kind) {
    if (cur_token == NULL) {
        num_parser_errors++;
        printf(BOLD_RED "Error: expected %s, reached end of file\n" RESET, token_kind_names[token_kind]);
        return;
    }

    if (cur_token->kind != token_kind) {
        num_parser_errors++;
        printf(BOLD_RED "Error: expected %s, got %s\n" RESET, token_kind_names[token_kind], token_kind_names[cur_token->kind]);
    }

    cur_token = cur_token->next;
}

struct SyntaxNode *parse(struct Token *head, char input_seq[]) {
    struct SyntaxNode *root;

    cur_token = head;
    num_parser_errors = 0;
    input = input_seq;

    match(START_TOKEN);
    
    root = parse_compound_statement();

    match(END_TOKEN);

    return root;
}


struct SyntaxNode *parse_compound_statement() {
    struct SyntaxNode *node = new_syntax_node(COMPOUND_STATEMENT_SYNTAX, NULL);

    match(OPEN_BRACE_TOKEN);

    if (is_declaration_specifier(cur_token)) 
        add_syntax_child(node, parse_declaration_list());

    if (cur_token != NULL && cur_token->kind != CLOSE_BRACE_TOKEN && cur_token->kind != END_TOKEN)
        add_syntax_child(node, parse_statement_list());
    
    match(CLOSE_BRACE_TOKEN);

    return node;
}

struct SyntaxNode *parse_declaration_list() {
    struct SyntaxNode *node = new_syntax_node(DECLARATION_LIST_SYNTAX, NULL);

    do {
        add_syntax_child(node, parse_declaration());
    } while (is_declaration_specifier(cur_token));

    return node;
}

struct SyntaxNode *parse_declaration() {
    struct SyntaxNode *node = new_syntax_node(DECLARATION_SYNTAX, NULL);

    add_syntax_child(node, parse_declaration_specifier());
    add_syntax_child(node, parse_declarator());
    match(SEMICOLON_TOKEN);

    return node;
}

// TODO: process sequences of declaration specifiers (like const, struct, enum, ...)
struct SyntaxNode *parse_declaration_specifier() {
    struct SyntaxNode *node = new_syntax_node(DECLARATION_SPECIFIER_SYNTAX, cur_token);

    if (is_declaration_specifier(cur_token)) {
        cur_token = cur_token->next;
    } else {
        node->syntax = ERROR_SYNTAX;
        node->token = NULL;
        num_parser_errors++;
        printf(BOLD_RED "Error: expected declaration specifier, got ");
        printf("\"%.*s\"\n" RESET, cur_token->length, input + cur_token->index);
    }

    return node;
}

// TODO: process more complex declarators (like *x, x[], ...)
struct SyntaxNode *parse_declarator() {
    struct SyntaxNode *node = new_syntax_node(DECLARATOR_SYNTAX, cur_token);

    if (cur_token->kind == IDENTIFIER_TOKEN) {
        cur_token = cur_token->next;
    } else {
        node->syntax = ERROR_SYNTAX;
        node->token = NULL;
        num_parser_errors++;
        printf(BOLD_RED "Error: expected identifier, got ");
        printf("\"%.*s\"\n" RESET, cur_token->length, input + cur_token->index);
    }

    return node;

}

struct SyntaxNode *parse_statement_list() {
    struct SyntaxNode *node = new_syntax_node(STATEMENT_LIST_SYNTAX, NULL);

    do {
        add_syntax_child(node, parse_expression_statement());
    } while (cur_token != NULL && cur_token->kind != CLOSE_BRACE_TOKEN && cur_token->kind != END_TOKEN);

    return node;
}

struct SyntaxNode *parse_expression_statement() {
    struct SyntaxNode *node = new_syntax_node(EXPRESSION_STATEMENT_SYNTAX, NULL);
    add_syntax_child(node, parse_expression());
    match(SEMICOLON_TOKEN);

    return node;
}


struct SyntaxNode *parse_expression() {
    return parse_assignment_expression();
}


struct SyntaxNode *parse_assignment_expression() {
    struct SyntaxNode *node, *new_node;

    node = parse_binary_expression(0);

    if (is_assignment_operator(cur_token)) {
        new_node = new_syntax_node(ASSIGNMENT_EXPRESSION_SYNTAX, cur_token);
       
        add_syntax_child(new_node, node);
        cur_token = cur_token->next;
        add_syntax_child(new_node, parse_expression());

        node = new_node;
    }

    return node;
}


struct SyntaxNode *parse_binary_expression(int parent_priority) {
    struct SyntaxNode *node, *new_node;
    int priority;

    node = parse_unary_expression();

    while (cur_token != NULL) {
        priority = get_binary_operator_priority(cur_token->kind);

        // Only process operators that have higher priority than the parent
        if (priority <= parent_priority)
            break;

        new_node = new_syntax_node(BINARY_EXPRESSION_SYNTAX, cur_token);
        add_syntax_child(new_node, node);
        cur_token = cur_token->next;
        add_syntax_child(new_node, parse_binary_expression(priority));

        node = new_node;
    }

    return node;
}

struct SyntaxNode *parse_unary_expression() {
    struct SyntaxNode *node;

    if (cur_token->kind == PLUS_TOKEN || cur_token->kind == MINUS_TOKEN) {
        node = new_syntax_node(UNARY_EXPRESSION_SYNTAX, cur_token);
        cur_token = cur_token->next;
        add_syntax_child(node, parse_unary_expression());
    } else {
        node = parse_primary_expression();
    }

    return node;
}



struct SyntaxNode *parse_primary_expression() {
    struct SyntaxNode *node = new_syntax_node(get_primary_expression_kind(cur_token), cur_token);

    if (node->syntax == ERROR_SYNTAX) {
        num_parser_errors++;
        node->token = NULL;
        printf(BOLD_RED "Error: expected primary expression, got ");
        printf("\"%.*s\"\n" RESET, cur_token->length, input + cur_token->index);
    } else {
        cur_token = cur_token->next;
        if (node->syntax == PARENTHESIZED_EXPRESSION_SYNTAX) {
            add_syntax_child(node, parse_expression());
            match(CLOSE_PAREN_TOKEN);
        }
    }

    return node;
}


