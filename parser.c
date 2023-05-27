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


void pretty_print(struct SyntaxNode *root) {
    char* indent = "";
    pretty_print_helper(root, indent, 0, 1);
}

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
        printf(" %s", token_type_names[root->token->kind]);
        break;
    case IDENTIFIER_SYNTAX:
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

int get_binary_operator_priority(enum TokenKind token_type) {
    switch (token_type) {
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

int is_assignment_operator(enum TokenKind token_type) {
    switch (token_type) {
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


enum SyntaxKind get_primary_expression_kind(enum TokenKind token_kind) {
    switch (token_kind)
    {
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



/**
 * Check that the current token matches the expected kind (error otherwise)
 * Also, progress to the next token.
*/
void match(enum TokenKind token_type) {
    if (cur_token == NULL) {
        num_parser_errors++;
        printf(BOLD_RED "Error: expected %s, reached end of file\n" RESET, token_type_names[token_type]);
        return;
    }

    if (cur_token->kind != token_type) {
        num_parser_errors++;
        printf(BOLD_RED "Error: expected %s, got %s\n" RESET, token_type_names[token_type], token_type_names[cur_token->kind]);
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
    struct SyntaxNode *node = malloc(sizeof(struct SyntaxNode));

    match(OPEN_BRACE_TOKEN);
    node->syntax = COMPOUND_STATEMENT_SYNTAX;
    node->first_child = parse_statement_list();
    node->last_child = node->first_child;
    match(CLOSE_BRACE_TOKEN);

    return node;
}



struct SyntaxNode *parse_statement_list() {
    struct SyntaxNode *node = malloc(sizeof(struct SyntaxNode));
    struct SyntaxNode *cur_child;

    node->syntax = STATEMENT_LIST_SYNTAX;

    node->first_child = parse_expression_statement();
    node->last_child = node->first_child;

    while (cur_token->kind != CLOSE_BRACE_TOKEN) {
        node->last_child->next = parse_expression_statement();
        node->last_child = node->last_child->next;
    }

    return node;
}


struct SyntaxNode *parse_expression_statement() {
    struct SyntaxNode *node = malloc(sizeof(struct SyntaxNode));

    node->syntax = EXPRESSION_STATEMENT_SYNTAX;
    node->first_child = parse_assignment_expression();
    node->last_child = node->first_child;
    match(SEMICOLON_TOKEN);

    return node;
}


struct SyntaxNode *parse_expression() {
    return parse_assignment_expression();
}


struct SyntaxNode *parse_assignment_expression() {
    struct SyntaxNode *node, *new_node;

    node = parse_binary_expression(0);

    if (is_assignment_operator(cur_token->kind)) {
        new_node = malloc(sizeof(struct SyntaxNode));
        new_node->first_child = node;
        new_node->syntax = ASSIGNMENT_EXPRESSION_SYNTAX;
        new_node->token = cur_token;

        cur_token = cur_token->next;
        new_node->last_child = parse_assignment_expression();
        new_node->first_child->next = new_node->last_child;

        node = new_node;
    }

    return node;
}


struct SyntaxNode *parse_binary_expression(int parent_priority) {
    struct SyntaxNode *node, *new_node;
    int priority;

    node = parse_unary_expression();

    while (1) {
        priority = get_binary_operator_priority(cur_token->kind);

        // Only process operators that have higher priority than the parent
        if (priority <= parent_priority)
            break;

        new_node = malloc(sizeof(struct SyntaxNode));
        new_node->first_child = node;
        new_node->token = cur_token;
        new_node->syntax = BINARY_EXPRESSION_SYNTAX;

        cur_token = cur_token->next;
        new_node->last_child = parse_binary_expression(priority);
        new_node->first_child->next = new_node->last_child;

        node = new_node;
    }

    return node;
}

struct SyntaxNode *parse_unary_expression() {
    struct SyntaxNode *new_node;

    if (cur_token->kind == PLUS_TOKEN || cur_token->kind == MINUS_TOKEN) {
        new_node = malloc(sizeof(struct SyntaxNode));
        new_node->syntax = UNARY_EXPRESSION_SYNTAX;
        new_node->token = cur_token;

        cur_token = cur_token->next;
        new_node->first_child = parse_primary_expression();
        new_node->last_child = new_node->first_child;
    } else {
        new_node = parse_primary_expression();
    }

    return new_node;

}



struct SyntaxNode *parse_primary_expression() {
    struct SyntaxNode *node = malloc(sizeof(struct SyntaxNode));

    node->token = cur_token;

    if (cur_token->kind == OPEN_PAREN_TOKEN) {
        node->syntax = PARENTHESIZED_EXPRESSION_SYNTAX;

        cur_token = cur_token->next;
        node->first_child = parse_expression();
        node->last_child = node->first_child;

        match(CLOSE_PAREN_TOKEN);
    } else {
        node->syntax = get_primary_expression_kind(cur_token->kind);

        if (node->syntax == ERROR_SYNTAX) {
            num_parser_errors++;
            printf(BOLD_RED "Error: expected primary expression, got ");
            printf("\"%.*s\"\n" RESET, cur_token->length, input + cur_token->index);
        }

        cur_token = cur_token->next;
    }

    return node;
}


