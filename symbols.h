/**
 * @file symbols.h
 * @author Kiran Tomlinson
 * @date 29 May 2023
 * @brief Header: provides Symbol and SymbolTable, plus relevant declarations
 */


#ifndef SYMBOLS
#define SYMBOLS

#include "types.h"

struct Symbol {
    char *name;
    struct Type *type;
    struct Symbol *next;
};

struct SymbolTable {
    struct Symbol **symbols;

    int capacity;
    int size;

    struct SymbolTable *parent;
    struct SymbolTable *first_child;
    struct SymbolTable *last_child;
    struct SymbolTable *next;
};


void print_table(struct SymbolTable *table);

unsigned int hash(char *string);

struct Symbol *new_symbol(char *name, struct Type *type);

struct SymbolTable *new_symbol_table();

void add_symbol(struct SymbolTable *table, struct Symbol *symbol);

struct Symbol *get_symbol(struct SymbolTable *table, char *symbol_name);

void free_symbol_table(struct SymbolTable *table);

struct SymbolTable *new_subtable(struct SymbolTable *parent);


#endif