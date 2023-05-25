#define BOLD_RED    "\033[1m\033[31m"
#define RESET       "\033[0m"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>


#include "semantics.h"
#include "parser.h"
#include "lexer.h"
#include "types.h"

int num_type_errors = 0;


void typecheck(struct SyntaxNode *root) {
    switch (root->syntax){
    case BINARY_EXPRESSION_SYNTAX:
        typecheck_binary_expression(root);
        break;
    case UNARY_EXPRESSION_SYNTAX:
        typecheck_unary_expression(root);
        break;
    case INT_CONSTANT_SYNTAX:
        root->type = &INT_TYPE;
        break;
    case STRING_SYNTAX:
        root->type = &CHAR_ARRAY_TYPE;
        break;
    case PARENTHESIZED_EXPRESSION_SYNTAX:
        typecheck(root->left);
        root->type = root->left->type;
        break;
    default:
        root->type = &VOID_TYPE;
        break;
    }
}


void typecheck_binary_expression(struct SyntaxNode *root) {
    typecheck(root->left);
    typecheck(root->right);

    root->type = root->left->type;

    if (types_match(root->left->type, root->right->type)) {
        root->type = root->left->type;
    } else {
        root->type = &ERROR_TYPE;

        printf(BOLD_RED "Error: binary operator %s is not applicable to types ",  token_type_names[root->token->kind]);
        print_type(root->left->type);
        printf(" and ");
        print_type(root->right->type);
        printf("\n" RESET);

        num_type_errors++;
    }
}

void typecheck_unary_expression(struct SyntaxNode *root) {
    typecheck(root->child);

    root->type = root->child->type;

    if (types_match(root->child->type, &INT_TYPE)) {
        root->type = &INT_TYPE;
    } else {
        root->type = &ERROR_TYPE;
        
        printf(BOLD_RED "Error: unary operator %s is not applicable to type ",  token_type_names[root->token->kind]);
        print_type(root->child->type);
        printf("\n" RESET);

        num_type_errors++;
    }
}

