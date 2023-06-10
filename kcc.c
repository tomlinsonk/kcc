/**
 * @file kcc.c
 * @author Kiran Tomlinson
 * @date 28 May 2023
 * @brief Entry point for the KiT C Compiler
 * 
 * This file contains the main method for kcc, which initiates
 * lexing, parsing, and typechecking.
 */

#define BOLD_RED    "\033[1m\033[31m"
#define RESET       "\033[0m"

#include "lexer.h"
#include "parser.h"
#include "emitter.h"
#include "semantics.h"
#include "symbols.h"


/**
 * Compile a C program into 6502 assebly (eventually)
 * @return a status code indicating the number of errors that occurred (0 = success) 
 */
int main() {
    FILE *fp = fopen("input.txt", "r");
    char *out_fname = "out.asm";

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
    fclose(fp);

    head = lex(input);

    // printf("\n---- TOKENS ----\n");
    // print_tokens(input, head);

    root = parse(head, input);

    symbol_table = new_symbol_table();

    printf("\n---- PARSE TREE ----\n");
    pretty_print(root);

    typecheck(root, symbol_table);

    if (!main_exists(symbol_table)) {
        printf(BOLD_RED "Error: no method int main().\n" RESET);
        num_semantic_errors++;
    }

    printf("\n\n---- TYPECHECKED TREE ----\n");
    pretty_print(root);

    // printf("\n\n---- SYMBOL TABLE ----\n");
    // print_table(symbol_table);
    // 
    // print_table(symbol_table->first_child);

    printf("\n\n%i syntax errors\n", num_syntax_errors);
    printf("%i semantic errors.\n", num_semantic_errors);

    if (num_syntax_errors + num_semantic_errors == 0) {
        printf("Emitting assembly to %s.\n", out_fname);
        emit(root, symbol_table, out_fname);
    } else {
        printf(BOLD_RED "Not compiling due to errors.\n" RESET);
    }

    printf("Done.\n");

    free_tree(root);
    free_tokens(head);
    free_symbol_table(symbol_table);

    return num_syntax_errors + num_semantic_errors;
}