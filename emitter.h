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


void emit_translation_unit(struct SyntaxNode *node);
void emit_function_definition(struct SyntaxNode *node);
void emit_compound_statement(struct SyntaxNode *node);
void emit_statement(struct SyntaxNode *node);

void emit(char *str, ...);
void emit_no_tab(char *str, ...);

#endif