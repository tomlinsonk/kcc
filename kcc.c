#include "lexer.h"
#include "parser.h"



int main() {
    FILE *fp = fopen("input.txt", "r");

    int size = fsize(fp);

    char input[size+1];

    for (int i = 0; i < size; i++) {
        input[i] = getc(fp);
    }
    input[size] = '\0';

    struct Token *head = lex(input);

    // print_tokens(input, head);

    struct SyntaxNode *root = parse(head, input);

    pretty_print(root);
    
    free_tree(root);
    free_tokens(head);

    return 0;
}