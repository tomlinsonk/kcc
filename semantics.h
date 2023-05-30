/**
 * @file semantics.h
 * @author Kiran Tomlinson
 * @date 29 May 2023
 * @brief Header: provides declarations for typechecking functions
 */

#ifndef SEMANTICS
#define SEMANTICS   

#include "parser.h"

extern int num_type_errors;

void typecheck(struct SyntaxNode *root);
void typecheck_assignment_expression(struct SyntaxNode *root);
void typecheck_binary_expression(struct SyntaxNode *root);
void typecheck_unary_expression(struct SyntaxNode *root);

#endif