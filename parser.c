#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "lexer.h"
#include "parser.h"

const char *syntax_names[] = {"INT_CONSTANT_SYNTAX", "ADDITIVE_EXPRESSION_SYNTAX", "MULTIPLICATIVE_EXPRESSION_SYNTAX", "ERROR_SYNTAX"};

struct Token *cur_token;
int num_errors;
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
        printf(" %i", *root->token->int_value);
    } else if (root->syntax == ADDITIVE_EXPRESSION_SYNTAX) {
        printf(" %s", token_type_names[root->token->type]);
    }

    printf("\n");

    char* new_indent = malloc((indent_size + 3) * sizeof(char));
    strcpy(new_indent, indent);
    strcat(new_indent, is_last ? "   " : "│  ");


    if (root->left != NULL) {
        pretty_print_helper(root->left, new_indent, indent_size + 3, 0);
    }

    if (root->right != NULL) {
        pretty_print_helper(root->right, new_indent, indent_size + 3, 1);
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

/**
 * Check that the current token matches the expected type (error otherwise)
 * Also, progress to the next token.
*/
void match(enum TokenType token_type) {
    if (cur_token->type != token_type) {
        num_errors++;
        printf("Error: expected %s, got %s\n", token_type_names[token_type], token_type_names[cur_token->type]);
    }

    cur_token = cur_token->next;
}





struct SyntaxNode *parse(struct Token *head, char input_seq[]) {
    cur_token = head;
    num_errors = 0;
    input = input_seq;

    match(START_TOKEN);
    
    struct SyntaxNode *root = parse_statement();

    match(END_TOKEN);

    return root;
}

struct SyntaxNode *parse_statement() {
    struct SyntaxNode *node = parse_additive_expression();

    match(SEMICOLON_TOKEN);

    return node;
}

struct SyntaxNode *parse_additive_expression() {
    struct SyntaxNode *node = parse_multiplicative_expression();
    struct SyntaxNode *new_node;

    while (cur_token->type == PLUS_TOKEN || cur_token->type == MINUS_TOKEN) {
        new_node = malloc(sizeof(struct SyntaxNode));
        new_node->left = node;
        new_node->token = cur_token;
        new_node->syntax = ADDITIVE_EXPRESSION_SYNTAX;

        cur_token = cur_token->next;
        new_node->right = parse_multiplicative_expression();

        node = new_node;
    }

    return node;
}

struct SyntaxNode *parse_multiplicative_expression() {
    struct SyntaxNode *node = parse_integer_constant();
    struct SyntaxNode *new_node;

    while (cur_token->type == STAR_TOKEN || cur_token->type == SLASH_TOKEN) {
        new_node = malloc(sizeof(struct SyntaxNode));
        new_node->left = node;
        new_node->token = cur_token;
        new_node->syntax = MULTIPLICATIVE_EXPRESSION_SYNTAX;

        cur_token = cur_token->next;
        new_node->right = parse_integer_constant();

        node = new_node;
    }

    return node;
}


struct SyntaxNode *parse_integer_constant() {
    struct SyntaxNode *node = malloc(sizeof(struct SyntaxNode));
    node->token = cur_token;

    if (cur_token->type == INT_TOKEN) {
        node->syntax = INT_CONSTANT_SYNTAX;
    } else {
        num_errors++;
        printf("Error: expected int, got ");
        printf("%.*s\n", cur_token->length, input + cur_token->index);
        node->syntax = ERROR_SYNTAX;
    }

    cur_token = cur_token->next;

    return node;
}


