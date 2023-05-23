#ifndef PARSER
#define PARSER

enum SyntaxType {
    INT_CONSTANT_SYNTAX,
    ADDITIVE_EXPRESSION_SYNTAX,
    MULTIPLICATIVE_EXPRESSION_SYNTAX,
    ERROR_SYNTAX
};

struct SyntaxNode {
    struct Token *token;
    struct SyntaxNode *left;
    struct SyntaxNode *right;
    enum SyntaxType syntax;
};

void pretty_print(struct SyntaxNode *root);

void pretty_print_helper(struct SyntaxNode *root, char* indent, int indent_size, int is_last);

void free_tree(struct SyntaxNode *root);

struct SyntaxNode *parse(struct Token *head, char input_seq[]);

struct SyntaxNode *parse_statement();

struct SyntaxNode *parse_additive_expression();

struct SyntaxNode *parse_multiplicative_expression();

struct SyntaxNode *parse_integer_constant();






#endif