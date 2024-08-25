/**
 * @file emitter.h
 * @author Kiran Tomlinson
 * @date 9 June 2023
 * @brief Header: provides emitter-related methods 
 */

#ifndef EMITTER
#define EMITTER

#include "parser.h"
#include "lexer.h"
#include "types.h"
#include "symbols.h"

struct Label {
    char *name;
    struct Label *next;
};


struct Label *new_label();
void free_labels();

void start_chunk(char *name);
void end_chunk(char *name);

void emit_translation_unit(struct SyntaxNode *node);
void emit_function_definition(struct SyntaxNode *node);

void emit_compound_statement(struct SyntaxNode *node);
void emit_statement(struct SyntaxNode *node);
void emit_return_statement(struct SyntaxNode *node);
void emit_expression_statement(struct SyntaxNode *node);
void emit_conditional_statement(struct SyntaxNode *node, int has_else);
void emit_while_statement(struct SyntaxNode *node);
void emit_for_statement(struct SyntaxNode *node);

void emit_expression(struct SyntaxNode *node);

void emit_binary_expression(struct SyntaxNode *node);
void emit_plus_binary_expression(struct SyntaxNode *node);
void emit_minus_binary_expression(struct SyntaxNode *node);
void emit_times_binary_expression(struct SyntaxNode *node);
void emit_divide_binary_expression(struct SyntaxNode *node);
void emit_mod_binary_expression(struct SyntaxNode *node);
void emit_equals_binary_expression(struct SyntaxNode *node);
void emit_not_equals_binary_expression(struct SyntaxNode *node);
void emit_shift(struct SyntaxNode *node, int left);
void emit_right_shift_binary_expression(struct SyntaxNode *node);
void emit_left_shift_binary_expression(struct SyntaxNode *node);

void emit_postfix_expression(struct SyntaxNode *node);
void emit_increment_postfix_expression(struct SyntaxNode *node);
void emit_decrement_postfix_expression(struct SyntaxNode *node);
void emit_array_index_postfix_expression(struct SyntaxNode *node);

void emit_unary_expression(struct SyntaxNode *node);
void emit_unary_minus_expression(struct SyntaxNode *node);

void emit_assignment_expression(struct SyntaxNode *node);
void emit_simple_assignment_expression(struct SyntaxNode *node);
void emit_complex_assignment_expression(struct SyntaxNode *node);

void emit_identifier(struct SyntaxNode *node);
void emit_int_constant(struct SyntaxNode *node);

void emit(char *str, ...);
void emit_label(struct Label *label);
void emit_named_label(char *name);

void emit_type_conversion(struct Type *from_type, struct Type *to_type);

void emit_two_byte_multiply();
void emit_two_byte_add();

#endif