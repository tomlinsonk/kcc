/**
 * @file emitter.c
 * @author Kiran Tomlinson
 * @date 9 June 2023
 * @brief Converts bound syntax tree into 65C02 assembly for the KiT  
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>

#include "emitter.h"
#include "parser.h"
#include "lexer.h"
#include "types.h"
#include "symbols.h"

#define BOLD_RED    "\033[1m\033[31m"
#define BOLD_YELLOW "\033[1m\033[33m"
#define RESET       "\033[0m"

static FILE *out;
static int n_labels = 0;
static struct Label *head_label = NULL;
static struct Label *tail_label = NULL;


/**
 * Emit a (possibly formatted) string to out. Adds a tab before and a newline afterwards
 * @param str the string to print
 * @param ... an optional list of variables if str needs to be formatted
 */
void emit(char *str, ...) {
    fputc('\t', out); 
    va_list args;
    va_start(args, str);
    vfprintf(out, str, args);
    va_end(args);
    fputc('\n', out);
}


/**
 * Emit an assembly label
 * @param label the label
 */
void emit_label(struct Label *label) {
    fprintf(out, "%s:\n", label->name);
}

/**
 * Emit an assembly label with an explicit name
 * @param name the name of the label
 */
void emit_named_label(char *name) {
    fprintf(out, "%s:\n", name);
}

/**
 * Make a new label _n (where n is the number of labels that exist already).
 * Adds to the linked list of labels.
 * Note: assumes there are fewer than 1 billion labels (seems ok)
 * @return a pointer to the new Label
 */
struct Label *new_label() {
    char *name = malloc(10 * sizeof(char));
    struct Label *label = malloc(sizeof(struct Label));
    sprintf(name, "_%i", n_labels++);
    label->name = name;
    label->next = NULL;

    if (head_label == NULL) {
        head_label = label;
        tail_label = label;
    } else {
        tail_label->next = label;
    }

    return label;
}

/**
 * Free all of the labels and their names
 */
void free_labels() {
    while (head_label != NULL) {
        tail_label = head_label->next;
        free(head_label->name);
        free(head_label);
        head_label = tail_label;
    }
}


/**
 * Emits the 65C02 assembly code for the given bound SyntaxTree
 * @param root the root of the syntax tree (a translation unit node)
 */
void emit_translation_unit(struct SyntaxNode *root) {
    FILE *template = fopen("template.asm", "r");
    char c;
    struct SyntaxNode *node;

    out = fopen("out.asm", "w");    

    while ((c = fgetc(template)) != EOF)
        fputc(c, out);

    fclose(template);


    for (node = root->first_child; node != NULL; node = node->next) {
        if (node->syntax == FUNCTION_DEFINITION_SYNTAX) {
            emit_function_definition(node);
        } else {
            printf(BOLD_YELLOW "Warning: no emitter support for external declarations.\n" RESET);
        }
    }

    fclose(out);
}


/**
 * Emit the 65C02 assembly for a function definition
 * @param node the function definition SyntaxNode
 * @param symbol_table the top-level symbol table
 */
void emit_function_definition(struct SyntaxNode *node) {
    struct Symbol *symbol = node->symbol;
    struct SyntaxNode *body = node->last_child;

    // Emit the function label
    emit_named_label(symbol->name);

    // TODO: handle arguments

    // Emit function body
    emit_compound_statement(node->last_child);

    // Return from the function
    emit("jsr _pop");
    emit("pha");
    emit("jsr _pop");
    emit("pha");
    emit("rts");
}


/**
 * Emit assembly for a compound statement. Allocates space for locals on the stack 
 * and pops them when done
 * @param node the compound statement node 
 */
void emit_compound_statement(struct SyntaxNode *node) {
    int n_local_bytes = 0;

    // Push space for locals if there are declarations
    if (node->first_child != NULL && node->first_child->syntax == DECLARATION_LIST_SYNTAX) {
        struct Symbol *decl_symbol;
        for (struct SyntaxNode *declaration = node->first_child->first_child; declaration != NULL; declaration = declaration->next) {
            decl_symbol = declaration->last_child->symbol;
            if (types_match(decl_symbol->type, &INT_TYPE)) {
                n_local_bytes += 2;
            } else if (types_match(decl_symbol->type, &CHAR_TYPE)) {
                n_local_bytes += 1;
            } else {
                printf(BOLD_RED "ERROR: no emitter support for type ");
                print_type(decl_symbol->type);
                printf("\n" RESET);
            }
        }

        if (n_local_bytes > 0)
            emit("add2 sp : #-%i", n_local_bytes);
    }

    // Emit all statements
    if (node->last_child != NULL && node->last_child->syntax == STATEMENT_LIST_SYNTAX) {
        node = node->last_child->first_child;
        while (node != NULL) {
            emit_statement(node);
            node = node->next;
        }
    }

    // Pop locals
    if (n_local_bytes > 0) 
        emit("add2 sp : #%i", n_local_bytes);
}

/**
 * Emit assembly for a statement 
 * @param node the statement node
 */
void emit_statement(struct SyntaxNode *node) {
    switch (node->syntax){
    case EXPRESSION_STATEMENT_SYNTAX:
        emit_expression_statement(node);
        break;
    case RETURN_STATEMENT_SYNTAX:
        emit_return_statement(node);
        break;
    default:
        printf(BOLD_RED "Error: can't emit statement kind %s" RESET, syntax_names[node->syntax]);
        break;
    }
}   

/**
 * Emit assembly for an expression statement 
 * @param node the expression statement node
 */
void emit_expression_statement(struct SyntaxNode *node) {
    if (node->first_child != NULL) {
        switch (node->first_child->syntax) {
        case UNARY_EXPRESSION_SYNTAX:
            // TODO
            break;    
        case BINARY_EXPRESSION_SYNTAX:
            // TODO
            break;
        case PARENTHESIZED_EXPRESSION_SYNTAX:
            // TODO
            break;
        case ASSIGNMENT_EXPRESSION_SYNTAX:
            // TODO
            break;
        default:
            printf(BOLD_RED "Error: can't emit expression kind %s" RESET, syntax_names[node->first_child->syntax]);
            break;
        }
    }

}

/**
 * Emit assembly for a return statement 
 * @param node the return statement node
 */
void emit_return_statement(struct SyntaxNode *node) {
    
}


