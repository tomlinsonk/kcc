#ifndef LEXER
#define LEXER

#include <stdio.h>

extern const char *token_type_names[];

enum TokenKind { 
    START_TOKEN,
    INT_TOKEN,
    PLUS_TOKEN,
    MINUS_TOKEN,
    STAR_TOKEN,
    SLASH_TOKEN,
    PERCENT_TOKEN,
    OPEN_PAREN_TOKEN,
    CLOSE_PAREN_TOKEN,
    OPEN_BRACE_TOKEN,
    CLOSE_BRACE_TOKEN,
    OPEN_BRACKET_TOKEN,
    CLOSE_BRACKET_TOKEN,
    SEMICOLON_TOKEN,
    STRING_LITERAL_TOKEN,
    END_TOKEN,
    UNKNOWN_TOKEN
};


struct Token {
    enum TokenKind kind;
    int index;
    int length;
    union {
        int int_value;
        char *string_value;
    };
    struct Token *next;
    int line_num;
};

void free_tokens(struct Token *head);

int fsize(FILE *fp);

struct Token *lex(char input[]);

void print_tokens(char input[], struct Token *current);

#endif

