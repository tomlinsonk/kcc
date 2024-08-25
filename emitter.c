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
    if (symbol->type->return_type->size == 1) {
        emit("lda ret");
    } else if (symbol->type->return_type->size == 2) {
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
        for (struct SyntaxNode *declaration = node->first_child->first_child; declaration != NULL; declaration = declaration->next) {
            n_local_bytes += get_memory_size(declaration->last_child->type);
            declaration->last_child->first_child->symbol->offset = -n_local_bytes;
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
    case IF_STATEMENT_SYNTAX:
        emit_conditional_statement(node, 0);
        break;
    case IF_ELSE_STATEMENT_SYNTAX:
        emit_conditional_statement(node, 1);
        break;
    case WHILE_STATEMENT_SYNTAX:
        emit_while_statement(node);
        break;
    case COMPOUND_STATEMENT_SYNTAX:
        emit_compound_statement(node);
        break;
    default:
        printf(BOLD_RED "Error: can't emit statement kind %s\n" RESET, syntax_names[node->syntax]);
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
        emit("add2 sp : #%i", node->first_child->type->size);
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
        emit_unary_expression(node);
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
    case NOP_EXPRESSION_SYNTAX:
        break;
    case IDENTIFIER_SYNTAX:
        emit_identifier(node);
        break;
    case INT_CONSTANT_SYNTAX:
        emit_int_constant(node);
        break;
    default:
        printf(BOLD_RED "Error: can't emit expression kind %s\n" RESET, syntax_names[node->syntax]);
        break;
    }

}


/**
 * Emit assembly for an integer constant. Pushes the value to the stack.
 * @param node the integer constant node
*/
void emit_int_constant(struct SyntaxNode *node) {
    start_chunk("int constant");
    if (node->type->size == 2) {
        emit("lda #>%i", node->token->int_value);
        emit("jsr _push");
    }
    emit("lda #<%i", node->token->int_value);
    emit("jsr _push");
    end_chunk("int constant");
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
    case MINUS_TOKEN:
        emit_minus_binary_expression(node);
        break;
    case STAR_TOKEN:
        emit_times_binary_expression(node);
        break;
    case SLASH_TOKEN:
        emit_divide_binary_expression(node);
        break;
    case PERCENT_TOKEN:
        emit_mod_binary_expression(node);
        break;
    case DOUBLE_EQUALS_TOKEN:
        emit_equals_binary_expression(node);
        break;
    case BANG_EQUALS_TOKEN:
        emit_not_equals_binary_expression(node);
        break;
    case DOUBLE_GREATER_TOKEN:
        emit_right_shift_binary_expression(node);
        break;
    case DOUBLE_LESS_TOKEN:
        emit_left_shift_binary_expression(node);
        break;
    default:
        printf(BOLD_RED "Error: can't emit binary operator %s\n" RESET, token_kind_names[node->token->kind]);
        break;
    }
}

/**
 * Emit assembly for a unary expression
 * @param node the expression node
*/
void emit_unary_expression(struct SyntaxNode *node) {
    emit_expression(node->first_child);

    switch (node->token->kind) {
    case PLUS_TOKEN:
        // Unary plus does nothing for us (real C promotes to int)
        break;
    case MINUS_TOKEN:
        emit_unary_minus_expression(node);
        break;
    default:
        printf(BOLD_RED "Error: can't emit unary operator %s\n" RESET, token_kind_names[node->token->kind]);
        break;
    }
}

/**
 * Emit assembly for a unary minus expression
 * @param node expression node
*/
void emit_unary_minus_expression(struct SyntaxNode *node) {
    start_chunk("unary -");
    if (node->type->size == 2) {
        emit("ldy #1");
        emit("lda (sp),y");
        emit("eor #$ff");
        emit("clc");
        emit("adc #1");
        emit("sta (sp),y");

        emit("iny");
        emit("lda (sp),y");
        emit("eor #$ff");
        emit("adc #0");
        emit("sta (sp),y");
    } else if (node->type->size == 1) {
        emit("ldy #1");
        emit("lda (sp),y");
        emit("eor #$ff");
        emit("inc");
        emit("sta (sp),y");
    }
    end_chunk("unary -");
}


/**
 * Emit assembly for an addition expression
 * @param node the expression node
*/
void emit_plus_binary_expression(struct SyntaxNode *node) {
    start_chunk("+ expression");
    if (node->type->size == 2) {
        emit_two_byte_add();
    } else if (node->type->size == 1) {
        emit("jsr _pop");
        emit("ldy #1");
        emit("clc");
        emit("adc (sp),y");     
        emit("sta (sp),y");
    }
    end_chunk("+ expression");
}

/**
 * Emit assembly for a subtraction expression
 * @param node the expression node
*/
void emit_minus_binary_expression(struct SyntaxNode *node) {
    start_chunk("- expression");
    if (node->type->size == 2) {
        emit("jsr _pop");
        emit("sta tmp");
        emit("jsr _pop");
        emit("sta tmp+1");
        emit("ldy #1");
        emit("lda (sp),y");
        emit("sec");
        emit("sbc tmp");
        emit("sta (sp),y");
        emit("iny");
        emit("lda (sp),y");
        emit("sbc tmp+1");
        emit("sta (sp),y");
    } else if (node->type->size == 1) {
        emit("jsr _pop");
        emit("sta tmp");
        emit("ldy #1");
        emit("lda (sp),y");
        emit("sec");
        emit("sbc tmp");     
        emit("sta (sp),y");
    }
    end_chunk("- expression");
}


/**
 * Emit assembly for a multiplication expression
 * @param node the expression node
*/
void emit_times_binary_expression(struct SyntaxNode *node) {
    start_chunk("* expression");
    if (node->type->size == 2) {
        emit_two_byte_multiply();
    } else if (node->type->size == 1) {
        emit("jsr _pop");
        emit("sta tmp");
        emit("jsr _pop");
        emit("sta tmp+1");
        emit("jsr _mul8");
        emit("jsr _push");
    }
    end_chunk("* expression");
}

/**
 * Emit assembly for a division expression
 * @param node the expression node
*/
void emit_divide_binary_expression(struct SyntaxNode *node) {
    start_chunk("/ expression");
    if (types_match(node->type, &INT_TYPE)) {
        emit("jsr _pop");
        emit("sta tmp+2");
        emit("jsr _pop");
        emit("sta tmp+3");
        emit("jsr _pop");
        emit("sta tmp");
        emit("jsr _pop");
        emit("sta tmp+1");
        emit("jsr _div16");
        emit("lda tmp+1");
        emit("jsr _push");
        emit("lda tmp");
        emit("jsr _push");
    } else if (types_match(node->type, &CHAR_TYPE)) {
        // Unsigned division for char
        emit("jsr _pop");
        emit("sta tmp+1");
        emit("jsr _pop");
        emit("sta tmp");
        emit("jsr _udiv8");
        emit("lda tmp");
        emit("jsr _push");
    } else {
        printf(BOLD_RED "Error: can't emit division for type ");
        print_type(node->type);
        printf(" \n" RESET);
    }

    end_chunk("/ expression");

}

/**
 * Emit assembly for a mod expression
 * @param node the expression node
*/
void emit_mod_binary_expression(struct SyntaxNode *node) {
    start_chunk("\% expression");
    if (types_match(node->type, &INT_TYPE)) {
        emit("jsr _pop");
        emit("sta tmp+2");
        emit("jsr _pop");
        emit("sta tmp+3");
        emit("jsr _pop");
        emit("sta tmp");
        emit("jsr _pop");
        emit("sta tmp+1");
        emit("jsr _udiv16");
        emit("lda tmp+5");
        emit("jsr _push");
        emit("lda tmp+4");
        emit("jsr _push");
    } else if (types_match(node->type, &CHAR_TYPE)) {
        // Unsigned division for char
        emit("jsr _pop");
        emit("sta tmp+1");
        emit("jsr _pop");
        emit("sta tmp");
        emit("jsr _udiv8");
        emit("jsr _push");
    } else {
        printf(BOLD_RED "Error: can't emit mod for type ");
        print_type(node->type);
        printf(" \n" RESET);
    }

    end_chunk("\% expression");
}


/**
 * Emit assembly for a bit shift (<< or >>).
 * @param node the SyntaxNode of the shift binary expression
 * @param left if 1, do <<; otherwise, do >>
*/
void emit_shift(struct SyntaxNode *node, int left) {
    struct Label *loop = new_label();
    struct Label *all_zeros = new_label();
    struct Label *done = new_label();

    if (node->type->size == 1) {
        emit("jsr _pop");
        emit("cmp #0");
        emit("beq %s", done->name); // if shifting by 0 bits, result is unchanged
        emit("cmp #8");  // if shifting by 8 or more bits, result is 0
        emit("bcs %s", all_zeros->name);

        // do shift
        emit("tax");
        emit("jsr _pop");
        emit_label(loop);
        emit(left ? "asl" : "lsr");
        emit("dex");
        emit("bne %s", loop->name);
        emit("jsr _push");
        emit("jmp %s", done->name);

        emit_label(all_zeros);
        emit("jsr _pop");
        emit("lda #0");
        emit("jsr _push");
    } else if (node->type->size == 2) {
        emit("jsr _pop");
        emit("tax");
        emit("jsr _pop");
        emit("cmp #0");
        emit("bne %s", all_zeros->name); // if hi byte of shift count is nonzero, result is 0
        emit("cpx #16");  // if shifting by 16 or more bits, result is 0
        emit("bcs %s", all_zeros->name);

        // do shift
        emit("jsr _pop");
        emit("sta tmp");
        emit("jsr _pop");
        emit("sta tmp+1");

        emit_label(loop);
        emit(left ? "asl tmp" : "lsr tmp+1");
        emit(left ? "rol tmp+1" : "ror tmp");
        emit("dex");
        emit("bne %s", loop->name);
        emit("lda tmp+1");
        emit("jsr _push");
        emit("lda tmp");
        emit("jsr _push");
        emit("jmp %s", done->name);

        emit_label(all_zeros);
        emit("add2 sp : #2");
        emit("lda #0");
        emit("jsr _push");
        emit("lda #0");
        emit("jsr _push");
    }
    emit_label(done);
}

/**
 * Emit assembly for a >> binary expression
 * @param node the expression node
*/
void emit_right_shift_binary_expression(struct SyntaxNode *node) {
    start_chunk(">> expression");
    emit_shift(node, 0);
    end_chunk(">> expression");
}

/**
 * Emit assembly for a << binary expression
 * @param node the expression node
*/
void emit_left_shift_binary_expression(struct SyntaxNode *node) {
    start_chunk("<< expression");
    emit_shift(node, 1);
    end_chunk("<< expression");
}


/**
 * Emit assembly for an == comparison expression
 * @param node the syntax node of the expreesion
*/
void emit_equals_binary_expression(struct SyntaxNode *node) {
    struct Label *not_equal_label = new_label();
    struct Label *done_label = new_label();

    start_chunk("== expression");
    if (node->type->size == 1) {
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
    } else if (node->type->size == 2) {
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
void emit_not_equals_binary_expression(struct SyntaxNode *node) {
    struct Label *not_equal_label = new_label();
    struct Label *done_label = new_label();

    start_chunk("!= expression");
    emit_equals_binary_expression(node);
    emit("ldy #1");
    emit("lda #1");
    emit("eor (sp),y");
    emit("sta (sp),y");
    end_chunk("!= expression");
}


/**
 * Emit assembly to push an identifier onto the stack. If it's an array,
 * pushes the starting address, otherwise pushes the value of the identifier. 
 * @param node the identifier node
 */
void emit_identifier(struct SyntaxNode *node) {
    start_chunk("identifier");

    if (node->type->kind == ARRAY_TYPE) {
        emit("mov2 fp : tmp");
        emit("add2 tmp : #%i", node->symbol->offset + 1);
        emit("lda tmp+1");
        emit("jsr _push");
        emit("lda tmp");
        emit("jsr _push"); 
    } else if (node->type->size == 1) {
        emit("mov2 fp : tmp");
        emit("add2 tmp : #%i", node->symbol->offset + 1);
        emit("lda (tmp)");
        emit("jsr _push");
    } else if (node->type->size == 2) {
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
    if (node->first_child->syntax == IDENTIFIER_SYNTAX) {
        emit_simple_assignment_expression(node);
    } else {
        emit_complex_assignment_expression(node);
    }
}

/**
 * Emit assembly for an assignment expression in which the lvalue is an
 * identifier. 
 * @param node the assignment expression node
*/
void emit_simple_assignment_expression(struct SyntaxNode *node) {
    emit_expression(node->last_child);

    if (!types_match(node->type, node->last_child->type)) 
        emit_type_conversion(node->last_child->type, node->type);

    start_chunk("simple assignment");

    emit("mov2 fp : tmp");
    emit("add2 tmp : #%i", 1 + node->first_child->symbol->offset);
    emit("ldy #1");
    emit("lda (sp),y");
    emit("sta (tmp)");

    if (node->type->size == 2) {
        emit("inc2 tmp");
        emit("iny");
        emit("lda (sp),y");
        emit("sta (tmp)");
    }

    end_chunk("simple assignment");
}


/**
 * Emit assembly for an assignment expression in which the lvalue is not just
 * an identifier and needs to be computed.
 * 1) compute the memory address of the lvalue and push it onto the stack
 * 2) compute the value to assign and push it onto the stack
 * 3) pop the value and the address and update the value at the address 
 * @param node the assignment expression node
*/
void emit_complex_assignment_expression(struct SyntaxNode *node) {
    // compute the address and push it onto the stack
    emit_expression(node->first_child);

    // compute the new value and push it onto the stack
    emit_expression(node->last_child);

    // If necessary, convert the value's type
    if (!types_match(node->type, node->last_child->type)) 
        emit_type_conversion(node->last_child->type, node->type);

    start_chunk("complex assignment");

    // Pop the value and store in tmp (and tmp+1 if two bytes)
    emit("jsr _pop");
    emit("sta tmp");
    if (node->type->size == 2) {
        emit("jsr _pop");
        emit("sta tmp+1");
    }

    // Pop the address and store in tmp+2 and tmp+3
    emit("jsr _pop");
    emit("sta tmp+2");
    emit("jsr _pop");
    emit("sta tmp+3");

    // Store the value at the address, and push it back onto the stack
    emit("lda tmp");
    emit("sta (tmp+2)");
    emit("jsr _push");
    if (node->type->size == 2) {
        emit("ldy #1"); 
        emit("lda tmp+1");
        emit("sta (tmp+2),y");
        emit("jsr _push");
    }


    end_chunk("complex assignment");
}


/**
 * Emit assembly for a postfix expression
 * @param node the postfix expression syntax node
*/
void emit_postfix_expression(struct SyntaxNode *node) {
    switch (node->token->kind) {
    case PLUS_PLUS_TOKEN:
        emit_increment_postfix_expression(node);
        break;
    case MINUS_MINUS_TOKEN:
        emit_decrement_postfix_expression(node);
        break;
    case OPEN_BRACKET_TOKEN:
        emit_array_index_postfix_expression(node);
        break;
    default:
        printf(BOLD_RED "Error: can't emit postfix operator %s\n" RESET, token_kind_names[node->token->kind]);
        break;
    }
}

/**
 * Emit assembly for a ++ postfix expression
 * @param the node of the postfix expression
*/
void emit_increment_postfix_expression(struct SyntaxNode *node) {
    emit_expression(node->first_child);
   
    start_chunk("++ expression");
    emit("mov2 fp : tmp");
    emit("add2 tmp : #%i", 1 + node->first_child->symbol->offset);
    emit("lda (tmp)");
    emit("clc");
    emit("adc #1");
    emit("sta (tmp)");
    if (node->type->size == 2) {
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
void emit_decrement_postfix_expression(struct SyntaxNode *node) {
    emit_expression(node->first_child);
    start_chunk("-- expression");
    emit("mov2 fp : tmp");
    emit("add2 tmp : #%i", 1 + node->first_child->symbol->offset);
    emit("lda (tmp)");
    emit("sec");
    emit("sbc #1");
    emit("sta (tmp)");
    if (node->type->size == 2) {
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
 * Emit assembly for an array index expression. If node is an lvalue or has array
 * type, places the address of the array item on the stack. Otherwise, places the
 * value of the array item on the stack.
*/
void emit_array_index_postfix_expression(struct SyntaxNode *node) {
    // Push the array address on the stack
    emit_expression(node->first_child);

    // Push the index onto the stack
    emit_expression(node->last_child);

    if (!types_match(node->last_child->type, &INT_TYPE))
        emit_type_conversion(node->last_child->type, &INT_TYPE);

    start_chunk("array index expression");
    int element_size = get_memory_size(node->type);

    if (element_size == 1) {
        // Do nothing! The index is the offset
    } else if (element_size == 2) {
        // Multiply offset by 2
        emit("jsr _pop");
        emit("sta tmp");
        emit("jsr _pop");
        emit("sta tmp+1");
        emit("asl tmp");
        emit("rol tmp+1");
        emit("lda tmp+1");
        emit("jsr _push");
        emit("lda tmp");
        emit("jsr _push");
    } else {
        // Do multiply routine to compute offset.
        emit("lda #>%i", element_size);
        emit("jsr _push");
        emit("lda #<%i", element_size);
        emit("jsr _push");

        emit_two_byte_multiply();
    }

    emit_two_byte_add();

    // If we want the value rather than the index, retreive it
    if (!node->is_lvalue && node->type->kind != ARRAY_TYPE) {
        emit("jsr _pop");
        emit("sta tmp");
        emit("jsr _pop");
        emit("sta tmp+1");

        if (node->type->size == 2) {
            emit("inc2 tmp");
            emit("lda (tmp)");
            emit("jsr _push");
            emit("dec2 tmp");
        }

        emit("lda (tmp)");
        emit("jsr _push");
    }

    end_chunk("array index expression");
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

        if (node->type->size == 2) {
            emit("jsr _pop");
            emit("sta ret+1");
        }

        end_chunk("return");
    }
}

/**
 * Emit assembly for an if or if-else statement
 * @param node the SyntaxNode of the conditional statement
*/
void emit_conditional_statement(struct SyntaxNode *node, int has_else) {
    struct Label *start_if = new_label();
    struct Label *end_if = new_label();
    struct Label *end_else = new_label();

    start_chunk("if condition");
    emit_expression(node->first_child);
    end_chunk("if condition");

    if (node->first_child->type->size == 2) {
        emit("jsr _pop");
        emit("tax");
        emit("jsr _pop");
        emit("bne %s", start_if->name);
        emit("txa");
        emit("bne %s", start_if->name);
        emit("jmp %s", end_if->name);
    } else if (node->first_child->type->size == 1) {
        emit("jsr _pop");
        emit("bne %s", start_if->name);
        emit("jmp %s", end_if->name);
    }

    start_chunk("if body");
    emit_label(start_if);
    emit_statement(node->first_child->next);
    if (has_else) emit("jmp %s", end_else->name);
    end_chunk("if body");

    emit_label(end_if);
    if (has_else) {
        start_chunk("else body");
        emit_statement(node->last_child);
        end_chunk("else body");
        emit_label(end_else);
    }
}

/**
 * Emit assembly for a while loop
 * @param node the node of the while statement
*/
void emit_while_statement(struct SyntaxNode *node) {
    struct Label *while_condition = new_label();
    struct Label *while_body = new_label();
    struct Label *end_while = new_label();

    start_chunk("while condition");
    emit_label(while_condition);
    emit_expression(node->first_child);
    if (node->first_child->type->size == 2) {
        emit("jsr _pop");
        emit("tax");
        emit("jsr _pop");
        emit("bne %s", while_body->name);
        emit("txa");
        emit("bne %s", while_body->name);
        emit("jmp %s", end_while->name);
    } else if (node->first_child->type->size == 1) {
        emit("jsr _pop");
        emit("bne %s", while_body->name);
        emit("jmp %s", end_while->name);
    }
    end_chunk("while condition");

    start_chunk("while body");
    emit_label(while_body);
    emit_statement(node->last_child);
    emit("jmp %s", while_condition->name);
    end_chunk("while body");
    emit_label(end_while);
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




void emit_two_byte_multiply() {
    emit("jsr _pop");
    emit("sta tmp");
    emit("jsr _pop");
    emit("sta tmp+1");
    emit("jsr _pop");
    emit("sta tmp+2");
    emit("jsr _pop");
    emit("sta tmp+3");
    emit("jsr _mul16");
    emit("lda tmp+5");
    emit("jsr _push");
    emit("lda tmp+4");
    emit("jsr _push");
}


void emit_two_byte_add() {
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
}