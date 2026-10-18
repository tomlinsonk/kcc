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
#include "optimize.h"
#include "parser.h"
#include "lexer.h"
#include "types.h"
#include "symbols.h"
#include "semantics.h"

#define BOLD_RED    "\033[1m\033[31m"
#define BOLD_YELLOW "\033[1m\033[33m"
#define RESET       "\033[0m"

static FILE *out;
static int n_labels = 0;
static struct Label *head_label = NULL;
static struct Label *tail_label = NULL;
static int local_offset = 0;
static int frame_size = 0;


/**
 * Get the index of a variable's first byte relative to fp, for (fp),y addressing.
 * Symbol offsets are relative to sp at function entry; fp sits frame_size bytes below that.
 * @param symbol the variable's symbol
 * @return the index (may exceed 255 for large frames)
 */
static int frame_index(struct Symbol *symbol) {
    return frame_size + symbol->offset + 1;
}

/**
 * Check whether every byte of a variable can be reached with (fp),y
 * @param node an identifier node
 */
static int fits_frame_index(struct SyntaxNode *node) {
    return frame_index(node->symbol) + get_memory_size(node->type) - 1 <= 255;
}

/**
 * Compute the number of bytes needed for locals in a subtree, where nested
 * compound statements stack on top of their parents and siblings share space.
 * @param node the root of the subtree
 */
static int max_local_bytes(struct SyntaxNode *node) {
    int own = 0;
    int max_child = 0;

    if (node->syntax == COMPOUND_STATEMENT_SYNTAX && node->first_child != NULL
            && node->first_child->syntax == DECLARATION_LIST_SYNTAX) {
        for (struct SyntaxNode *declaration = node->first_child->first_child; declaration != NULL; declaration = declaration->next)
            own += get_memory_size(declaration->last_child->type);
    }

    for (struct SyntaxNode *child = node->first_child; child != NULL; child = child->next) {
        int child_bytes = max_local_bytes(child);
        if (child_bytes > max_child)
            max_child = child_bytes;
    }

    return own + max_child;
}

/**
 * Add or subtract 1 from a variable in place with (fp),y
 * @param node an identifier node whose bytes fit in a frame index
 * @param carry_op "clc" to add or "sec" to subtract
 * @param op "adc" or "sbc"
 */
static void emit_frame_add(struct SyntaxNode *node, char *carry_op, char *op) {
    emit("ldy #%i", frame_index(node->symbol));
    emit(carry_op);
    emit("lda (fp),y");
    emit("%s #1", op);
    emit("sta (fp),y");
    if (node->type->size == 2) {
        emit("iny");
        emit("lda (fp),y");
        emit("%s #0", op);
        emit("sta (fp),y");
    }
}

static struct SyntaxNode *strip_parens(struct SyntaxNode *node) {
    while (node->syntax == PARENTHESIZED_EXPRESSION_SYNTAX)
        node = node->first_child;
    return node;
}

/**
 * Check whether an expression is a leaf that can be loaded directly instead of
 * being evaluated onto the stack: a constant or a variable reachable with (fp),y.
 * @param node the expression node
 */
static int is_leaf(struct SyntaxNode *node) {
    node = strip_parens(node);
    switch (node->syntax) {
    case INT_CONSTANT_SYNTAX:
    case CHAR_CONSTANT_SYNTAX:
        return 1;
    case IDENTIFIER_SYNTAX:
        return node->type->kind != ARRAY_TYPE && fits_frame_index(node);
    default:
        return 0;
    }
}

/**
 * Load one byte of a leaf's value into A. Bytes past the leaf's size are 0, which
 * zero-extends a char to an int. May overwrite Y.
 * @param node a leaf expression node
 * @param byte 0 for the low byte, 1 for the high byte
 */
static void emit_load_leaf_byte(struct SyntaxNode *node, int byte) {
    node = strip_parens(node);

    if (byte >= node->type->size) {
        emit("lda #0");
        return;
    }

    switch (node->syntax) {
    case INT_CONSTANT_SYNTAX:
        emit(byte ? "lda #>%i" : "lda #<%i", node->token->int_value);
        break;
    case CHAR_CONSTANT_SYNTAX:
        emit("lda #%i", node->token->char_value);
        break;
    default:
        emit("ldy #%i", frame_index(node->symbol) + byte);
        emit("lda (fp),y");
        break;
    }
}

/**
 * Move an operand into dest (low byte) and dest+1 (high byte). If the operand is
 * deferred, its leaf value is loaded directly; otherwise it is popped off the stack.
 * @param operand the operand's expression node
 * @param deferred whether the operand was left unevaluated because it's a leaf
 * @param size the number of bytes to move
 * @param dest the zero page destination, or NULL to leave a 1-byte operand in A
 */
static void emit_fetch(struct SyntaxNode *operand, int deferred, int size, char *dest) {
    for (int byte = 0; byte < size; byte++) {
        if (deferred)
            emit_load_leaf_byte(operand, byte);
        else
            emit("jsr _pop");

        if (dest != NULL)
            emit("sta %s+%i", dest, byte);
    }
}

static int is_short_circuit(struct SyntaxNode *node) {
    return node->token->kind == DOUBLE_AMPERSAND_TOKEN || node->token->kind == DOUBLE_PIPE_TOKEN;
}

/**
 * Whether a binary expression loads its right operand directly instead of evaluating it onto the stack
 */
static int defers_rhs(struct SyntaxNode *node) {
    return !is_short_circuit(node) && is_leaf(node->last_child);
}

/**
 * Whether a binary expression loads its left operand directly. Only done when the
 * right operand is also a leaf, so evaluation order can't matter.
 */
static int defers_lhs(struct SyntaxNode *node) {
    return defers_rhs(node) && is_leaf(node->first_child);
}

/**
 * Move a binary expression's right operand into dest (see emit_fetch). Must happen before emit_fetch_lhs.
 */
static void emit_fetch_rhs(struct SyntaxNode *node, char *dest) {
    emit_fetch(node->last_child, defers_rhs(node), node->type->size, dest);
}

/**
 * Move a binary expression's left operand into dest (see emit_fetch)
 */
static void emit_fetch_lhs(struct SyntaxNode *node, char *dest) {
    emit_fetch(node->first_child, defers_lhs(node), node->type->size, dest);
}

/**
 * Evaluate the operands of a binary expression that aren't loaded directly,
 * pushing them onto the stack (lhs, then rhs) converted to the expression's type.
 */
static void emit_binary_operands(struct SyntaxNode *node) {
    if (!defers_lhs(node)) {
        emit_expression(node->first_child);
        if (!types_match(node->first_child->type, node->type))
            emit_type_conversion(node->first_child->type, node->type);
    }

    if (!defers_rhs(node)) {
        emit_expression(node->last_child);
        if (!types_match(node->last_child->type, node->type))
            emit_type_conversion(node->last_child->type, node->type);
    }
}

static int is_comparison(struct SyntaxNode *node) {
    switch (node->token->kind) {
    case DOUBLE_EQUALS_TOKEN:
    case BANG_EQUALS_TOKEN:
    case LESS_TOKEN:
    case LESS_EQUALS_TOKEN:
    case GREATER_TOKEN:
    case GREATER_EQUALS_TOKEN:
        return 1;
    default:
        return 0;
    }
}

static int is_constant(struct SyntaxNode *node) {
    node = strip_parens(node);
    return node->syntax == INT_CONSTANT_SYNTAX || node->syntax == CHAR_CONSTANT_SYNTAX;
}

/**
 * Jump to target unless the given branch instruction (e.g. "bne") is taken.
 * A branch over a jmp is used because branches only reach 127 bytes.
 */
static void emit_jump_unless(char *branch, struct Label *target) {
    struct Label *skip = new_label();
    emit("%s %s", branch, skip->name);
    emit("jmp %s", target->name);
    emit_label(skip);
}

/**
 * Get an operand of a comparison ready to use as an instruction argument. Constants
 * are used as immediate values; anything else is moved into tmp_name (see emit_fetch).
 * @param src filled with the argument for each byte (e.g. "#<300" or "tmp+1")
 */
static void emit_operand_source(struct SyntaxNode *operand, int deferred, int size, char *tmp_name, char src[2][16]) {
    struct SyntaxNode *leaf = strip_parens(operand);

    if (deferred && is_constant(leaf)) {
        for (int byte = 0; byte < size; byte++) {
            if (byte >= leaf->type->size)
                strcpy(src[byte], "#0");
            else if (leaf->syntax == CHAR_CONSTANT_SYNTAX)
                sprintf(src[byte], "#%i", leaf->token->char_value);
            else
                sprintf(src[byte], byte ? "#>%i" : "#<%i", leaf->token->int_value);
        }
        return;
    }

    emit_fetch(operand, deferred, size, tmp_name);
    for (int byte = 0; byte < size; byte++)
        sprintf(src[byte], "%s+%i", tmp_name, byte);
}

/**
 * Compare a comparison expression's operands and jump to target if the
 * comparison's result is when_true. Ints compare signed, chars unsigned.
 */
static void emit_comparison_branch(struct SyntaxNode *node, struct Label *target, int when_true) {
    int kind = node->token->kind;
    int size = node->type->size;
    // Every comparison is lhs == rhs or x < y, possibly negated, with x and y possibly swapped
    int equality = kind == DOUBLE_EQUALS_TOKEN || kind == BANG_EQUALS_TOKEN;
    int negate = kind == BANG_EQUALS_TOKEN || kind == GREATER_EQUALS_TOKEN || kind == LESS_EQUALS_TOKEN;
    int swap = kind == GREATER_TOKEN || kind == LESS_EQUALS_TOKEN;
    int jump_if = when_true != negate;
    char lhs[2][16], rhs[2][16];

    start_chunk("comparison");
    emit_binary_operands(node);
    emit_operand_source(node->last_child, defers_rhs(node), size, "tmp", rhs);
    emit_operand_source(node->first_child, defers_lhs(node), size, "tmp+2", lhs);

    char (*x)[16] = swap ? rhs : lhs;
    char (*y)[16] = swap ? lhs : rhs;

    emit("lda %s", x[0]);
    emit("cmp %s", y[0]);

    if (equality && size == 1) {
        emit_jump_unless(jump_if ? "bne" : "beq", target);
    } else if (equality && jump_if) {
        struct Label *skip = new_label();
        emit("bne %s", skip->name);
        emit("lda %s", x[1]);
        emit("cmp %s", y[1]);
        emit("bne %s", skip->name);
        emit("jmp %s", target->name);
        emit_label(skip);
    } else if (equality) {
        struct Label *jump = new_label();
        struct Label *skip = new_label();
        emit("bne %s", jump->name);
        emit("lda %s", x[1]);
        emit("cmp %s", y[1]);
        emit("beq %s", skip->name);
        emit_label(jump);
        emit("jmp %s", target->name);
        emit_label(skip);
    } else if (size == 1) {
        // Carry is clear if x < y (unsigned)
        emit_jump_unless(jump_if ? "bcs" : "bcc", target);
    } else {
        // The sign of x - y, corrected for overflow (N xor V), is set if x < y (signed)
        struct Label *no_overflow = new_label();
        emit("lda %s", x[1]);
        emit("sbc %s", y[1]);
        emit("bvc %s", no_overflow->name);
        emit("eor #$80");
        emit_label(no_overflow);
        emit_jump_unless(jump_if ? "bpl" : "bmi", target);
    }
    end_chunk("comparison");
}

/**
 * Evaluate an expression and jump to target if its truth value (nonzero) is when_true
 */
static void emit_truth_branch(struct SyntaxNode *node, struct Label *target, int when_true) {
    struct SyntaxNode *leaf = strip_parens(node);
    int size = node->type->kind == ARRAY_TYPE ? 2 : node->type->size;
    int deferred = is_leaf(node);

    if (is_constant(leaf)) {
        int value = leaf->syntax == INT_CONSTANT_SYNTAX ? leaf->token->int_value : leaf->token->char_value;
        if ((value != 0) == when_true)
            emit("jmp %s", target->name);
        return;
    }

    if (!deferred)
        emit_expression(node);

    // Load the value so that Z is set if it's zero
    if (deferred)
        emit_load_leaf_byte(node, 0);
    else
        emit("jsr _pop");

    if (size == 2) {
        emit("sta tmp");
        if (deferred)
            emit_load_leaf_byte(node, 1);
        else
            emit("jsr _pop");
        emit("ora tmp");
    }

    emit_jump_unless(when_true ? "beq" : "bne", target);
}

/**
 * Emit assembly that jumps to target if the condition's truth value is when_true,
 * and otherwise falls through. Comparisons, &&, ||, and ! branch directly instead
 * of computing a 0 or 1. Leaves the stack unchanged.
 * @param node the condition expression
 * @param target the label to jump to
 * @param when_true 1 to jump if the condition is true, 0 to jump if it's false
 */
void emit_branch(struct SyntaxNode *node, struct Label *target, int when_true) {
    node = strip_parens(node);

    if (node->syntax == UNARY_EXPRESSION_SYNTAX && node->token->kind == BANG_TOKEN) {
        emit_branch(node->first_child, target, !when_true);
    } else if (node->syntax == BINARY_EXPRESSION_SYNTAX && node->token->kind == DOUBLE_AMPERSAND_TOKEN) {
        if (when_true) {
            struct Label *skip = new_label();
            emit_branch(node->first_child, skip, 0);
            emit_branch(node->last_child, target, 1);
            emit_label(skip);
        } else {
            emit_branch(node->first_child, target, 0);
            emit_branch(node->last_child, target, 0);
        }
    } else if (node->syntax == BINARY_EXPRESSION_SYNTAX && node->token->kind == DOUBLE_PIPE_TOKEN) {
        if (when_true) {
            emit_branch(node->first_child, target, 1);
            emit_branch(node->last_child, target, 1);
        } else {
            struct Label *skip = new_label();
            emit_branch(node->first_child, skip, 1);
            emit_branch(node->last_child, target, 0);
            emit_label(skip);
        }
    } else if (node->syntax == BINARY_EXPRESSION_SYNTAX && is_comparison(node)) {
        emit_comparison_branch(node, target, when_true);
    } else {
        emit_truth_branch(node, target, when_true);
    }
}

/**
 * Push the value (0 or 1) of a comparison, &&, ||, or ! expression
 * @param node the expression node
 */
static void emit_boolean_value(struct SyntaxNode *node) {
    struct Label *true_label = new_label();
    struct Label *done_label = new_label();

    start_chunk("boolean value");
    emit_branch(node, true_label, 1);
    emit("lda #0");
    emit("bra %s", done_label->name);
    emit_label(true_label);
    emit("lda #1");
    emit_label(done_label);

    if (node->type->size == 2) {
        emit("tax");
        emit("lda #0");
        emit("jsr _push");
        emit("txa");
    }
    emit("jsr _push");
    end_chunk("boolean value");
}


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
 * @param out_path the file to write the assembly to
 */
void emit_translation_unit(struct SyntaxNode *root, char *out_path) {
    FILE *template = fopen("template.asm", "r");
    FILE *out_file = fopen(out_path, "w");
    char *code;
    size_t code_size;
    char c;
    struct SyntaxNode *node;

    while ((c = fgetc(template)) != EOF)
        fputc(c, out_file);

    fclose(template);

    // Emit into memory so the code can be optimized before writing it out
    out = open_memstream(&code, &code_size);
    start_chunk("literals");
    for (int i = 0; i < literal_list.size; i++) {
        node = literal_list.array[i];
        emit_label(node->label);
        emit(".text \"%.*s\"", node->token->length, node->token->string_value);
        emit(".byte 0");
    }
    end_chunk("literals");


    for (node = root->first_child; node != NULL; node = node->next) {
        if (node->syntax == FUNCTION_DEFINITION_SYNTAX) {
            emit_function_definition(node);
        } else {
            printf(BOLD_YELLOW "Warning: no emitter support for external declarations.\n" RESET);
        }
    }

    fclose(out);
    optimize(code, out_file);
    free(code);
    fclose(out_file);
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
    // Allocate all locals and point the frame pointer at the bottom of the frame
    frame_size = max_local_bytes(body);
    if (frame_size > 0)
        emit("add2 sp : #-%i", frame_size);
    emit("mov2 sp : fp");

    end_chunk("function setup");

    // Emit function body
    emit_compound_statement(node->last_child);

    // Return from the function
    start_chunk("function cleanup");
    emit_named_label("_return");

    // restore stack pointer
    if (frame_size > 0)
        emit("add2 fp : #%i : sp", frame_size);
    else
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
 * Emit assembly for a compound statement. Assigns frame offsets to its locals
 * (space for them is allocated when the function starts).
 * @param node the compound statement node 
 */
void emit_compound_statement(struct SyntaxNode *node) {
    int n_local_bytes = 0;

    if (node->first_child != NULL && node->first_child->syntax == DECLARATION_LIST_SYNTAX) {
        for (struct SyntaxNode *declaration = node->first_child->first_child; declaration != NULL; declaration = declaration->next) {
            n_local_bytes += get_memory_size(declaration->last_child->type);
            declaration->last_child->first_child->symbol->offset = -(n_local_bytes + local_offset);
        }

        // Nested compound statements put their locals below these
        local_offset += n_local_bytes;
    }

    // Emit all statements
    if (node->last_child != NULL && node->last_child->syntax == STATEMENT_LIST_SYNTAX) {
        node = node->last_child->first_child;
        while (node != NULL) {
            emit_statement(node);
            node = node->next;
        }
    }

    local_offset -= n_local_bytes;
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
 * Emit assembly for an expression statement. The expression is evaluated for its
 * side effects only, so its value isn't left on the stack.
 * @param node the expression statement node
 */
void emit_expression_statement(struct SyntaxNode *node) {
    if (node->first_child != NULL)
        emit_discarded_expression(node->first_child);
}

/**
 * Emit assembly for an expression whose value is unused, leaving nothing on the stack.
 * Assignments, ++, --, and function calls skip pushing their result; other expressions
 * are evaluated and then popped.
 * @param node the expression node
 */
void emit_discarded_expression(struct SyntaxNode *node) {
    switch (node->syntax) {
    case NOP_EXPRESSION_SYNTAX:
        return;
    case PARENTHESIZED_EXPRESSION_SYNTAX:
        emit_discarded_expression(node->first_child);
        return;
    case ASSIGNMENT_EXPRESSION_SYNTAX:
        emit_assignment_expression(node, 0);
        return;
    case POSTFIX_EXPRESSION_SYNTAX:
        if (node->token->kind == PLUS_PLUS_TOKEN) {
            emit_increment_postfix_expression(node, 0);
            return;
        } else if (node->token->kind == MINUS_MINUS_TOKEN) {
            emit_decrement_postfix_expression(node, 0);
            return;
        } else if (node->token->kind == OPEN_PAREN_TOKEN) {
            emit_function_call_postfix_expression(node, 0);
            return;
        }
        break;
    default:
        break;
    }

    emit_expression(node);

    // Arrays (including string literals) are pushed as their 2-byte address
    int pushed = node->type->kind == ARRAY_TYPE ? 2 : node->type->size;
    if (pushed > 0)
        emit("add2 sp : #%i", pushed);
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
        emit_assignment_expression(node, 1);
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
 * Emit assembly for a string literal. Pushes the addess of the literal.
 */
void emit_string_literal(struct SyntaxNode *node) {
    start_chunk("string literal");
    emit("lda #>%s", node->label->name);
    emit("jsr _push");
    emit("lda #<%s", node->label->name);
    emit("jsr _push");
    end_chunk("string literal");
}


/**
 * Emit assembly for a binary expression. The expression is evaluated in these stages: 
 * 1) evaluate the left operand, which pushes the value onto the stack. Convert type if needed.
 * 2) evaluate the right operand, which pushes the value onto the stack. Convert type if needed.
 * 3) pop the operands off the stack and compute this expression's value
 * 4) push the value of the binary expression onto the stack.
 * Constant and variable operands skip steps 1 and 2 and are loaded directly in step 3.
 * Comparisons, && and || are emitted as branches (see emit_branch) that push 0 or 1.
 * @param node the binary expression node
 */
void emit_binary_expression(struct SyntaxNode *node) {
    if (is_comparison(node) || is_short_circuit(node)) {
        emit_boolean_value(node);
        return;
    }

    // Leaf operands are loaded directly by the operator instead of being pushed
    emit_binary_operands(node);
    
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
 * Emit assembly for a unary expression. Evaluates the child expression, 
 * which pushes the value onto the stack. 
 * For the addressof operator, the child's value is not evaluated and the
 * child's address is pushed to the stack instead.
 * @param node the expression node
*/
void emit_unary_expression(struct SyntaxNode *node) {
    if (node->token->kind == BANG_TOKEN) {
        emit_boolean_value(node);
        return;
    }

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
    int index = frame_index(node->symbol);

    start_chunk("identifier address");
    emit("clc");
    emit("lda fp");
    emit("adc #<%i", index);
    emit("tax");
    emit("lda fp+1");
    emit("adc #>%i", index);
    emit("jsr _push");
    emit("txa");
    emit("jsr _push");
    end_chunk("identifier address");
}


/**
 * Emit assembly for the address of an array index expression like a[i]. Pushes the address to the stack.
 * @param node the array index expression node
 */
void emit_array_index_address(struct SyntaxNode *node) {
    start_chunk("array index address");
    emit_array_index_address_to_tmp(node);
    emit("lda tmp+1");
    emit("jsr _push");
    emit("lda tmp");
    emit("jsr _push");
    end_chunk("array index address");
}


/**
 * Compute the address of an array index expression like a[i] into tmp, tmp+1.
 * Local arrays, and pointer variables indexed by a leaf, are loaded directly
 * instead of being evaluated onto the stack.
 * @param node the array index expression node
 */
void emit_array_index_address_to_tmp(struct SyntaxNode *node) {
    struct SyntaxNode *base = strip_parens(node->first_child);
    struct SyntaxNode *index = node->last_child;
    int index_deferred = is_leaf(index);
    int base_is_local_array = base->syntax == IDENTIFIER_SYNTAX && base->type->kind == ARRAY_TYPE;
    int base_deferred = base_is_local_array || (index_deferred && is_leaf(base));
    int element_size = get_memory_size(node->type);

    if (!base_deferred)
        emit_expression(node->first_child);

    if (!index_deferred) {
        emit_expression(index);
        if (!types_match(index->type, &INT_TYPE))
            emit_type_conversion(index->type, &INT_TYPE);
    }

    // Compute the byte offset of the element in tmp
    emit_fetch(index, index_deferred, 2, "tmp");
    if (element_size == 2) {
        emit("asl tmp");
        emit("rol tmp+1");
    } else if (element_size > 2) {
        emit("lda #<%i", element_size);
        emit("sta tmp+2");
        emit("lda #>%i", element_size);
        emit("sta tmp+3");
        emit("jsr _mul16");
        emit("mov2 tmp+4 : tmp");
    }

    // Get the base address in tmp+2
    if (base_is_local_array) {
        emit("add2 fp : #%i : tmp+2", frame_index(base->symbol));
    } else {
        emit_fetch(base, base_deferred, 2, "tmp+2");
    }

    emit("add2 tmp : tmp+2");
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
        emit_fetch_rhs(node, "tmp");
        if (defers_lhs(node)) {
            emit_fetch_lhs(node, "tmp+2");
            emit("clc");
            emit("lda tmp+2");
            emit("adc tmp");
            emit("tax");
            emit("lda tmp+3");
            emit("adc tmp+1");
            emit("jsr _push");
            emit("txa");
            emit("jsr _push");
        } else {
            emit_two_byte_add();
        }
    } else if (node->type->size == 1) {
        if (defers_lhs(node)) {
            emit_fetch_rhs(node, "tmp");
            emit_fetch_lhs(node, NULL);
            emit("clc");
            emit("adc tmp");
            emit("jsr _push");
        } else {
            emit_fetch_rhs(node, NULL);
            emit("ldy #1");
            emit("clc");
            emit("adc (sp),y");     
            emit("sta (sp),y");
        }
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
        emit_fetch_rhs(node, "tmp");
        if (defers_lhs(node)) {
            emit_fetch_lhs(node, "tmp+2");
            emit("sec");
            emit("lda tmp+2");
            emit("sbc tmp");
            emit("tax");
            emit("lda tmp+3");
            emit("sbc tmp+1");
            emit("jsr _push");
            emit("txa");
            emit("jsr _push");
        } else {
            emit("ldy #1");
            emit("lda (sp),y");
            emit("sec");
            emit("sbc tmp");
            emit("sta (sp),y");
            emit("iny");
            emit("lda (sp),y");
            emit("sbc tmp+1");
            emit("sta (sp),y");
        }
    } else if (node->type->size == 1) {
        emit_fetch_rhs(node, "tmp");
        if (defers_lhs(node)) {
            emit_fetch_lhs(node, NULL);
            emit("sec");
            emit("sbc tmp");
            emit("jsr _push");
        } else {
            emit("ldy #1");
            emit("lda (sp),y");
            emit("sec");
            emit("sbc tmp");     
            emit("sta (sp),y");
        }
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
        emit_fetch_rhs(node, "tmp");
        emit_fetch_lhs(node, "tmp+2");
        emit_two_byte_multiply();
    } else if (node->type->size == 1) {
        emit_fetch_rhs(node, "tmp");
        emit_fetch_lhs(node, "tmp+1");
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
        emit_fetch_rhs(node, "tmp+2");
        emit_fetch_lhs(node, "tmp");
        emit("jsr _div16");
        emit("lda tmp+1");
        emit("jsr _push");
        emit("lda tmp");
        emit("jsr _push");
    } else if (types_match(node->type, &CHAR_TYPE)) {
        // Unsigned division for char
        emit_fetch_rhs(node, "tmp+1");
        emit_fetch_lhs(node, "tmp");
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
        emit_fetch_rhs(node, "tmp+2");
        emit_fetch_lhs(node, "tmp");
        emit("jsr _mod16");
        emit("lda tmp+5");
        emit("jsr _push");
        emit("lda tmp+4");
        emit("jsr _push");
    } else if (types_match(node->type, &CHAR_TYPE)) {
        // Unsigned division for char
        emit_fetch_rhs(node, "tmp+1");
        emit_fetch_lhs(node, "tmp");
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
    struct Label *push_result = new_label();
    struct Label *done = new_label();

    emit_fetch_rhs(node, "tmp+2");
    emit_fetch_lhs(node, "tmp");

    emit("ldx tmp+2");
    if (node->type->size == 2) {
        emit("lda tmp+3");
        emit("bne %s", all_zeros->name); // if hi byte of shift count is nonzero, result is 0
    }
    emit("cpx #%i", 8 * node->type->size); // shifting by at least the bit width gives 0
    emit("bcs %s", all_zeros->name);
    emit("cpx #0");
    emit("beq %s", push_result->name);   // shifting by 0 leaves the value unchanged

    emit_label(loop);
    if (node->type->size == 2) {
        emit(left ? "asl tmp" : "lsr tmp+1");
        emit(left ? "rol tmp+1" : "ror tmp");
    } else {
        emit(left ? "asl tmp" : "lsr tmp");
    }
    emit("dex");
    emit("bne %s", loop->name);

    emit_label(push_result);
    if (node->type->size == 2) {
        emit("lda tmp+1");
        emit("jsr _push");
    }
    emit("lda tmp");
    emit("jsr _push");
    emit("jmp %s", done->name);

    emit_label(all_zeros);
    emit("lda #0");
    emit("jsr _push");
    if (node->type->size == 2) {
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
 * Emit assembly to push an identifier onto the stack. If it's an array,
 * pushes the starting address, otherwise pushes the value of the identifier. 
 * @param node the identifier node
 */
void emit_identifier(struct SyntaxNode *node) {
    int index = frame_index(node->symbol);

    start_chunk("identifier");

    if (node->type->kind == ARRAY_TYPE) {
        emit_identifier_address(node);
    } else if (fits_frame_index(node)) {
        if (node->type->size == 2) {
            emit("ldy #%i", index + 1);
            emit("lda (fp),y");
            emit("jsr _push");
            emit("dey");
        } else {
            emit("ldy #%i", index);
        }
        emit("lda (fp),y");
        emit("jsr _push");
    } else {
        emit("add2 fp : #%i : tmp", index);
        if (node->type->size == 2) {
            emit("ldy #1");
            emit("lda (tmp),y");
            emit("jsr _push");
        }
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
 * Assignments to variables in the frame skip step 1 and store with (fp),y.
 * @param node the assignment expression node
 * @param push_result whether to push the assigned value as the expression's value
*/
void emit_assignment_expression(struct SyntaxNode *node, int push_result) {
    struct SyntaxNode *lvalue = node->first_child;
    struct SyntaxNode *value = node->last_child;
    int direct = lvalue->syntax == IDENTIFIER_SYNTAX && fits_frame_index(lvalue);
    int value_deferred = is_leaf(value);

    start_chunk("assignment expression");

    // compute the address and push it onto the stack
    if (!direct)
        emit_address(lvalue);

    // compute the new value and push it onto the stack (leaves are loaded directly later)
    if (!value_deferred) {
        emit_expression(value);

        // If necessary, convert the value's type
        if (!types_match(node->type, value->type)) 
            emit_type_conversion(value->type, node->type);
    }

    if (direct) {
        int index = frame_index(lvalue->symbol);
        if (value_deferred) {
            // Store each byte, and push it as the expression's value (high byte first)
            for (int i = node->type->size - 1; i >= 0; i--) {
                emit_load_leaf_byte(value, i);
                emit("ldy #%i", index + i);
                emit("sta (fp),y");
                if (push_result)
                    emit("jsr _push");
            }
        } else if (push_result) {
            // Copy the value from the top of the stack into the variable, leaving it on the stack
            for (int i = 0; i < node->type->size; i++) {
                emit("ldy #%i", i + 1);
                emit("lda (sp),y");
                emit("ldy #%i", index + i);
                emit("sta (fp),y");
            }
        } else {
            // Pop the value into the variable (low byte first)
            for (int i = 0; i < node->type->size; i++) {
                emit("jsr _pop");
                emit("ldy #%i", index + i);
                emit("sta (fp),y");
            }
        }
        end_chunk("assignment expression");
        return;
    }

    // Move the value into tmp (and tmp+1 if two bytes)
    emit_fetch(value, value_deferred, node->type->size, "tmp");

    // Pop the address and store in tmp+2 and tmp+3
    emit("jsr _pop");
    emit("sta tmp+2");
    emit("jsr _pop");
    emit("sta tmp+3");

    // Store the value at the address, and push it back onto the stack
    emit("lda tmp");
    emit("sta (tmp+2)");
    if (node->type->size == 2) {
        emit("ldy #1"); 
        emit("lda tmp+1");
        emit("sta (tmp+2),y");
        if (push_result) {
            emit("jsr _push");
            emit("lda tmp");
        }
    }
    if (push_result)
        emit("jsr _push");


    end_chunk("assignment expression");
}


/**
 * Emit assembly for a postfix expression
 * @param node the postfix expression syntax node
*/
void emit_postfix_expression(struct SyntaxNode *node) {
    switch (node->token->kind) {
    case PLUS_PLUS_TOKEN:
        emit_increment_postfix_expression(node, 1);
        break;
    case MINUS_MINUS_TOKEN:
        emit_decrement_postfix_expression(node, 1);
        break;
    case OPEN_BRACKET_TOKEN:
        emit_array_index_postfix_expression(node);
        break;
    case OPEN_PAREN_TOKEN:
        emit_function_call_postfix_expression(node, 1);
        break;
    default:
        printf(BOLD_RED "Error: can't emit postfix operator %s\n" RESET, token_kind_names[node->token->kind]);
        break;
    }
}

/**
 * Emit assembly for a ++ postfix expression
 * @param the node of the postfix expression
 * @param push_result whether to push the old value as the expression's value
*/
void emit_increment_postfix_expression(struct SyntaxNode *node, int push_result) {
    if (push_result)
        emit_expression(node->first_child);

    start_chunk("++ expression");
    if (node->first_child->syntax == IDENTIFIER_SYNTAX && fits_frame_index(node->first_child)) {
        emit_frame_add(node->first_child, "clc", "adc");
        end_chunk("++ expression");
        return;
    }

    emit_address(node->first_child);
    emit("jsr _pop");
    emit("sta tmp");
    emit("jsr _pop");
    emit("sta tmp+1");
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
 * @param push_result whether to push the old value as the expression's value
*/
void emit_decrement_postfix_expression(struct SyntaxNode *node, int push_result) {
    if (push_result)
        emit_expression(node->first_child);
    start_chunk("-- expression");
    if (node->first_child->syntax == IDENTIFIER_SYNTAX && fits_frame_index(node->first_child)) {
        emit_frame_add(node->first_child, "sec", "sbc");
        end_chunk("-- expression");
        return;
    }

    emit_address(node->first_child);
    emit("jsr _pop");
    emit("sta tmp");
    emit("jsr _pop");
    emit("sta tmp+1");
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

    emit_array_index_address_to_tmp(node);

    // Retrieve the value at the computed address
    if (node->type->size == 2) {
        emit("ldy #1");
        emit("lda (tmp),y");
        emit("jsr _push");
    }

    emit("lda (tmp)");
    emit("jsr _push");
    

    end_chunk("array index expression");
}

/**
 * Emit assembly for a function call postfix expression
 * @param node the SyntaxNode of the function call postfix expression
 * @param push_result whether to push the return value (if any) onto the stack
 */
void emit_function_call_postfix_expression(struct SyntaxNode *node, int push_result) {
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
    if (node->first_child->symbol->type->param_bytes > 0)
        emit("add2 sp : #%i", node->first_child->symbol->type->param_bytes); 

    if (push_result && !(node->type->kind == PRIMITIVE_TYPE && node->type->primitive == VOID_PRIMITIVE)) {
        // Push the return value onto the stack
        if (node->type->size == 2) {
            emit("lda ret+1");
            emit("jsr _push");
        }
        emit("lda ret");
        emit("jsr _push");
    }
    
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
        int deferred = is_leaf(node->first_child);

        if (!deferred) {
            emit_expression(node->first_child);

            if (!types_match(node->first_child->type, node->type))
                emit_type_conversion(node->first_child->type, node->type);
        }

        start_chunk("return");

        emit_fetch(node->first_child, deferred, node->type->size, "ret");

        emit("jmp _return");

        end_chunk("return");
    }
}

/**
 * Emit assembly for an if or if-else statement
 * @param node the SyntaxNode of the conditional statement
*/
void emit_conditional_statement(struct SyntaxNode *node, int has_else) {
    struct Label *end_if = new_label();
    struct Label *end_else = new_label();

    start_chunk("if condition");
    emit_branch(node->first_child, end_if, 0);
    end_chunk("if condition");

    start_chunk("if body");
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
    struct Label *end_while = new_label();

    start_chunk("while condition");
    emit_label(while_condition);
    emit_branch(node->first_child, end_while, 0);
    end_chunk("while condition");

    start_chunk("while body");
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
    struct Label *end_for = new_label();

    start_chunk("for init");
    emit_discarded_expression(node->first_child);
    end_chunk("for init");

    start_chunk("for condition");
    emit_label(for_condition);
    if (node->first_child->next->syntax != NOP_EXPRESSION_SYNTAX)
        emit_branch(node->first_child->next, end_for, 0);
    end_chunk("for condition");

    start_chunk("for body");
    emit_statement(node->last_child);
    end_chunk("for body");

    start_chunk("for increment");
    emit_discarded_expression(node->first_child->next->next);
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




/**
 * Multiply the ints in tmp,tmp+1 and tmp+2,tmp+3 and push the product
 */
void emit_two_byte_multiply() {
    emit("jsr _mul16");
    emit("lda tmp+5");
    emit("jsr _push");
    emit("lda tmp+4");
    emit("jsr _push");
}


/**
 * Add the int in tmp,tmp+1 to the int on top of the stack, in place
 */
void emit_two_byte_add() {
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