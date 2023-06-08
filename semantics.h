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

extern int num_semantic_errors;

void typecheck(struct SyntaxNode *root, struct SymbolTable *symbol_table);
void typecheck_assignment_expression(struct SyntaxNode *root, struct SymbolTable *symbol_table);
void typecheck_binary_expression(struct SyntaxNode *root, struct SymbolTable *symbol_table);
void typecheck_unary_expression(struct SyntaxNode *root, struct SymbolTable *symbol_table);
void typecheck_return_statement(struct SyntaxNode *root, struct SymbolTable *symbol_table);

void bind_declaration(struct SyntaxNode *specifier_node, struct SyntaxNode *declarator_node, struct SymbolTable *symbol_table);
void bind_identifier(struct SyntaxNode *identifier_node, struct SymbolTable *symbol_table);
void bind_function_definition(struct SyntaxNode *node, struct SymbolTable *symbol_table);

#endif