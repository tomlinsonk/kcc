#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "lexer.h"


const char *token_type_names[] = {"START", "INT", "+", "-", "*", "/", "%", "(", ")", "{", "}", "[", "]", ";", "STRING", "END", "UNKNOWN"};

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
int get_int_value(char input[], int start, int length) {
    int n = 0;
    for (int i = start; i < start + length; i++) {
        n = 10 * n + (input[i] - '0');
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
    head->kind = START_TOKEN;

    struct Token *tail = malloc(sizeof(struct Token));
    tail->kind = END_TOKEN;

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
            new_token->kind = INT_TOKEN;
            new_token->int_value = get_int_value(input, new_token->index, new_token->length);
        } else if (input[i] == '"') {
            i++;
            while (input[i] != '\0' && input[i] != '"') 
                i++;
            
            if (input[i] != '\0')
                i++;

            new_token->length = i - new_token->index;
            new_token->kind = STRING_LITERAL_TOKEN;
            new_token->string_value = input + new_token->index + 1;
        } else if (strchr("+-*/%(){}[];", input[i]) != NULL) {
            new_token->length = 1;

            switch (input[i]) {
            case '+':
                new_token->kind = PLUS_TOKEN;
                break;
            case '-':
                new_token->kind = MINUS_TOKEN;
                break;
            case '*':
                new_token->kind = STAR_TOKEN;
                break;
            case '/':
                new_token->kind = SLASH_TOKEN;
                break;    
            case '%':
                new_token->kind = PERCENT_TOKEN;
                break;                 
            case '(':
                new_token->kind = OPEN_PAREN_TOKEN;
                break;
            case ')':
                new_token->kind = CLOSE_PAREN_TOKEN;
                break;
            case '{':
                new_token->kind = OPEN_BRACE_TOKEN;
                break;
            case '}':
                new_token->kind = CLOSE_BRACE_TOKEN;
                break;
            case '[':
                new_token->kind = OPEN_BRACKET_TOKEN;
                break;
            case ']':
                new_token->kind = CLOSE_BRACKET_TOKEN;
                break;
            case ';':
                new_token->kind = SEMICOLON_TOKEN;
                break;
            }
            i++;
        } else {
            new_token->kind = UNKNOWN_TOKEN;
            new_token->index = i;
            new_token->length = 1;
            i++;
        }

        cur_token->next = new_token;
        cur_token = new_token;
    }

    return head;
}

void print_tokens(char input[], struct Token *current) {
    while (current != NULL) {
        printf("%s ", token_type_names[current->kind]);
        printf("%.*s\n", current->length, input + current->index);
        current = current->next;
    }
}



