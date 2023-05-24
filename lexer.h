#ifndef LEXER
#define LEXER

#include <stdio.h>

static const char *token_type_names[] = {
    "START", "INT", "+", "-", "*", "/", "%", "(", ")", "{", "}", "[", "]", ";", "STRING", 
    "IDENTIFIER", "INT_KEYWORD", "CHAR_KEYWORD", "ASM_KEYWORD", "IF_KEYWORD", "ELSE_KEYWORD", "WHILE_KEYWORD", "FOR_KEYWORD", "RETURN_KEYWORD",  
    "END", "UNKNOWN"
};

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
    IDENTIFIER_TOKEN,
    INT_KEYWORD_TOKEN,
    CHAR_KEYWORD_TOKEN,
    ASM_KEYWORD_TOKEN,
    IF_KEYWORD_TOKEN,
    ELSE_KEYWORD_TOKEN,
    WHILE_KEYWORD_TOKEN,
    FOR_KEYWORD_TOKEN,
    RETURN_KEYWORD_TOKEN,
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

