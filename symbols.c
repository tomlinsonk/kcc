/**
 * @file symbols.c
 * @author Kiran Tomlinson
 * @date 28 May 2023
 * @brief Provides functions for manipulating symbols and symbol tables  
 * 
 * This file provides implementations of symbols and symbol tables to
 * track the use of identifiers in a program. Symbol tables are implemented
 * using hash tables with separate chaining. Tables are arranged in a tree
 * to represent nested scopes.
 */


#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "symbols.h"

#define INTIAL_CAPACITY 13


/**
 * Print a symbol table for debugging
 * @param table a pointer to the table to print
 */
void print_table(struct SymbolTable *table) {
    int i;
    struct Symbol *current;

    if (table == NULL)
        return;

    for (i = 0; i < table->capacity; i++) {
        printf("%i: ", i);

        current = (table->symbols)[i];

        while (current != NULL) {
            printf("(%s, %u, %i) ", current->name, hash(current->name), current->type->size);
            current = current->next;
        }

        printf("\n");
    }
}


/**
 * Compute a hash code for a string
 * @param string a pointer to the string
 * @return an integer hash code of the string
 */
unsigned int hash(char *string) {
    unsigned int h = 0;
    int i;

    for (i = 0; string[i] != '\0'; i++) {
        h = h * 31 + string[i];
    }

    return h;
}

/**
 * Allocate space for a new symbol table and return a pointer to it
 * Sets capacity according to INTIAL_CAPACITY and initializes symbol lists to null
 * @return a pointer to the new SymbolTable
 */
struct SymbolTable *new_symbol_table() {
    struct SymbolTable *table = malloc(sizeof(struct SymbolTable));
    int i;

    table->symbols = malloc(INTIAL_CAPACITY * sizeof(struct Symbol *));
    table->capacity = INTIAL_CAPACITY;
    table->size = 0;

    for (i = 0; i < INTIAL_CAPACITY; i++)
        (table->symbols)[i] = NULL;

    return table;
}


/**
 * Allocate a new symbol. Initialize it with the given name and type
 * @param name the name of the new symbol
 * @param type the type of the new symbol
 * @param return a pointer to the new symbol 
 */
struct Symbol *new_symbol(char *name, struct Type *type) {
    struct Symbol *symbol = malloc(sizeof(struct Symbol));
    symbol->name = name;
    symbol->type = type;

    return symbol;
}

/**
 * Free a table and all of the symbols in it. Also, recursively free all subtables
 * @param tabel a pointer to the root table to free
 */
void free_symbol_table(struct SymbolTable *table) {
    int i;
    struct Symbol *current, *next;
    struct SymbolTable *current_child, *next_child; 

    // Free the list of Symbols in each hash table cell, including all fields of Symbol
    for (i = 0; i < table->capacity; i++) {
        current = (table->symbols)[i];
        while (current != NULL) {
            next = current->next;
            free(current->name);
            free(current);
            current = next;
        }
    }

    // Then, free the symbols array
    free(table->symbols);

    // Recursively free all child tables
    current_child = table->first_child;
    while (current_child != NULL) {
        next_child = current_child->next;
        free_symbol_table(current_child);
        current_child = next_child;
    }

    // Finally, free this table
    free(table);

}


/**
 * Retreive a symbol from a symbol table by name. If it's not in the current table,
 * look in the parent tables. Returns null if a symbol with that name can't be found
 * @param table the table in which to start searching
 * @param symbol_name the name of the symbol to retreive
 * @return a pointer to the symbol, or null if the symbol can't be found
 */
struct Symbol *get_symbol(struct SymbolTable *table, char *symbol_name) {
    struct Symbol *symbol;

    if (table == NULL)
        return NULL;

    symbol = get_symbol_in_table(table, symbol_name);

    if (symbol == NULL) 
        // if we didn't find the symbol in this table, look recursively in the parent table
        symbol = get_symbol(table->parent, symbol_name);

    return symbol;
}


/**
 * Retreive a symbol from a symbol table by name, but without looking in parent tables
 * @param table the table in which to search
 * @param symbol_name the name of the symbol to retreive
 * @return a pointer to the symbol, or null if the symbol isn't in table
 */
struct Symbol *get_symbol_in_table(struct SymbolTable *table, char *symbol_name) {
    int i = hash(symbol_name) % table->capacity;
    struct Symbol *cur_symbol = (table->symbols)[i];

    while (cur_symbol != NULL) {
        if (strcmp(symbol_name, cur_symbol->name) == 0)
            return cur_symbol;
        
        cur_symbol = cur_symbol->next;
    }

    return NULL; 
}


/**
 * Add a symbol to a symbol table. Does not check for duplicates
 * @param table a pointer to the table to add to
 * @param symbol a pointer to the symbol to add
 */
void add_symbol(struct SymbolTable *table, struct Symbol *symbol) {
    int i = hash(symbol->name) % table->capacity;
    struct Symbol *cur_symbol = (table->symbols)[i];

    symbol->next = cur_symbol;
    (table->symbols)[i] = symbol;

}

/**
 * Create a new empty subtable
 * @param parent the table under which to place the new one
 * @return a pointer to the new table
 */
struct SymbolTable *new_subtable(struct SymbolTable *parent) {
    struct SymbolTable *child = new_symbol_table();
    
    if (parent->first_child == NULL) {
        parent->first_child = parent->last_child = child;
    } else {
        parent->last_child->next = child;
        parent->last_child = child;
    }

    child->parent = parent;

    return child;
}