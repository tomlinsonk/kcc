#define BOLD_RED    "\033[1m\033[31m"
#define RESET       "\033[0m"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "lexer.h"
#include "parser.h"
#include "types.h"


const char *syntax_names[] = {"INT_CONSTANT", "STRING_LITERAL", "UNARY_EXPRESSION", "BINARY_EXPRESSION", "PARENTHESIZED_EXPRESSION", "ERROR"};

struct Token *cur_token;
int num_parser_errors;
char *input;


int get_height(struct SyntaxNode *root) {
    if (root == NULL) {
        return 0;
    }

    int left_height = get_height(root->left);
    int right_height = get_height(root->right);

    return 1 + (left_height > right_height ? left_height : right_height);
}

void pretty_print(struct SyntaxNode *root) {
    char* indent = "";
    pretty_print_helper(root, indent, 0, 1);
}

void pretty_print_helper(struct SyntaxNode *root, char* indent, int indent_size, int is_last) {
    char* marker = is_last ? "└──" : "├──";

    printf("%s", indent);
    printf("%s", marker);
    printf("%s", syntax_names[root->syntax]);

    if (root->syntax == INT_CONSTANT_SYNTAX) {
        printf(" %i", root->token->int_value);
    } else if (root->syntax == BINARY_EXPRESSION_SYNTAX || root->syntax == UNARY_EXPRESSION_SYNTAX) {
        printf(" %s", token_type_names[root->token->kind]);
    }

    printf(" ");
    print_type(root->type);

    printf("\n");

    char* new_indent = malloc((indent_size + 4) * sizeof(char));

    strcpy(new_indent, indent);
    strcat(new_indent, is_last ? "   " : "│  ");

    if (root->left != NULL) {
        pretty_print_helper(root->left, new_indent, indent_size + 4, root->right == NULL);
    }

    if (root->right != NULL) {
        pretty_print_helper(root->right, new_indent, indent_size + 4, 1);
    }


    free(new_indent);
}

void free_tree(struct SyntaxNode *root) {
    if (root != NULL) {
        free_tree(root->left);
        free_tree(root->right);
        free(root);
    }
}

int get_binary_operator_priority(enum TokenKind token_type) {
    switch (token_type) {
        case STAR_TOKEN:
        case SLASH_TOKEN:
            return 2;
        case PLUS_TOKEN:
        case MINUS_TOKEN:
            return 1;
        default:
            break;
    }

    return 0;
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
    cur_token = head;
    num_parser_errors = 0;
    input = input_seq;

    match(START_TOKEN);
    
    struct SyntaxNode *root = parse_statement();

    match(END_TOKEN);

    return root;
}

struct SyntaxNode *parse_statement() {
    struct SyntaxNode *node = parse_binary_expression(0);

    match(SEMICOLON_TOKEN);

    return node;
}

struct SyntaxNode *parse_binary_expression(int parent_priority) {
    struct SyntaxNode *node = parse_unary_expression();
    struct SyntaxNode *new_node;

    int priority;

    while (1) {
        priority = get_binary_operator_priority(cur_token->kind);

        // Only process operators that have higher priority than the parent
        if (priority <= parent_priority)
            break;

        new_node = malloc(sizeof(struct SyntaxNode));
        new_node->left = node;
        new_node->token = cur_token;
        new_node->syntax = BINARY_EXPRESSION_SYNTAX;

        cur_token = cur_token->next;
        new_node->right = parse_binary_expression(priority);

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
        new_node->left = parse_primary_expression();
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
        node->left = parse_binary_expression(0);

        match(CLOSE_PAREN_TOKEN);
    } else if (cur_token->kind == INT_TOKEN) {
        node->syntax = INT_CONSTANT_SYNTAX;
        cur_token = cur_token->next;
    } else if (cur_token->kind == STRING_LITERAL_TOKEN) {
        node->syntax = STRING_LITERAL_SYNTAX;
        cur_token = cur_token->next;
    } else {
        num_parser_errors++;
        printf(BOLD_RED "Error: expected primary expression, got ");
        printf("\"%.*s\"\n" RESET, cur_token->length, input + cur_token->index);
        node->syntax = ERROR_SYNTAX;
        cur_token = cur_token->next;
    }

    return node;
}


