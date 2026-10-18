/**
 * @file optimize.c
 * @brief Optimizations on emitted 65C02 assembly
 *
 * The emitter evaluates every expression on the C stack, so it often pushes a
 * value and immediately pops it again. jsr _push followed by jsr _pop (with only
 * comments between them) leaves A, X, Y, C, V, and sp unchanged, so the pair can
 * be removed. The only difference is the N and Z flags, which _pop sets from A.
 * If later code may read those flags, the pair is replaced with cmp #0, which
 * sets N and Z the same way.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "optimize.h"

enum FlagUse { FLAGS_READ, FLAGS_SET, FLAGS_UNTOUCHED };

// Anything not in these lists (branches, labels, jumps, ...) is assumed to read N and Z
static const char *SETS_NZ[] = {
    "lda", "ldx", "ldy", "tax", "tay", "txa", "tya", "tsx", "pla", "plx", "ply", "plp",
    "and", "ora", "eor", "adc", "sbc", "cmp", "cpx", "cpy", "bit",
    "inc", "dec", "inx", "iny", "dex", "dey", "asl", "lsr", "rol", "ror",
    "mov", "mov2", "add2", "inc2", "dec2",  // macros from template.asm that start by setting N and Z
    NULL
};

static const char *LEAVES_NZ[] = {
    "sta", "stx", "sty", "stz", "pha", "phx", "phy",
    "clc", "sec", "cli", "sei", "cld", "sed", "clv", "nop", NULL
};


static char *trim(char *line) {
    while (*line == ' ' || *line == '\t') line++;
    return line;
}

static int is_comment(char *line) {
    line = trim(line);
    return *line == '\0' || strncmp(line, "//", 2) == 0;
}

static int is_line(char *line, char *text) {
    return strcmp(trim(line), text) == 0;
}

static int mnemonic_in(char *line, const char **list) {
    char *start = trim(line);
    size_t len = strcspn(start, " \t");

    for (int i = 0; list[i] != NULL; i++) {
        if (strlen(list[i]) == len && strncmp(start, list[i], len) == 0)
            return 1;
    }
    return 0;
}

/**
 * Determine whether the code starting at lines[start] may read the N or Z flags
 * before setting them. Labels, jumps, and anything unrecognized count as reads.
 */
static enum FlagUse next_flag_use(char **lines, int n_lines, int start) {
    for (int i = start; i < n_lines; i++) {
        char *line = lines[i];

        if (is_comment(line))
            continue;
        if (is_line(line, "jsr _push") || is_line(line, "jsr _pop"))
            return FLAGS_SET;
        if (mnemonic_in(line, LEAVES_NZ))
            continue;
        if (mnemonic_in(line, SETS_NZ))
            return FLAGS_SET;
        return FLAGS_READ;
    }
    return FLAGS_UNTOUCHED;
}

/**
 * Optimize the assembly in code and write the result to out.
 * @param code newline-separated assembly (modified in place)
 * @param out the file to write to
 */
void optimize(char *code, FILE *out) {
    int n_lines = 0;
    int capacity = 1024;
    char **lines = malloc(capacity * sizeof(char *));
    char *line;

    // Split code into lines
    for (line = strtok(code, "\n"); line != NULL; line = strtok(NULL, "\n")) {
        if (n_lines == capacity) {
            capacity *= 2;
            lines = realloc(lines, capacity * sizeof(char *));
        }
        lines[n_lines++] = line;
    }

    // Output is built as a stack of lines so that removing a pair can expose
    // another removable pair (e.g. push, push, pop, pop).
    char **kept = malloc(n_lines * sizeof(char *));
    int n_kept = 0;

    for (int i = 0; i < n_lines; i++) {
        if (is_line(lines[i], "jsr _pop")) {
            int j = n_kept - 1;
            while (j >= 0 && is_comment(kept[j]))
                j--;

            if (j >= 0 && is_line(kept[j], "jsr _push")) {
                memmove(&kept[j], &kept[j + 1], (n_kept - j - 1) * sizeof(char *));
                n_kept--;

                if (next_flag_use(lines, n_lines, i + 1) != FLAGS_SET)
                    kept[n_kept++] = "\tcmp #0";
                continue;
            }
        }
        kept[n_kept++] = lines[i];
    }

    for (int i = 0; i < n_kept; i++)
        fprintf(out, "%s\n", kept[i]);

    free(kept);
    free(lines);
}
