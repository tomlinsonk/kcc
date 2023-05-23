#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "lexer.h"


const char *token_type_names[] = {"START", "INT", "+", "-", "*", "/", "%", "(", ")", "{", "}", "[", "]", ";", "END", "UNKNOWN"};

/**
 * Return the size of a file
 * @param *fp: pointer to file
*/
int fsize(FILE *fp) {
    fseek(fp, 0, SEEK_END); 
    int size = ftell(fp);
    rewind(fp);

    return size;
}

/**
 * Parse an int value from a subarray of input, return a pointer to int
*/
int *get_int_value(char input[], int start, int length) {
    int *n = malloc(sizeof(int));
    for (int i = start; i < start + length; i++) {
        *n = 10 * (*n) + (input[i] - '0');
    }
    
    return n;
}

/**
 * Free every token and value in the given linked list
*/
void free_tokens(struct Token *head) {
    struct Token *current = head;
    struct Token *next = head->next;

    while (current != NULL) {
        next = current->next;
        free(current->value);
        free(current);
        current = next;
    }
}

/**
 * Parse the char array into a linked chain of tokens.
 * @return a pointer to the first token in the chain
*/
struct Token *lex(char input[]) {

    struct Token *head = malloc(sizeof(struct Token));
    head->type = START_TOKEN;

    struct Token *tail = malloc(sizeof(struct Token));
    tail->type = END_TOKEN;

    head->next = tail;

    struct Token *cur_token = head;
    struct Token *new_token;


    int i = 0;
    while (1) {
        // consume spaces
        while (isspace(input[i])) 
            i++;
        

        if (input[i] == '\0')
            break;

        new_token = malloc(sizeof(struct Token));
        new_token->index = i;
        new_token->next = tail;


        if (isdigit(input[i])) {
            while (isdigit(input[i])) 
                i++;
            
            new_token->length = i - new_token->index;
            new_token->type = INT_TOKEN;
            new_token->value = get_int_value(input, new_token->index, new_token->length);
        } else if (strchr("+-*/%(){}[];", input[i]) != NULL) {
            new_token->length = 1;

            switch (input[i]) {
            case '+':
                new_token->type = PLUS_TOKEN;
                break;
            case '-':
                new_token->type = MINUS_TOKEN;
                break;
            case '*':
                new_token->type = STAR_TOKEN;
                break;
            case '/':
                new_token->type = SLASH_TOKEN;
                break;    
            case '%':
                new_token->type = PERCENT_TOKEN;
                break;                 
            case '(':
                new_token->type = OPEN_PAREN_TOKEN;
                break;
            case ')':
                new_token->type = CLOSE_PAREN_TOKEN;
                break;
            case '{':
                new_token->type = OPEN_BRACE_TOKEN;
                break;
            case '}':
                new_token->type = CLOSE_BRACE_TOKEN;
                break;
            case '[':
                new_token->type = OPEN_BRACKET_TOKEN;
                break;
            case ']':
                new_token->type = CLOSE_BRACKET_TOKEN;
                break;
            case ';':
                new_token->type = SEMICOLON_TOKEN;
                break;
            }
            i++;
        } else {
            new_token->type = UNKNOWN_TOKEN;
            new_token->index = i;
            new_token-> length = 1;
            i++;
        }

        cur_token->next = new_token;
        cur_token = new_token;
    }

    return head;
}

void print_tokens(char input[], struct Token *current) {
    while (current != NULL) {
        printf("%s ", token_type_names[current->type]);
        printf("%.*s\n", current->length, input + current->index);
        current = current->next;
    }
}



