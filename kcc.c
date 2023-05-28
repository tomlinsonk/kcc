#include "lexer.h"
#include "parser.h"
#include "semantics.h"
#include "symbols.h"


int main() {
    setvbuf(stdout, NULL, _IONBF, 0);    
    FILE *fp = fopen("input.txt", "r");
    int size = fsize(fp);
    char input[size+1];
    int i;
    struct Token *head;
    struct SyntaxNode *root;

    for (i = 0; i < size; i++) {
        input[i] = getc(fp);
    }
    input[size] = '\0';

    head = lex(input);

    

    // printf("\n---- TOKENS ----\n");
    // print_tokens(input, head);

    root = parse(head, input);

    typecheck(root);

    printf("\n---- PARSE TREE ----\n");
    pretty_print(root);

    printf("\nCompiled with %i parser errors and %i type errors.\n\n", num_parser_errors, num_type_errors);
    
    free_tree(root);
    free_tokens(head);

    new_symbol_table();

    return num_parser_errors + num_type_errors;
}