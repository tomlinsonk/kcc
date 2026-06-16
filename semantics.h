/**
 * @file semantics.h
 * @author Kiran Tomlinson
 * @date 29 May 2023
 * @brief Header: provides declarations for typechecking functions
 */

#ifndef SEMANTICS
#define SEMANTICS   

#include "parser.h"
#include "symbols.h"
#include "types.h"

struct NodeList {
    struct SyntaxNode **array;
    int capacity;
    int size;
};

extern int num_semantic_errors;
extern struct NodeList literal_list;


int main_exists(struct SymbolTable *symbol_table);
struct Type *get_enclosing_function_return_type(struct SyntaxNode *node);
struct Type *get_declaration_type(struct SyntaxNode *specifier_node, struct SyntaxNode *declarator_node);
char *copy_identifier_name(struct SyntaxNode *node);
void compute_param_bytes(struct SyntaxNode *function_node);

void typecheck(struct SyntaxNode *root, struct SymbolTable *symbol_table);
void typecheck_assignment_expression(struct SyntaxNode *root, struct SymbolTable *symbol_table);
void typecheck_binary_expression(struct SyntaxNode *root, struct SymbolTable *symbol_table);
void typecheck_unary_expression(struct SyntaxNode *root, struct SymbolTable *symbol_table);
void typecheck_postfix_expression(struct SyntaxNode *root, struct SymbolTable *symbol_table);
void typecheck_return_statement(struct SyntaxNode *root, struct SymbolTable *symbol_table);
void typecheck_arguments(struct SyntaxNode *argument_node, struct Type *param_type, struct SymbolTable *symbol_table);
void bind_and_typecheck_declaration(struct SyntaxNode *specifier_node, struct SyntaxNode *declarator_node, struct SymbolTable *symbol_table);

void register_builtins(struct SymbolTable *symbol_table);
void bind_declaration(struct SyntaxNode *specifier_node, struct SyntaxNode *declarator_node, struct SymbolTable *symbol_table);
void bind_identifier(struct SyntaxNode *identifier_node, struct SymbolTable *symbol_table);
void bind_function_definition(struct SyntaxNode *node, struct SymbolTable *symbol_table);
void bind_function_parameters(struct SyntaxNode *function_node, struct SymbolTable *symbol_table);

void init_literal_list();
void add_literal(struct SyntaxNode *node);

#endif