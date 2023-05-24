#ifndef PARSER
#define PARSER

#include "types.h"



extern const char *syntax_names[];
extern int num_parser_errors;


enum SyntaxType {
    INT_CONSTANT_SYNTAX,
    STRING_LITERAL_SYNTAX,
    UNARY_EXPRESSION_SYNTAX,
    BINARY_EXPRESSION_SYNTAX,
    PARENTHESIZED_EXPRESSION_SYNTAX,
    ERROR_SYNTAX
};



struct SyntaxNode {
    struct Token *token;
    struct SyntaxNode *left;
    struct SyntaxNode *right;
    enum SyntaxType syntax;
    struct Type *type;
};

void pretty_print(struct SyntaxNode *root);

void pretty_print_helper(struct SyntaxNode *root, char* indent, int indent_size, int is_last);

void free_tree(struct SyntaxNode *root);

struct SyntaxNode *parse(struct Token *head, char input_seq[]);

struct SyntaxNode *parse_statement();

struct SyntaxNode *parse_binary_expression(int priority);

struct SyntaxNode *parse_unary_expression();

struct SyntaxNode *parse_primary_expression();






#endif