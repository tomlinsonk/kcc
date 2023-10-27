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
static int bytes_pushed = 0;



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

    if (symbol->type->return_type->bytes == 1) {
        emit("lda ret");
    } else if (symbol->type->return_type->bytes == 2) {
        emit("lda ret");
        emit("ldx ret+1");
    }

    emit("rts");
}


/**
 * Emit assembly for a compound statement. Allocates space for locals on the stack 
 * and pops them when done
 * @param node the compound statement node 
 */
void emit_compound_statement(struct SyntaxNode *node) {
    int n_local_bytes = 0;
    int bytes;

    // Push space for locals if there are declarations
    if (node->first_child != NULL && node->first_child->syntax == DECLARATION_LIST_SYNTAX) {
        struct Symbol *decl_symbol;
        for (struct SyntaxNode *declaration = node->first_child->first_child; declaration != NULL; declaration = declaration->next) {
            decl_symbol = declaration->last_child->symbol;
            bytes_pushed += decl_symbol->type->bytes;
            n_local_bytes += decl_symbol->type->bytes;
            decl_symbol->offset = -bytes_pushed;
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
    if (n_local_bytes > 0) {
        emit("add2 sp : #%i", n_local_bytes);
        bytes_pushed -= n_local_bytes;
    }
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
 * Emit assembly for an expression statement. The expression evaluation results in the expression
 * value pushed to the stack. When the statement terminates, the value is popped off the stack
 * @param node the expression statement node
 */
void emit_expression_statement(struct SyntaxNode *node) {
    int bytes_pushed_before = bytes_pushed;

    if (node->first_child != NULL) {
        emit_expression(node->first_child);
        emit("add2 sp : #%i", node->first_child->type->bytes);
        bytes_pushed -= node->first_child->type->bytes;

        if (bytes_pushed != bytes_pushed_before)
            printf(BOLD_RED "Error: bytes pushed is %i, before was %i.\n" RESET, bytes_pushed, bytes_pushed_before);
    }
}

/**
 * Emit assembly for evaluating an expression. The value of the expression is
 * pushed onto the stack.
 * @param node the expression node
 */
void emit_expression(struct SyntaxNode *node) {
    switch (node->syntax) {
    case UNARY_EXPRESSION_SYNTAX:
        // TODO
        break;    
    case BINARY_EXPRESSION_SYNTAX:
        emit_binary_expression(node);
        break;
    case PARENTHESIZED_EXPRESSION_SYNTAX:
        emit_expression(node->first_child);
        break;
    case ASSIGNMENT_EXPRESSION_SYNTAX:
        emit_assignment_expression(node);
        break;
    case IDENTIFIER_SYNTAX:
        emit_identifier(node);
        break;
    case INT_CONSTANT_SYNTAX:
        emit("lda #>%i", node->token->int_value);
        emit("jsr _push");
        emit("lda #<%i", node->token->int_value);
        emit("jsr _push");
        bytes_pushed += 2;
        break;
    default:
        printf(BOLD_RED "Error: can't emit expression kind %s\n" RESET, syntax_names[node->syntax]);
        break;
    }

}


/**
 * Emit assembly for a binary expression. The expression is evaluated in these stages: 
 * 1) evaluate the left operand, which pushes the value onto the stack
 * 2) evaluate the right operand, which pushes the value onto the stack
 * 3) pop the operands off the stack and compute this expression's value
 * 4) push the value of the binary expression onto the stack
 * @param node the binary expression node
 */
void emit_binary_expression(struct SyntaxNode *node) {
    emit_expression(node->first_child);
    emit_expression(node->last_child);
    
    switch (node->token->kind) {
    case PLUS_TOKEN:
        if (node->type->bytes == 2) {
            emit("jsr _pop");
            emit("sta tmp");
            emit("jsr _pop");
            emit("sta tmp+1");
            emit("ldy #1");
            emit("clc");
            emit("lda tmp");
            emit("adc (sp),y");
            emit("sta (sp),y");
            emit("iny");
            emit("lda tmp+1");
            emit("adc (sp),y");
            emit("sta (sp),y");
            bytes_pushed -= 2;
        } else if (node->type->bytes == 1) {
            emit("jsr _pop");
            emit("ldy #1");
            emit("clc");
            emit("adc (sp),y");
            emit("sta (sp),y");
            bytes_pushed -= 1;
        }
        break;
    default:
        printf(BOLD_RED "Error: can't emit binary operator %s\n" RESET, token_kind_names[node->token->kind]);
        break;
    }
}

/**
 * Emit assembly to push an identifier onto the stack
 * TODO: currently assumes offset is < 256; might not be
 * @param node the identifier node
 */
void emit_identifier(struct SyntaxNode *node) {
    printf("emitting identifier. Bytes pushed before: %i\n", bytes_pushed);
    if (node->type->bytes == 1) {
        emit("ldy #%i", 1 + node->symbol->offset + bytes_pushed);
        emit("lda (sp),y");
        emit("jsr _push");
        bytes_pushed += 1;
    } else if (node->type->bytes == 2) {
        emit("ldy #%i", 2 + node->symbol->offset + bytes_pushed);
        emit("lda (sp),y");
        emit("jsr _push");
        emit("lda (sp),y");
        emit("jsr _push");
        bytes_pushed += 2;
    }

    printf("emitted identifier. Bytes pushed after: %i\n", bytes_pushed);
} 


/**
 * Emit assembly for an assignmnet expression. Updates the value stored in the 
 * left hand side and leaves new value on top of the stack
 * TODO: assumes offset is < 256. Should fix eventually
 * @param node the assignment expression node
 */
void emit_assignment_expression(struct SyntaxNode *node) {
    emit_expression(node->last_child);

    if (node->type->bytes == 1) {
        emit("ldy #1");
        emit("lda (sp),y");
        emit("ldy #%i", 1 + node->first_child->symbol->offset + bytes_pushed);
        emit("sta (sp),y");
    } else if (node->type->bytes == 2) {
        emit("ldy #1");
        emit("lda (sp),y");
        emit("ldy #%i", 1 + node->first_child->symbol->offset + bytes_pushed);
        emit("sta (sp),y");

        emit("ldy #2");
        emit("lda (sp),y");
        emit("ldy #%i", 2 + node->first_child->symbol->offset + bytes_pushed);
        emit("sta (sp),y");
    }
}



/**
 * Emit assembly for a return statement 
 * @param node the return statement node
 */
void emit_return_statement(struct SyntaxNode *node) {
    if (node->first_child != NULL) {
        emit_expression(node->first_child);

        emit("jsr _pop");
        emit("sta ret");

        if (node->first_child->type->bytes == 2) {
            emit("jsr _pop");
            emit("sta ret+1");
        }
    }
    
}


