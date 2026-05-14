/**
 * @file lexer.h
 * @author Kiran Tomlinson
 * @date 28 May 2023
 * @brief Header: provides Token, TokenKind, and related declarations 
 */

#ifndef LEXER
#define LEXER

#include <stdio.h>

static const char *token_kind_names[] = {
    "//",
    "INT", "STRING", 
    "+", "-", "*", "/", "%", "++", "--",
    "~", "|", "&", "^", ">>", "<<",
    "!", "||", "&&",
    "==", "!=", ">", "<", ">=", "<=",
    "=", "*=", "/=", "%=", "+=", "-=", ">>=", "<<=", "&=", "^=", "|=",
    "(", ")", "{", "}", "[", "]", ";", ",",
    "IDENTIFIER", "INT_KEYWORD", "CHAR_KEYWORD", "VOID_KEYWORD", "ASM_KEYWORD", "IF_KEYWORD", "ELSE_KEYWORD", "WHILE_KEYWORD", "FOR_KEYWORD", "RETURN_KEYWORD",  
    "START", "END", "UNKNOWN"
};

enum TokenKind { 
    // Comments
    COMMENT_TOKEN,
    
    // Literals
    INT_CONSTANT_TOKEN,
    STRING_TOKEN,

    // Arithmetic operators
    PLUS_TOKEN,
    MINUS_TOKEN,
    STAR_TOKEN,
    SLASH_TOKEN,
    PERCENT_TOKEN,
    PLUS_PLUS_TOKEN,
    MINUS_MINUS_TOKEN,

    // Bitwise operators
    TILDE_TOKEN,
    PIPE_TOKEN,
    AMPERSAND_TOKEN,
    CARET_TOKEN,
    DOUBLE_GREATER_TOKEN,
    DOUBLE_LESS_TOKEN,

    // Logical operators
    BANG_TOKEN,
    DOUBLE_PIPE_TOKEN,
    DOUBLE_AMPERSAND_TOKEN,

    // Comparison operators
    DOUBLE_EQUALS_TOKEN,
    BANG_EQUALS_TOKEN,
    GREATER_TOKEN,
    LESS_TOKEN,
    GREATER_EQUALS_TOKEN,
    LESS_EQUALS_TOKEN,

    // Assignment operators
    EQUALS_TOKEN, 
    STAR_EQUALS_TOKEN,
    SLASH_EQUALS_TOKEN,
    PERCENT_EQUALS_TOKEN,
    PLUS_EQUALS_TOKEN,
    MINUS_EQUALS_TOKEN,
    DOUBLE_GREATER_EQUALS_TOKEN,
    DOUBLE_LESS_EQUALS_TOKEN,
    AMPERSAND_EQUALS_TOKEN,
    CARET_EQUALS_TOKEN,
    PIPE_EQUALS_TOKEN,

    // Syntax tokens
    OPEN_PAREN_TOKEN,
    CLOSE_PAREN_TOKEN,
    OPEN_BRACE_TOKEN,
    CLOSE_BRACE_TOKEN,
    OPEN_BRACKET_TOKEN,
    CLOSE_BRACKET_TOKEN,
    SEMICOLON_TOKEN,
    COMMA_TOKEN,

    // Identifiers and keywords
    IDENTIFIER_TOKEN,
    INT_KEYWORD_TOKEN,
    CHAR_KEYWORD_TOKEN,
    VOID_KEYWORD_TOKEN,
    ASM_KEYWORD_TOKEN,
    IF_KEYWORD_TOKEN,
    ELSE_KEYWORD_TOKEN,
    WHILE_KEYWORD_TOKEN,
    FOR_KEYWORD_TOKEN,
    RETURN_KEYWORD_TOKEN,

    // Other
    START_TOKEN,
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

