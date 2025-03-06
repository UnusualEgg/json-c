#include "json.h"
#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

enum token_type {
    TOK_L_BRACE,
    TOK_R_BRACE,
    TOK_STR,
    TOK_NUM,
    TOK_L_BRACKET,
    TOK_R_BRACKET,
    TOK_NUL,
    TOK_TRUE,
    TOK_FALSE,
};
struct token {
    enum token_type type;
    size_t start;
    size_t end;
};
struct token_vector {
    struct token* tokens;
    size_t len;
};

bool push_token(struct token_vector* vec,struct token* token) {
    struct token* new = reallocarray(vec->tokens, vec->len+1, sizeof(struct token));
    if (!new) {
        return false;
    }
    vec->tokens=new;
    new[vec->len]=*token;
    return true;
}
struct string {
    char* str;
    size_t len;
};
bool string_get(struct string* str,size_t index) {
    if (index>=str->len) {
        return false;
    }
    return str->str[index];
}

void newline(struct jerr *err, size_t *index) {
    err->line++;
    err->last_nl = *index;
}
// bool ws(char *str, size_t str_len, size_t *index, char *c, struct jerr *err) {
//     do {
//         if (*index >= str_len) {
//             return true;
//         }
//         *c = str[(*index)];
//         if (*c == '\n') {
//             newline(err, index);
//         }
//         (*index)++;
//     } while (isspace(*c));
//     (*index)--;
//     // now c==[*index]
//     return false;
// }
enum type find_type(char *str, size_t str_len, size_t *index) {
    if (*index >= str_len) {
        return UNKNOWN;
    }
    char c = str[*index];
#if dbp
    printf("find_type(%c)\n", c);
#endif
    if (isdigit(c) || c == '-') {
        return JNUMBER;
    }
    switch (c) {
        case '[':
            return JARRAY;
        case '{':
            return JOBJECT;
        case 't':
        case 'f':
            return JBOOL;
        case '"':
            return JSTR;
        case 'n':
            return JNULL;
        default:
            return UNKNOWN;
            // fprintf(stderr,"help what tye starts with
            // %c?\n",(char)c);exit(EXIT_FAILURE);
    }
}
void lexer(char* str, size_t str_len, struct jerr* err) {
    size_t index = 0;
    struct token_vector vec;
    if (index >= str_len) {
        err->iserr = true;
        err->type=UNKNOWN;
        err->expected[0]='a';
        err->expected[1]='n';
        err->expected[2]='y';
        return;
    }
    while (index<str_len) {

    char c = str[index];
#if dbp
    printf("find_type(%c)\n", c);
#endif
    if (isdigit(c) || c == '-') {
        //great check if number is valid
        size_t start = index;
        while (isdigit(c)||c=='-'||c=='.') {
            c=str[index++];
        }
        size_t end = index-1;
        struct token t = {.type=TOK_NUM,.start=start,.end=end};
        if (!push_token(&vec, &t)) return;
        continue;
    }
    switch (c) {
        case '[':

            //TODO
            stuct token t = {.type=TOK_L_BRACKET,}
            return JARRAY;
        case '{':
            return JOBJECT;
        case 't':
        case 'f':
        return JBOOL;
        case '"':
        return JSTR;
        case 'n':
        return JNULL;
        default:
            return UNKNOWN;
            // fprintf(stderr,"help what tye starts with
            // %c?\n",(char)c);exit(EXIT_FAILURE);
    }
    }
}
void lexer_null(char* str, size_t str_len, size_t *index, struct jerr *err) {
    
}