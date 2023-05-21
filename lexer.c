#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>


enum token_type { 
    START_TOKEN,
    INT_TOKEN,
    PLUS_TOKEN,
    MINUS_TOKEN,
    STAR_TOKEN,
    SLASH_TOKEN,
    OPEN_PAREN_TOKEN,
    CLOSE_PAREN_TOKEN,
    OPEN_BRACE_TOKEN,
    CLOSE_BRACE_TOKEN,
    OPEN_BRACKET_TOKEN,
    CLOSE_BRACKET_TOKEN,
    SEMICOLON_TOKEN,
    END_TOKEN,
    UNKNOWN_TOKEN
};

const char *token_type_names[] = {"START", "INT", "+", "-", "*", "/", "(", ")", "{", "}", "[", "]", ";", "END", "UNKNOWN"};

struct token {
    enum token_type type;
    int index;
    int length;
    void *value;
    struct token *next;
};


/**
 * Return the size of a file
 * @param *fp: pointer to file
*/
int fsize(FILE *fp) {
    fseek(fp, 0, SEEK_END); 
    int size = ftell(fp);
    rewind(fp);

    return size;
}

/**
 * Parse an int value from a subarray of input, return a pointer to int
*/
int *get_int_value(char input[], int start, int length) {
    int *n = malloc(sizeof(int));
    for (int i = start; i < start + length; i++) {
        *n = 10 * (*n) + (input[i] - '0');
    }
    
    return n;
}

/**
 * Free every token and value in the given linked list
*/
void free_tokens(struct token *head) {
    struct token *current = head;
    struct token *next = head->next;

    while (current != NULL) {
        next = current->next;
        free(current->value);
        free(current);
        current = next;
    }
}

/**
 * Parse the char array into a linked chain of tokens.
 * @return a pointer to the first token in the chain
*/
struct token *lex(char input[]) {

    struct token *head = malloc(sizeof(struct token));
    head->type = START_TOKEN;

    struct token *tail = malloc(sizeof(struct token));
    tail->type = END_TOKEN;

    head->next = tail;

    struct token *cur_token = head;
    struct token *new_token;


    int i = 0;
    while (1) {
        // consume spaces
        while (isspace(input[i])) 
            i++;
        

        if (input[i] == '\0')
            break;

        new_token = malloc(sizeof(struct token));
        new_token->index = i;
        new_token->next = tail;


        if (isdigit(input[i])) {
            while (isdigit(input[i])) 
                i++;
            
            new_token->length = i - new_token->index;
            new_token->type = INT_TOKEN;
            new_token->value = get_int_value(input, new_token->index, new_token->length);
        } else if (strchr("+-*/(){}[];", input[i]) != NULL) {
            new_token->length = 1;

            switch (input[i]) {
            case '+':
                new_token->type = PLUS_TOKEN;
                break;
            case '-':
                new_token->type = MINUS_TOKEN;
                break;
            case '*':
                new_token->type = STAR_TOKEN;
                break;
            case '/':
                new_token->type = SLASH_TOKEN;
                break;            
            case '(':
                new_token->type = OPEN_PAREN_TOKEN;
                break;
            case ')':
                new_token->type = CLOSE_PAREN_TOKEN;
                break;
            case '{':
                new_token->type = OPEN_BRACE_TOKEN;
                break;
            case '}':
                new_token->type = CLOSE_BRACE_TOKEN;
                break;
            case '[':
                new_token->type = OPEN_BRACKET_TOKEN;
                break;
            case ']':
                new_token->type = CLOSE_BRACKET_TOKEN;
                break;
            case ';':
                new_token->type = SEMICOLON_TOKEN;
                break;
            }
            i++;
        } else {
            new_token->type = UNKNOWN_TOKEN;
            new_token->index = i;
            new_token-> length = 1;
            i++;
        }

        cur_token->next = new_token;
        cur_token = new_token;
    }

    return head;
}

void print_tokens(char input[], struct token *current) {
    // skip start token
    // current = current->next; 

    while (current != NULL) {
        printf("%s ", token_type_names[current->type]);
        printf("%.*s\n", current->length, input + current->index);
        current = current->next;
    }
}




int main() {
    FILE *fp = fopen("input.txt", "r");

    int size = fsize(fp);

    char input[size+1];

    for (int i = 0; i < size; i++) {
        input[i] = getc(fp);
    }
    input[size] = '\0';

    struct token *head = lex(input);

    print_tokens(input, head);
    
    free_tokens(head);

    return 0;
}