/**
 * @file kcc.c
 * @author Kiran Tomlinson
 * @date 28 May 2023
 * @brief Entry point for the KiT C Compiler
 * 
 * This file contains the main method for kcc, which initiates
 * lexing, parsing, and typechecking.
 */

#include "lexer.h"
#include "parser.h"
#include "semantics.h"
#include "symbols.h"


/**
 * Compile a C program into 6502 assebly (eventually)
 * @return a status code indicating the number of errors that occurred (0 = success) 
 */
int main() {
    FILE *fp = fopen("input.txt", "r");
    int size = fsize(fp);
    char input[size+1];
    int i;
    struct Token *head;
    struct SyntaxNode *root;
    struct SymbolTable *symbol_table;

    setvbuf(stdout, NULL, _IONBF, 0);    


    for (i = 0; i < size; i++) {
        input[i] = getc(fp);
    }
    input[size] = '\0';

    head = lex(input);

    // printf("\n---- TOKENS ----\n");
    // print_tokens(input, head);

    root = parse(head, input);

    symbol_table = new_symbol_table();


    printf("\n---- PARSE TREE ----\n");
    pretty_print(root);


    typecheck(root, symbol_table);

    printf("\n\n---- TYPECHECKED TREE ----\n");
    pretty_print(root);

    printf("\n\n---- SYMBOL TABLE ----\n");
    print_table(symbol_table);


    printf("\n\nCompiled with %i syntax errors and %i semantic errors.\n\n", num_syntax_errors, num_semantic_errors);
    
    free_tree(root);
    free_tokens(head);
    free_symbol_table(symbol_table);


    return num_syntax_errors + num_semantic_errors;
}