/**
 * @file lexer.c
 * @author Kiran Tomlinson
 * @date 28 May 2023
 * @brief Converts source file into a list of tokens 
 * 
 * This file provides functions to read a source file and construct a list
 * of tokens for the parser.
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "lexer.h"



/** 
 * Return the size of a file
 * @param *fp: pointer to file
 * @return the number of characters in the file
 */
int fsize(FILE *fp) {
    int size;
    
    fseek(fp, 0, SEEK_END); 
    size = ftell(fp);
    rewind(fp);

    return size;
}

/**
 * Parse an int value from a subarray of input
 * @param char input[] the input text
 * @param start the index of the first character to parse in input
 * @param length the number of digits in the int
 * @return the integer value represented in input[start, ..., start + length - 1]
 */
int get_int_value(char input[], int start, int length) {
    int n = 0;
    int i;

    for (i = start; i < start + length; i++) {
        n = 10 * n + (input[i] - '0');
    }
    return n;
}

/**
 * Parse a char value from a subarray of input. Assumes the char is in single quotes
 * @param char input[] the input text
 * @param start the index of the opening quote in input
 * @param length the number of characters in the char literal (3 or 4 if there's an escape character)
 * @return the char value represented in input[start, ..., start + length - 1]
 */
char get_char_value(char input[], int start, int length) {
    if (length == 3) {
        return input[start+1];
    } else if (length == 4 && input[start+1] == '\\') {
        switch (input[start+2]) {
        case 'n':
            return '\n';
        case 't':
            return '\t';
        case 'r':
            return '\r';
        case '0':
            return '\0';
        case '\\':
            return '\\';
        case '\'':
            return '\'';
        default:
            printf("Error: unrecognized escape character \\%c\n", input[start+2]);
            return input[start+2];
        }
    } else {
        printf("Error: invalid char literal\n");
    }
    return '\0';
}

/**
 * Determine the token kind of an identifier (i.e., check for keywords)
 * If not a keyword, then it's an identifier
 * @param char input[] the input text
 * @param start the index of the first character to parse in input
 * @param length the number of characters in the identifier
 * @return the token kind
 */
enum TokenKind get_identifier_kind(char input[], int start, int length) {
    if (length == 3 && strncmp(input + start, "int", length) == 0)
        return INT_KEYWORD_TOKEN;
    if (length == 4 && strncmp(input + start, "char", length) == 0)
        return CHAR_KEYWORD_TOKEN;
    if (length == 4 && strncmp(input + start, "void", length) == 0)
        return VOID_KEYWORD_TOKEN;
    if (length == 3 && strncmp(input + start, "asm", length) == 0)
        return ASM_KEYWORD_TOKEN;
    if (length == 2 && strncmp(input + start, "if", length) == 0)
        return IF_KEYWORD_TOKEN;
    if (length == 4 && strncmp(input + start, "else", length) == 0)
        return ELSE_KEYWORD_TOKEN;
    if (length == 3 && strncmp(input + start, "for", length) == 0)
        return FOR_KEYWORD_TOKEN;
    if (length == 5 && strncmp(input + start, "while", length) == 0)
        return WHILE_KEYWORD_TOKEN;
    if (length == 6 && strncmp(input + start, "return", length) == 0)
        return RETURN_KEYWORD_TOKEN;

    return IDENTIFIER_TOKEN;
}

/**
 * Free every token and value in the given linked list
 * @param head a pointer to the first node in the chain 
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
 * Parse the char array into a linked chain of tokens
 * @return a pointer to the first token in the chain
*/
struct Token *lex(char input[]) {
    struct Token *head, *tail, *cur_token, *new_token;
    int i = 0;

    head = malloc(sizeof(struct Token));
    tail = malloc(sizeof(struct Token));
    head->kind = START_TOKEN;
    head->next = tail;
    tail->kind = END_TOKEN;
    tail->next = NULL;

    cur_token = head;

    while (1) {
        /* consume spaces */
        while (isspace(input[i])) 
            i++;
        
        if (input[i] == '\0')
            break;

        /* consume commented lines */
        if (input[i] == '/' && input[i+1] == '/') {
            i += 2;
            while (input[i] != '\n' && input[i] != '\0')
                i++;
            continue;
        }

        new_token = malloc(sizeof(struct Token));
        new_token->index = i;
        new_token->next = tail;

        if (isdigit(input[i])) {
            /* Check for int literal */ 
            while (isdigit(input[i])) 
                i++;
            
            new_token->length = i - new_token->index;
            new_token->kind = INT_CONSTANT_TOKEN;
            new_token->int_value = get_int_value(input, new_token->index, new_token->length);
        } else if (isalpha(input[i])) {
            /* Check for identifier */
            while (isalnum(input[i]) || input[i] == '_') 
                i++;
            
            new_token->length = i - new_token->index;
            new_token->kind = get_identifier_kind(input, new_token->index, new_token->length);;
            new_token->string_value = input + new_token->index;
        } else if (input[i] == '\'') {
            /* Check for char literal */
            i++;
            while (input[i] != '\0' && input[i] != '\'')
                i++;
            
            if (input[i] != '\0')
                i++;

            new_token->length = i - new_token->index;
            new_token->kind = CHAR_CONSTANT_TOKEN;
            new_token->char_value = get_char_value(input, new_token->index, new_token->length);
        } else if (input[i] == '"') {
            /* Check for string literal */
            i++;
            while (input[i] != '\0' && input[i] != '"') 
                i++;
            
            if (input[i] != '\0')
                i++;

            new_token->length = i - new_token->index - 2;
            new_token->kind = STRING_TOKEN;
            new_token->string_value = input + new_token->index + 1;
        } else if (input[i] == '>' && input[i+1] == '>') {
            /* Check for >> and >>= */
            if (input[i+2] == '=') {
                new_token->length = 3;
                new_token->kind = DOUBLE_GREATER_EQUALS_TOKEN;
            } else {
                new_token->length = 2;
                new_token->kind = DOUBLE_GREATER_TOKEN;
            }
            i += new_token->length;
        } else if (input[i] == '<' && input[i+1] == '<') {
            /* Check for << and <<= */
            if (input[i+2] == '=') {
                new_token->length = 3;
                new_token->kind = DOUBLE_LESS_EQUALS_TOKEN;
            } else {
                new_token->length = 2;
                new_token->kind = DOUBLE_LESS_TOKEN;
            }
            i += new_token->length;
        } else if (input[i] == '|' && input[i+1] == '|') {
            new_token->length = 2;
            new_token->kind = DOUBLE_PIPE_TOKEN;
            i += 2;
        } else if (input[i] == '&' && input[i+1] == '&') {
            new_token->length = 2;
            new_token->kind = DOUBLE_AMPERSAND_TOKEN;
            i += 2;
        } else if (strchr("=!><*/%+-&^|", input[i]) != NULL) {
            /* Check for bitwise and arithmetic operators and their assignment operators */
            if (input[i+1] == '=') {
                new_token->length = 2;
                switch (input[i]) {
                case '=':
                    new_token->kind = DOUBLE_EQUALS_TOKEN;
                    break;
                case '!':
                    new_token->kind = BANG_EQUALS_TOKEN;
                    break;
                case '>':
                    new_token->kind = GREATER_EQUALS_TOKEN;
                    break;
                case '<':
                    new_token->kind = LESS_EQUALS_TOKEN;
                    break;
                case '*':
                    new_token->kind = STAR_EQUALS_TOKEN;
                    break;
                case '/':
                    new_token->kind = SLASH_EQUALS_TOKEN;
                    break;
                case '%':
                    new_token->kind = PERCENT_EQUALS_TOKEN;
                    break;
                case '+':
                    new_token->kind = PLUS_EQUALS_TOKEN;
                    break;
                case '-':
                    new_token->kind = MINUS_EQUALS_TOKEN;
                    break;
                case '&':
                    new_token->kind = AMPERSAND_EQUALS_TOKEN;
                    break;
                case '^':
                    new_token->kind = CARET_EQUALS_TOKEN;
                    break;
                case '|':
                    new_token->kind = PIPE_EQUALS_TOKEN;
                    break;
                }
            } else {
                new_token->length = 1;
                switch (input[i]) {
                case '=':
                    new_token->kind = EQUALS_TOKEN;
                    break;
                case '!':
                    new_token->kind = BANG_TOKEN;
                    break;
                case '>':
                    new_token->kind = GREATER_TOKEN;
                    break;
                case '<':
                    new_token->kind = LESS_TOKEN;
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
                case '+':
                    new_token->kind = PLUS_TOKEN;
                    if (input[i+1] == '+') {
                        new_token->length = 2;
                        new_token->kind = PLUS_PLUS_TOKEN;
                    }
                    break;
                case '-':
                    new_token->kind = MINUS_TOKEN;
                    if (input[i+1] == '-') {
                        new_token->length = 2;
                        new_token->kind = MINUS_MINUS_TOKEN;
                    }
                    break;
                case '&':
                    new_token->kind = AMPERSAND_TOKEN;
                    break;
                case '^':
                    new_token->kind = CARET_TOKEN;
                    break;
                case '|':
                    new_token->kind = PIPE_TOKEN;
                    break;
                }
            }
            i += new_token->length;
        } else if (strchr("(){}[];,", input[i]) != NULL) {
            /* Check for syntax tokens */
            new_token->length = 1;

            switch (input[i]) {
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
            case ',':
                new_token->kind = COMMA_TOKEN;
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

/**
 * Print out a list of tokens, and the source text each corresponds to
 * @param input the source text
 * @param head a pointer to the first token in the chain 
 */
void print_tokens(char input[], struct Token *head) {
    while (head != NULL) {
        printf("%s ", token_kind_names[head->kind]);
        printf("%.*s\n", head->length, input + head->index);
        head = head->next;
    }
}



