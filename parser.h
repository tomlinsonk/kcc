/**
 * @file parser.h
 * @author Kiran Tomlinson
 * @date 29 May 2023
 * @brief Header: provides SyntaxNode, SyntaxKind, and related declarations
 */

#ifndef PARSER
#define PARSER

#include "types.h"
#include "lexer.h"



static const char *syntax_names[] = {
    "INT_CONSTANT", "STRING", "IDENTIFIER", 
    "UNARY_EXPRESSION", "BINARY_EXPRESSION", "PARENTHESIZED_EXPRESSION", "ASSIGNMENT_EXPRESSION",
    "EXPRESSION_STATEMENT", "COMPOUND_STATEMENT", "STATEMENT_LIST", 
    "DECLARATOR", "DECLARATION_SPECIFIER", "DECLARATION", "DECLARATION_LIST",
    "ERROR"
};

extern int num_parser_errors;


enum SyntaxKind {
    INT_CONSTANT_SYNTAX,
    STRING_SYNTAX,
    IDENTIFIER_SYNTAX,

    UNARY_EXPRESSION_SYNTAX,
    BINARY_EXPRESSION_SYNTAX,
    PARENTHESIZED_EXPRESSION_SYNTAX,
    ASSIGNMENT_EXPRESSION_SYNTAX,

    EXPRESSION_STATEMENT_SYNTAX,
    COMPOUND_STATEMENT_SYNTAX,
    STATEMENT_LIST_SYNTAX,

    DECLARATOR_SYNTAX,
    DECLARATION_SPECIFIER_SYNTAX,
    DECLARATION_SYNTAX,
    DECLARATION_LIST_SYNTAX,

    ERROR_SYNTAX
};



struct SyntaxNode {
    enum SyntaxKind syntax;
    struct Token *token;
    struct Type *type;
    struct SyntaxNode *first_child;
    struct SyntaxNode *last_child;
    struct SyntaxNode *next;
};

void pretty_print(struct SyntaxNode *root);

void pretty_print_helper(struct SyntaxNode *root, char* indent, int indent_size, int is_last);

void free_tree(struct SyntaxNode *root);

int is_assignment_operator(struct Token *token);

int is_declaration_specifier(struct Token *token);

struct SyntaxNode *parse(struct Token *head, char input_seq[]);

struct SyntaxNode *parse_compound_statement();

struct SyntaxNode *parse_declaration_list();
struct SyntaxNode *parse_declaration();
struct SyntaxNode *parse_declaration_specifier();
struct SyntaxNode *parse_declarator();

struct SyntaxNode *parse_statement_list();
struct SyntaxNode *parse_expression_statement();

struct SyntaxNode *parse_expression();
struct SyntaxNode *parse_assignment_expression();
struct SyntaxNode *parse_binary_expression(int priority);
struct SyntaxNode *parse_unary_expression();
struct SyntaxNode *parse_primary_expression();





#endif