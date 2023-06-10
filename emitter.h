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


void emit(struct SyntaxNode *node, struct SymbolTable *symbol_table, char *fname);


#endif