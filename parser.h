#ifndef PARSER
#define PARSER


enum syntax_type {
    INTEGER_CONSTANT,
    BINARY_OPERATOR
};


struct syntax_node {
    struct Token *token;
};

#endif