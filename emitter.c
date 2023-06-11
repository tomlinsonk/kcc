/**
 * @file emitter.c
 * @author Kiran Tomlinson
 * @date 9 June 2023
 * @brief Converts bound syntax tree into 65C02 assembly for the KiT  
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>


#include "emitter.h"
#include "parser.h"
#include "lexer.h"
#include "types.h"
#include "symbols.h"

#define BOLD_RED    "\033[1m\033[31m"
#define BOLD_YELLOW "\033[1m\033[33m"
#define RESET       "\033[0m"


FILE *out;


/**
 * Emit a (possibly formatted) string to out. Adds a tab before and a newline afterwards
 * @param str the string to print
 * @param ... an optional list of variables if str needs to be formatted
 */
void emit(char *str, ...) {
    fputc('\t', out); 
    va_list args;
    va_start(args, str);
    vfprintf(out, str, args);
    va_end(args);
    fputc('\n', out);
}


/**
 * Emit a (possibly formatted) string to out. Adds a newline afterwards
 * @param str the string to print
 * @param ... an optional list of variables if str needs to be formatted
 */
void emit_no_tab(char *str, ...) {
    va_list args;
    va_start(args, str);
    vfprintf(out, str, args);
    va_end(args);
    fputc('\n', out);
}


/**
 * Emits the 65C02 assembly code for the given bound SyntaxTree
 * @param root the root of the syntax tree (a translation unit node)
 * @param symbol_table the top-level symbol table
 */
void emit_translation_unit(struct SyntaxNode *root, struct SymbolTable *symbol_table) {
    FILE *template = fopen("template.asm", "r");
    char c;
    struct SyntaxNode *node;

    out = fopen("out.asm", "w");    

    while ((c = fgetc(template)) != EOF)
        fputc(c, out);

    fclose(template);


    for (node = root->first_child; node != NULL; node = node->next) {
        if (node->syntax == FUNCTION_DEFINITION_SYNTAX) {
            emit_function_definition(node, symbol_table);
        } else {
            printf(BOLD_YELLOW "Warning: no emitter support for external declarations.\n" RESET);
        }
    }

    fclose(out);
}


/**
 * Emit the 65C02 assembly for a function definition
 * @param node the function definition SyntaxNode
 * @param symbol_table the top-level symbol table
 */
void emit_function_definition(struct SyntaxNode *node, struct SymbolTable *symbol_table) {
    struct Symbol *symbol = node->symbol;

    // Emit the function label
    emit_no_tab("%s:", symbol->name);

    // TODO: handle arguments

    // TODO: push space for locals

    // TODO: emit function body

    // TODO: pop locals

    // Return from the function
    emit("jsr _pop");
    emit("pha");
    emit("jsr _pop");
    emit("pha");
    emit("rts");
}




