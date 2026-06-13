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
static int local_offset = 0;


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

    // Compute parameter offsets
    if (node->first_child->next->next->syntax == PARAMETER_LIST_SYNTAX) {
        int offset = 4 + node->type->param_bytes; // 4 bytes for return address and old frame pointer
        struct SyntaxNode *param_node = node->first_child->next->next->first_child;
        while (param_node != NULL) {
            offset -= get_memory_size(param_node->last_child->type);
            param_node->last_child->first_child->symbol->offset = offset;
            param_node = param_node->next;
        }
    }

    start_chunk("function setup");

    // Emit the function label
    emit_named_label(symbol->name);
    emit("{");
    // set frame pointer
    emit("mov2 sp : fp");

    end_chunk("function setup");

    // Emit function body
    emit_compound_statement(node->last_child);

    // Return from the function
    start_chunk("function cleanup");
    emit_named_label("_return");

    // restore stack pointer
    emit("mov2 fp : sp");

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
    emit("}");
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
            declaration->last_child->first_child->symbol->offset = -(n_local_bytes + local_offset);
        }

        // Move stack pointer after locals for this compound statement
        // Update local offset so that nested compound statements can allocate space for their locals
        if (n_local_bytes > 0) {
            emit("add2 sp : #-%i", n_local_bytes);
            local_offset += n_local_bytes;
        }
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
        local_offset -= n_local_bytes;
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
    case FOR_STATEMENT_SYNTAX:
        emit_for_statement(node);
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
        if (node->first_child->type->size > 0)
            emit("add2 sp : #%i", get_memory_size(node->first_child->type));
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
    case CHAR_CONSTANT_SYNTAX:
        emit_char_constant(node);
        break;
    case STRING_SYNTAX:
        emit_string_literal(node);
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
 * Emit assembly for a char constant. Pushes the ASCII byte to the stack.
 */
void emit_char_constant(struct SyntaxNode *node) {
    start_chunk("char constant");
    emit("lda #%i", node->token->char_value);
    emit("jsr _push");
    end_chunk("char constant");
}

/**
 * Emit assembly for a string literal. Pushes the null-terminated char array to the stack.
 */
void emit_string_literal(struct SyntaxNode *node) {
    start_chunk("string literal");
    for (int i = 0; i < node->token->length; i++) {
        emit("lda #'%c'", *(node->token->string_value+i));
        emit("jsr _push");
    }
    emit("lda #0");
    emit("jsr _push");

    end_chunk("string literal");
}


/**
 * Emit assembly for a binary expression. The expression is evaluated in these stages: 
 * 1) evaluate the left operand, which pushes the value onto the stack. Convert type if needed.
 * 2) evaluate the right operand, which pushes the value onto the stack. Convert type if needed.
 * 3) pop the operands off the stack and compute this expression's value
 * 4) push the value of the binary expression onto the stack.
 * For short-circuiting operators (&& and ||), the left operand is popped off the stack in step 1.5.
 * If a short-circuit occurs, jump to step 4. With no short circuit, continue to step 2 as normal and step 3 only pops the right operand.
 * @param node the binary expression node
 */
void emit_binary_expression(struct SyntaxNode *node) {
    struct Label *short_circuit_label = new_label();
 
    emit_expression(node->first_child);
    if (!types_match(node->first_child->type, node->type))
        emit_type_conversion(node->first_child->type, node->type);

    // handle short-circuiting for && and ||
    if (node->token->kind == DOUBLE_AMPERSAND_TOKEN) {
        emit_and_short_circuit_check(node, short_circuit_label);
    } else if (node->token->kind == DOUBLE_PIPE_TOKEN) {
        emit_or_short_circuit_check(node, short_circuit_label);
    }

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
    case LESS_TOKEN:
        emit_less_binary_expression(node);
        break;
    case LESS_EQUALS_TOKEN:
        emit_less_equals_binary_expression(node);
        break;
    case GREATER_TOKEN:
        emit_greater_binary_expression(node);
        break;
    case GREATER_EQUALS_TOKEN:
        emit_greater_equals_binary_expression(node);
        break;
    case DOUBLE_GREATER_TOKEN:
        emit_right_shift_binary_expression(node);
        break;
    case DOUBLE_LESS_TOKEN:
        emit_left_shift_binary_expression(node);
        break;
    case DOUBLE_PIPE_TOKEN:
        emit_or_binary_expression(node, short_circuit_label);
        break;
    case DOUBLE_AMPERSAND_TOKEN:
        emit_and_binary_expression(node, short_circuit_label);
        break;
    default:
        printf(BOLD_RED "Error: can't emit binary operator %s\n" RESET, token_kind_names[node->token->kind]);
        break;
    }
}

/**
 * Emit assembly for a unary expression. Evaluates the child expression, 
 * which pushes the value onto the stack. 
 * For the addressof operator, the child's value is not evaluated and the
 * child's address is pushed to the stack instead.
 * @param node the expression node
*/
void emit_unary_expression(struct SyntaxNode *node) {
    // Addressof operator doesn't evaluate child's value
    if (node->token->kind != AMPERSAND_TOKEN)
        emit_expression(node->first_child);

    switch (node->token->kind) {
    case AMPERSAND_TOKEN:
        start_chunk("unary &");
        emit_address(node->first_child);
        end_chunk("unary &");
        break;
    case PLUS_TOKEN:
        // Unary plus does nothing for us (real C promotes to int)
        break;
    case MINUS_TOKEN:
        emit_minus_unary_expression(node);
        break;
    case BANG_TOKEN:
        emit_not_unary_expression(node);
        break;
    case STAR_TOKEN:
        emit_dereference_unary_expression(node);
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
void emit_minus_unary_expression(struct SyntaxNode *node) {
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
 * Emit assembly for a unary not expression
 * @param node expression node
*/
void emit_not_unary_expression(struct SyntaxNode *node) {
    struct Label *done_label = new_label();

    start_chunk("unary !");
    emit("stz tmp");
    for (int i = 0; i < node->type->size; i++) {
        emit("jsr _pop");
        emit("ora tmp");
        emit("sta tmp");
    }
    emit("ldx #1");
    emit("lda tmp");
    emit("beq %s", done_label->name);
    emit("ldx #0");
    emit_label(done_label);
    emit("lda #0");
    for (int i = 0; i < node->type->size - 1; i++)
        emit("jsr _push");
    emit("txa");
    emit("jsr _push");

    end_chunk("unary !");
}

/**
 * Push the address of an lvalue onto the stack
 * @param node the node
 */
void emit_address(struct SyntaxNode *node) {
    switch (node->syntax) {
    case IDENTIFIER_SYNTAX:
        emit_identifier_address(node);
        break;
    case PARENTHESIZED_EXPRESSION_SYNTAX:
        emit_address(node->first_child);
        break;
    case POSTFIX_EXPRESSION_SYNTAX:
        if (node->token->kind == OPEN_BRACKET_TOKEN) {
            emit_array_index_address(node);
        } else {
            printf(BOLD_RED "Error: can't emit address for postfix expression with token %s\n" RESET, token_kind_names[node->token->kind]);
        }
        break;
    case UNARY_EXPRESSION_SYNTAX:
        if (node->token->kind == STAR_TOKEN) {
            emit_expression(node->first_child);
        } else {
            printf(BOLD_RED "Error: can't emit address for unary expression with token %s\n" RESET, token_kind_names[node->token->kind]);
        }
        break;
    default:
        printf(BOLD_RED "Error: can't emit address for syntax kind %s\n" RESET, syntax_names[node->syntax]);
        break;
    }
}

/**
 * Emit assembly for the address of an identifier. Pushes the address to the stack.
 * @param node the identifier node
 */
void emit_identifier_address(struct SyntaxNode *node) {
    start_chunk("identifier address");
    emit("mov2 fp : tmp");
    emit("add2 tmp : #%i", node->symbol->offset + 1);
    emit("lda tmp+1");
    emit("jsr _push");
    emit("lda tmp");
    emit("jsr _push");
    end_chunk("identifier address");
}


/**
 * Emit assembly for the address of an array index expression like a[i]. Pushes the address to the stack.
 * @param node the array index expression node
 */
void emit_array_index_address(struct SyntaxNode *node) {
    start_chunk("array index address");

    // Push the array address on the stack
    emit_expression(node->first_child);

    // Push the index onto the stack
    emit_expression(node->last_child);

    if (!types_match(node->last_child->type, &INT_TYPE))
        emit_type_conversion(node->last_child->type, &INT_TYPE);

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

    end_chunk("array index address");
}


/**
 * Emit assembly for a dereference expression like *p (child already evalued and
 * pushed to the stack). Pops the address off the stack, dereferences it, and
 * pushes the value onto the stack.
 * @param node the expression node
 */
void emit_dereference_unary_expression(struct SyntaxNode *node) {
    start_chunk("unary *");
    emit("jsr _pop");
    emit("sta tmp");
    emit("jsr _pop");
    emit("sta tmp+1");
    
    if (node->type->size == 2) {
        emit("ldy #1");
        emit("lda (tmp),y");
        emit("jsr _push");
    }
    emit("lda (tmp)");
    emit("jsr _push");
    end_chunk("unary *");
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
 * Emit assembly for a || binary expression
 * @param node the expression node
 * @param short_circuit_label the label to jump to for short-circuiting
*/
void emit_or_binary_expression(struct SyntaxNode *node, struct Label *short_circuit_label) {
    struct Label *true_label = new_label();
    struct Label *done_label = new_label();

    start_chunk("|| expression");
    emit("stz tmp");
    emit("ldx #0");
    for (int i = 0; i < node->type->size; i++) {
        emit("jsr _pop");
        emit("ora tmp");
        emit("sta tmp");
    }
    emit("lda tmp");
    emit("bne %s", true_label->name);
    emit("jmp %s", done_label->name);

    emit_label(short_circuit_label);
    emit_label(true_label);
    emit("ldx #1");

    emit_label(done_label);
    emit("lda #0");
    for (int i = 0; i < node->type->size - 1; i++)
        emit("jsr _push");
    emit("txa");
    emit("jsr _push");

    end_chunk("|| expression");
}

/**
 * Emit assembly for the short-circuit check of an || expression.
 * Evaluates the left operand and jumps to the end of the expression if it's true
 * @param node the || binary expression node
 * @param short_circuit_label the label to jump to if the left operand is true
*/
void emit_or_short_circuit_check(struct SyntaxNode *node, struct Label *short_circuit_label) {
    struct Label *no_short_circuit_label = new_label();

    start_chunk("|| short-circuit check");
    emit("stz tmp");
    for (int i = 0; i < node->type->size; i++) {
        emit("jsr _pop");
        emit("ora tmp");
        emit("sta tmp");
    }
    emit("lda tmp");
    emit("beq %s", no_short_circuit_label->name);
    emit("jmp %s", short_circuit_label->name);

    emit_label(no_short_circuit_label);
    end_chunk("|| short-circuit check");
}


/**
 * Emit assembly for a && binary expression
 * @param node the expression node
*/
void emit_and_binary_expression(struct SyntaxNode *node, struct Label *short_circuit_label) {
    struct Label *false_label = new_label();
    struct Label *done_label = new_label();

    start_chunk("&& expression");
    emit("stz tmp");
    emit("ldx #1");
    for (int i = 0; i < node->type->size; i++) {
        emit("jsr _pop");
        emit("ora tmp");
        emit("sta tmp");
    }
    emit("lda tmp");
    emit("beq %s", false_label->name);
    emit("jmp %s", done_label->name);

    emit_label(short_circuit_label);
    emit_label(false_label);
    emit("ldx #0");

    emit_label(done_label);
    emit("lda #0");
    for (int i = 0; i < node->type->size - 1; i++)
        emit("jsr _push");
    emit("txa");
    emit("jsr _push");

    end_chunk("&& expression");
}


/**
 * Emit assembly for the short-circuit check of an && expression.
 * Evaluates the left operand and jumps to the end of the expression if it's false
 * @param node the && binary expression node
 * @param short_circuit_label the label to jump to if the left operand is false
*/
void emit_and_short_circuit_check(struct SyntaxNode *node, struct Label *short_circuit_label) {
    struct Label *no_short_circuit_label = new_label();

    start_chunk("&& short-circuit check");
    emit("stz tmp");
    for (int i = 0; i < node->type->size; i++) {
        emit("jsr _pop");
        emit("ora tmp");
        emit("sta tmp");
    }
    emit("lda tmp");
    emit("bne %s", no_short_circuit_label->name);
    emit("jmp %s", short_circuit_label->name);

    emit_label(no_short_circuit_label);
    end_chunk("&& short-circuit check");
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
 * Emit assembly for a < comparison expression
 * @param node the syntax node of the expreesion
*/
void emit_less_binary_expression(struct SyntaxNode *node) {
    struct Label *done_label = new_label();
    struct Label *less_label = new_label();
    struct Label *not_less_label = new_label();

    start_chunk("< expression");
    if (node->type->size == 1) {
        emit("jsr _pop");
        emit("sta tmp");
        emit("jsr _pop");
        emit("cmp tmp");
        emit("bcc %s", less_label->name);
        emit("lda #0");
        emit("jmp %s", done_label->name);
        emit_label(less_label);
        emit("lda #1");
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
        
        emit("cmp tmp+1");
        emit("bcc %s", less_label->name);
        emit("bne %s", not_less_label->name);
        emit("cpx tmp");
        emit("bcs %s", not_less_label->name);

        emit_label(less_label);
        emit("lda #0");
        emit("jsr _push");
        emit("lda #1");
        emit("jmp %s", done_label->name); 

        emit_label(not_less_label);
        emit("lda #0");
        emit("jsr _push");
        emit("lda #0");

        emit_label(done_label);
        emit("jsr _push");
    }

    end_chunk("< expression");
}

/**
 * Emit assembly for a > comparison expression
 * @param node the syntax node of the expreesion
*/
void emit_greater_binary_expression(struct SyntaxNode *node) {
    struct Label *done_label = new_label();
    struct Label *greater_label = new_label();
    struct Label *not_greater_label = new_label();

    start_chunk("> expression");
    if (node->type->size == 1) {
        emit("jsr _pop");
        emit("tax");
        emit("jsr _pop");
        emit("sta tmp");
        emit("cpx tmp");

        emit("bcc %s", greater_label->name);
        emit("lda #0");
        emit("jmp %s", done_label->name);
        emit_label(greater_label);
        emit("lda #1");
        emit_label(done_label);
        emit("jsr _push");
    } else if (node->type->size == 2) {
        emit("jsr _pop");
        emit("tax");
        emit("jsr _pop");
        emit("tay");
        emit("jsr _pop");
        emit("sta tmp");
        emit("jsr _pop");
        emit("sta tmp+1");
        
        emit("cpy tmp+1");
        emit("bcc %s", greater_label->name);
        emit("bne %s", not_greater_label->name);
        emit("cpx tmp");
        emit("bcs %s", not_greater_label->name);

        emit_label(greater_label);
        emit("lda #0");
        emit("jsr _push");
        emit("lda #1");
        emit("jmp %s", done_label->name); 

        emit_label(not_greater_label);
        emit("lda #0");
        emit("jsr _push");
        emit("lda #0");

        emit_label(done_label);
        emit("jsr _push");
    }

    end_chunk("> expression");
}

/**
 * Emit assembly for a <= comparison expression
 * @param node the syntax node of the expreesion
*/
void emit_less_equals_binary_expression(struct SyntaxNode *node) {
    start_chunk("<= expression");
    emit_greater_binary_expression(node);
    emit("ldy #1");
    emit("lda #1");
    emit("eor (sp),y");
    emit("sta (sp),y");
    end_chunk("<= expression");
}

/**
 * Emit assembly for a >= comparison expression
 * @param node the syntax node of the expreesion
*/
void emit_greater_equals_binary_expression(struct SyntaxNode *node) {
    start_chunk(">= expression");
    emit_less_binary_expression(node);
    emit("ldy #1");
    emit("lda #1");
    emit("eor (sp),y");
    emit("sta (sp),y");
    end_chunk(">= expression");
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
        emit_identifier_address(node);
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
 * Emit assembly for an assignment expression.
 * 1) compute the memory address of the lvalue and push it onto the stack
 * 2) compute the value to assign and push it onto the stack
 * 3) pop the value and the address and update the value at the address 
 * @param node the assignment expression node
*/
void emit_assignment_expression(struct SyntaxNode *node) {
    start_chunk("assignment expression");
    // compute the address and push it onto the stack
    emit_address(node->first_child);

    // compute the new value and push it onto the stack
    emit_expression(node->last_child);

    // If necessary, convert the value's type
    if (!types_match(node->type, node->last_child->type)) 
        emit_type_conversion(node->last_child->type, node->type);


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


    end_chunk("assignment expression");
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
    case OPEN_PAREN_TOKEN:
        emit_function_call_postfix_expression(node);
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
 * Emit assembly for an array index expression. Places the
 * value of the array item on the stack. (emit_array_index_address would be
 * called to push the address onto the stack)
 * @param node the SyntaxNode of the array index postfix expression
*/
void emit_array_index_postfix_expression(struct SyntaxNode *node) {
    start_chunk("array index expression");

    emit_array_index_address(node);

    // Retrieve the value at the computed address
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
    

    end_chunk("array index expression");
}

/**
 * Emit assembly for a function call postfix expression
 * @param node the SyntaxNode of the function call postfix expression
 */
void emit_function_call_postfix_expression(struct SyntaxNode *node) {
    struct Label *return_label = new_label();
    start_chunk("function call");

    // Evaluate and push the arguments onto the stack
    if (node->first_child->next != NULL) {
        struct SyntaxNode *arg = node->first_child->next;
        struct Type *param_type = node->first_child->symbol->type->param_type;
        while (arg != NULL) {
            emit_expression(arg);
            if (!types_match(arg->type, param_type))
                emit_type_conversion(arg->type, param_type);

            arg = arg->next;
            param_type = param_type->next;
        }
    }

    // Push the frame pointer onto the stack
    emit("lda fp+1");
    emit("jsr _push");
    emit("lda fp");
    emit("jsr _push");

    // Push the return address onto the stack
    emit("lda #(<(%s-1))", return_label->name);
    emit("jsr _push");
    emit("lda #(>(%s-1))", return_label->name);
    emit("jsr _push");
    
    // Jump to the function
    emit("jmp %s", node->first_child->symbol->name);

    // Re-entry point from function call
    emit_label(return_label);

    // Pop the arguments off the stack
    emit("add2 sp : #%i", node->first_child->symbol->type->param_bytes); 

    // Push the return value onto the stack
    if (node->type->size == 2) {
        emit("lda ret+1");
        emit("jsr _push");
    }
    emit("lda ret");
    emit("jsr _push");

    end_chunk("function call");
}


/**
 * Emit assembly for a return statement.
 * If the return statement has an expression, evaluates it and pushes the value onto the stack.
 * Stores the return value in ret and jumps to the return label of the function.
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

        emit("jmp _return");

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
 * Emit assembly for a for loop
 * @param node the node of the for statement
 */
void emit_for_statement(struct SyntaxNode *node) {
    struct Label *for_condition = new_label();
    struct Label *for_body = new_label();
    struct Label *end_for = new_label();

    start_chunk("for init");
    emit_expression(node->first_child);
    // pop the value of the initializer off the stack
    emit("add2 sp : #%i", node->first_child->type->size);
    end_chunk("for init");

    start_chunk("for condition");
    emit_label(for_condition);
    if (node->first_child->next->syntax != NOP_EXPRESSION_SYNTAX) {
        emit_expression(node->first_child->next);

        if (node->first_child->next->type->size == 2) {
            emit("jsr _pop");
            emit("tax");
            emit("jsr _pop");
            emit("bne %s", for_body->name);
            emit("txa");
            emit("bne %s", for_body->name);
            emit("jmp %s", end_for->name);
        } else if (node->first_child->next->type->size == 1) {
            emit("jsr _pop");
            emit("bne %s", for_body->name);
            emit("jmp %s", end_for->name);
        }
    }
    end_chunk("for condition");

    start_chunk("for body");
    emit_label(for_body);
    emit_statement(node->last_child);
    end_chunk("for body");

    start_chunk("for increment");
    emit_expression(node->first_child->next->next);
    // pop the value of the increment off the stack
    emit("add2 sp : #%i", node->first_child->next->next->type->size);
    end_chunk("for increment");
    emit("jmp %s", for_condition->name);

    emit_label(end_for);
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
    } else if (from_type->kind == ARRAY_TYPE && to_type->kind == POINTER_TYPE) {
        // no conversion needed
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