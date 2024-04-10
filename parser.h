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
#include "symbols.h"



static const char *syntax_names[] = {
    "INT_CONSTANT", "STRING", "IDENTIFIER", 
    "UNARY_EXPRESSION", "BINARY_EXPRESSION", "PARENTHESIZED_EXPRESSION", "ASSIGNMENT_EXPRESSION", "POSTFIX_EXPRESSION",
    "IF_STATEMENT", "IF_ELSE_STATEMENT", "WHILE_STATEMENT", "RETURN_STATEMENT", "EXPRESSION_STATEMENT", "COMPOUND_STATEMENT", "STATEMENT_LIST", 
    "DECLARATOR", "ARRAY_DECLARATOR", "DECLARATION_SPECIFIER", "DECLARATION", "DECLARATION_LIST",
    "FUNCTION_DEFINITION", "PARAMETER_LIST",
    "TRANSLATION_UNIT",
    "ERROR"
};

extern int num_syntax_errors;


enum SyntaxKind {
    INT_CONSTANT_SYNTAX,
    STRING_SYNTAX,
    IDENTIFIER_SYNTAX,

    UNARY_EXPRESSION_SYNTAX,
    BINARY_EXPRESSION_SYNTAX,
    PARENTHESIZED_EXPRESSION_SYNTAX,
    ASSIGNMENT_EXPRESSION_SYNTAX,
    POSTFIX_EXPRESSION_SYNTAX,

    IF_STATEMENT_SYNTAX,
    IF_ELSE_STATEMENT_SYNTAX,
    WHILE_STATEMENT_SYNTAX,
    RETURN_STATEMENT_SYNTAX,
    EXPRESSION_STATEMENT_SYNTAX,
    COMPOUND_STATEMENT_SYNTAX,
    STATEMENT_LIST_SYNTAX,

    DECLARATOR_SYNTAX,
    ARRAY_DECLARATOR_SYNTAX,
    DECLARATION_SPECIFIER_SYNTAX,
    DECLARATION_SYNTAX,
    DECLARATION_LIST_SYNTAX,

    FUNCTION_DEFINITION_SYNTAX,
    PARAMETER_LIST_SYNTAX,

    TRANSLATION_UNIT_SYNTAX,

    ERROR_SYNTAX
};



struct SyntaxNode {
    enum SyntaxKind syntax;
    struct Token *token;
    struct Type *type;
    int is_lvalue;
    struct Symbol *symbol;
    struct SyntaxNode *first_child;
    struct SyntaxNode *last_child;
    struct SyntaxNode *next;
    struct SyntaxNode *parent;
};

void pretty_print(struct SyntaxNode *root);

void pretty_print_helper(struct SyntaxNode *root, char* indent, int indent_size, int is_last);

void free_tree(struct SyntaxNode *root);

int is_assignment_operator(struct Token *token);

int is_declaration_specifier(struct Token *token);

struct SyntaxNode *parse(struct Token *head, char input_seq[]);


struct SyntaxNode *parse_translation_unit();

struct SyntaxNode *parse_function_definition(); 



struct SyntaxNode *parse_declaration_list();
struct SyntaxNode *parse_declaration();
struct SyntaxNode *parse_declaration_specifier();
struct SyntaxNode *parse_direct_declarator();

struct SyntaxNode *parse_statement_list();
struct SyntaxNode *parse_statement();
struct SyntaxNode *parse_compound_statement();
struct SyntaxNode *parse_expression_statement();
struct SyntaxNode *parse_return_statement();
struct SyntaxNode *parse_conditional_statement();
struct SyntaxNode *parse_while_statement();

struct SyntaxNode *parse_expression();
struct SyntaxNode *parse_assignment_expression();
struct SyntaxNode *parse_binary_expression(int priority);
struct SyntaxNode *parse_unary_expression();
struct SyntaxNode *parse_postfix_expression();
struct SyntaxNode *parse_primary_expression();





#endif