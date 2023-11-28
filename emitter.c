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
 * Add a comment to the emitted assembly indicating the start of named chunk
 * @param str the name for the chunk
*/
void start_chunk(char *name) {
    fprintf(out, "// START %s\n", name);
}

/**
 * Add a comment to the emitted assembly indicating the end of named chunk
 * @param str the name for the chunk
*/
void end_chunk(char *name) {
    fprintf(out, "// END %s\n", name);
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
    
    start_chunk("function setup");

    // Emit the function label
    emit_named_label(symbol->name);

    // TODO: handle arguments

    // set frame pointer
    emit("mov2 sp : fp");

    end_chunk("function setup");


    // Emit function body
    emit_compound_statement(node->last_child);

    // Return from the function
    start_chunk("function cleanup");

    // pop return address off C stack and repare for rts
    emit("jsr _pop");
    emit("pha");
    emit("jsr _pop");
    emit("pha");

    // restore frame pointer
    emit("jsr _pop");
    emit("sta fp");
    emit("jsr _pop");
    emit("sta fp+1");

    // store return value in registers, if any
    if (symbol->type->return_type->bytes == 1) {
        emit("lda ret");
    } else if (symbol->type->return_type->bytes == 2) {
        emit("lda ret");
        emit("ldx ret+1");
    }

    emit("rts");
    end_chunk("function cleanup");

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
            n_local_bytes += decl_symbol->type->bytes;
            decl_symbol->offset = -n_local_bytes;
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
    if (node->first_child != NULL) {
        emit_expression(node->first_child);
        emit("add2 sp : #%i", node->first_child->type->bytes);
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
    case POSTFIX_EXPRESSION_SYNTAX:
        emit_postfix_expression(node);
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
        start_chunk("int constant");
        emit("lda #>%i", node->token->int_value);
        emit("jsr _push");
        emit("lda #<%i", node->token->int_value);
        emit("jsr _push");
        end_chunk("int constant");
        break;
    default:
        printf(BOLD_RED "Error: can't emit expression kind %s\n" RESET, syntax_names[node->syntax]);
        break;
    }

}


/**
 * Emit assembly for a binary expression. The expression is evaluated in these stages: 
 * 1) evaluate the left operand, which pushes the value onto the stack. Convert type if needed.
 * 2) evaluate the right operand, which pushes the value onto the stack. Convert type if needed.
 * 3) pop the operands off the stack and compute this expression's value
 * 4) push the value of the binary expression onto the stack
 * @param node the binary expression node
 */
void emit_binary_expression(struct SyntaxNode *node) {
    emit_expression(node->first_child);
    if (!types_match(node->first_child->type, node->type))
        emit_type_conversion(node->first_child->type, node->type);

    emit_expression(node->last_child);
    if (!types_match(node->last_child->type, node->type))
        emit_type_conversion(node->last_child->type, node->type);
    
    switch (node->token->kind) {
    case PLUS_TOKEN:
        emit_plus_binary_expression(node);
        break;
    case DOUBLE_EQUALS_TOKEN:
        emit_double_equals_binary_expression(node);
        break;
    case BANG_EQUALS_TOKEN:
        emit_bang_equals_binary_expression(node);
        break;
    default:
        printf(BOLD_RED "Error: can't emit binary operator %s\n" RESET, token_kind_names[node->token->kind]);
        break;
    }
}


/**
 * Emit assembly for an addition expression
 * @param node the expression node
*/
void emit_plus_binary_expression(struct SyntaxNode *node) {
    start_chunk("+ exression");
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
    } else if (node->type->bytes == 1) {
        emit("jsr _pop");
        emit("ldy #1");
        emit("clc");
        emit("adc (sp),y");     
        emit("sta (sp),y");
    }
    end_chunk("+ exression");
}


/**
 * Emit assembly for an == comparison expression
 * @param node the syntax node of the expreesion
*/
void emit_double_equals_binary_expression(struct SyntaxNode *node) {
    struct Label *not_equal_label = new_label();
    struct Label *done_label = new_label();

    start_chunk("== expression");
    if (node->type->bytes == 1) {
        emit("jsr _pop");
        emit("sta tmp");
        emit("jsr _pop");
        emit("cmp tmp");
        emit("bne %s", not_equal_label->name);
        emit("lda #1");
        emit("jmp %s", done_label->name);

        emit_label(not_equal_label);
        emit("lda #0");

        emit_label(done_label);
        emit("jsr _push");
    } else if (node->type->bytes == 2) {
        emit("jsr _pop");
        emit("sta tmp");
        emit("jsr _pop");
        emit("sta tmp+1");
        emit("jsr _pop");
        emit("tax");
        emit("jsr _pop");

        emit("cpx tmp");
        emit("bne %s", not_equal_label->name);
        emit("cmp tmp+1");
        emit("bne %s", not_equal_label->name);
        emit("lda #0");
        emit("jsr _push");
        emit("lda #1");
        emit("jmp %s", done_label->name);

        emit_label(not_equal_label);
        emit("lda #0");
        emit("jsr _push");
        emit("lda #0");
        emit_label(done_label);
        emit("jsr _push");
    }
    end_chunk("== expression");
}


/**
 * Emit assembly for an != comparison expression
 * @param node the syntax node of the expreesion
*/
void emit_bang_equals_binary_expression(struct SyntaxNode *node) {
    struct Label *not_equal_label = new_label();
    struct Label *done_label = new_label();

    start_chunk("!= expression");
    emit_double_equals_binary_expression(node);
    emit("ldy #1");
    emit("lda #1");
    emit("eor (sp),y");
    emit("sta (sp),y");
    end_chunk("!= expression");
}


/**
 * Emit assembly to push an identifier onto the stack
 * @param node the identifier node
 */
void emit_identifier(struct SyntaxNode *node) {
    start_chunk("identifier");

    if (node->type->bytes == 1) {
        emit("mov2 fp : tmp");
        emit("add2 tmp : #%i", node->symbol->offset + 1);
        emit("lda (tmp)");
        emit("jsr _push");
    } else if (node->type->bytes == 2) {
        emit("mov2 fp : tmp");
        emit("add2 tmp : #%i", node->symbol->offset + 2);
        emit("lda (tmp)");
        emit("jsr _push");
        emit("dec2 tmp");
        emit("lda (tmp)");
        emit("jsr _push");
    }
    end_chunk("identifier");
} 


/**
 * Emit assembly for an assignment expression. Updates the value stored in the 
 * left hand side and leaves new value on top of the stack
 * @param node the assignment expression node
 */
void emit_assignment_expression(struct SyntaxNode *node) {
    emit_expression(node->last_child);

    if (!types_match(node->type, node->last_child->type)) 
        emit_type_conversion(node->last_child->type, node->type);

    start_chunk("assignment");

    emit("mov2 fp : tmp");
    emit("add2 tmp : #%i", 1 + node->first_child->symbol->offset);
    emit("ldy #1");
    emit("lda (sp),y");
    emit("sta (tmp)");

    if (node->type->bytes == 2) {
        emit("inc2 tmp");
        emit("iny");
        emit("lda (sp),y");
        emit("sta (tmp)");
    }

    end_chunk("assignment");

}


/**
 * Emit assembly for a postfix expression
 * @param node the postfix expression syntax node
*/
void emit_postfix_expression(struct SyntaxNode *node) {
    switch (node->token->kind) {
    case PLUS_PLUS_TOKEN:
        emit_plus_plus_postfix_expression(node);
        break;
    case MINUS_MINUS_TOKEN:
        emit_minus_minus_postfix_expression(node);
        break;
    default:
        printf(BOLD_RED "Error: can't emit postfix operator %s" RESET, token_kind_names[node->token->kind]);
        break;
    }
}

/**
 * Emit assembly for a ++ postfix expression
 * @param the node of the postfix expression
*/
void emit_plus_plus_postfix_expression(struct SyntaxNode *node) {
    emit_expression(node->first_child);
   
    start_chunk("++ expression");
    emit("mov2 fp : tmp");
    emit("add2 tmp : #%i", 1 + node->first_child->symbol->offset);
    emit("lda (tmp)");
    emit("clc");
    emit("adc #1");
    emit("sta (tmp)");
    if (node->type->bytes == 2) {
        struct Label *no_carry = new_label();
        emit("bcc %s", no_carry->name);
        emit("inc2 tmp");
        emit("lda (tmp)");
        emit("inc");
        emit("sta (tmp)");
        emit_label(no_carry);
    }
    end_chunk("++ expression");
}

/**
 * Emit assembly for a -- postfix expression
 * @param the node of the postfix expression
*/
void emit_minus_minus_postfix_expression(struct SyntaxNode *node) {
    emit_expression(node->first_child);
    start_chunk("-- expression");
    emit("mov2 fp : tmp");
    emit("add2 tmp : #%i", 1 + node->first_child->symbol->offset);
    emit("lda (tmp)");
    emit("sec");
    emit("sbc #1");
    emit("sta (tmp)");
    if (node->type->bytes == 2) {
        struct Label *no_carry = new_label();
        emit("bcs %s", no_carry->name);
        emit("inc2 tmp");
        emit("lda (tmp)");
        emit("dec");
        emit("sta (tmp)");
        emit_label(no_carry);
    }
    end_chunk("-- expression");
}


/**
 * Emit assembly for a return statement 
 * @param node the return statement node
 */
void emit_return_statement(struct SyntaxNode *node) {
    if (node->first_child != NULL) {
        emit_expression(node->first_child);

        if (!types_match(node->first_child->type, node->type))
            emit_type_conversion(node->first_child->type, node->type);

        start_chunk("return");

        emit("jsr _pop");
        emit("sta ret");

        if (node->type->bytes == 2) {
            emit("jsr _pop");
            emit("sta ret+1");
        }

        end_chunk("return");
    }
}


/**
 * Emit assembly for converting the value on top of the stack from one type to another.
 * Pushes/pops from the stack as necessary if the types have different byte lengths
 * @param from_type the type of the current value on top of the stack
 * @param to_type the type to convert to.
*/
void emit_type_conversion(struct Type *from_type, struct Type *to_type) {
    start_chunk("type convert");

    if (types_match(from_type, &INT_TYPE) && types_match(to_type, &CHAR_TYPE)) {
        emit("jsr _pop");
        emit("sta tmp");
        emit("jsr _pop");
        emit("lda tmp");
        emit("jsr _push");
    } else if (types_match(from_type, &CHAR_TYPE) && types_match(to_type, &INT_TYPE)) {
        emit("jsr _pop");
        emit("sta tmp");
        emit("lda #0");
        emit("jsr _push");
        emit("lda tmp");
        emit("jsr _push");
    } else {
        printf(BOLD_RED "Error: No emitter support for conversion from type ");
        print_type(from_type);
        printf(" to type ");
        print_type(to_type);
        printf("\n" RESET);
    }

    end_chunk("type convert");
}


