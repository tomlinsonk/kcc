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

#include "emitter.h"
#include "parser.h"
#include "lexer.h"
#include "types.h"
#include "symbols.h"

char *HEADER = 
    ".cpu _65c02\n"
    ".encoding \"ascii\"\n"
    ".segment Code [outPrg=\"%s.prg\", start=$1000]\n";

char *FOOTER = 
    ".segment Stack [virtual, start=$5000]\n"
    "\t.fill $FFF, 0\n";


/**
 * Emits the 65C02 assembly code for the given bound SyntaxTree
 * @param root the root of the syntax tree
 * @param symbol_table the top-level symbol table
 * @param fname the name of the ourput file
 */
void emit(struct SyntaxNode *root, struct SymbolTable *symbol_table, char *fname) {
    FILE *out = fopen(fname, "w");

    fprintf(out, HEADER, "out");


    fputs(FOOTER, out);

    fclose(out);
}
