// debug print
#ifndef dbp
#define dbp 1
#endif

#include "json.h"
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <hashmap.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROJECT_NAME "json"
// eh it works
#define exit_null(j, v, n)                                                                         \
    if (!v) {                                                                                      \
        free_object(j);                                                                            \
        return NULL;                                                                               \
    }
#define exit_v(j, v, e, n)                                                                         \
    if (v == e) {                                                                                  \
        free_object(j);                                                                            \
        return NULL;                                                                               \
    }
#define exit_malloc(type_t)                                                                        \
    if (!j) {                                                                                      \
        err->iserr = true;                                                                         \
        err->type = type_t;                                                                        \
        err->pos = *index;                                                                         \
        err->errno_set = true;                                                                     \
        return NULL;                                                                               \
    }
#if dbp
void pair_printer(struct hashmap_node *node) {
    struct key_pair *pair = node->val;
    printf("{\"%s\":", pair->key);
    printf("(%s)", jtype_to_str(pair->val->type));
    // print_object(pair->val);
    printf("}");
}
#endif
// manipulation
struct jvalue *jobj_get(struct jvalue *value, const char *key) {
    if (value->type != JOBJECT)
        return NULL;
    struct key_pair *pair = hm_get(value->val.obj, key);
    if (!pair)
        return NULL;
    return pair->val;
}
struct jvalue *jobj_set(struct jvalue *obj, const char *key, struct jvalue *value) {
    if (obj->type != JOBJECT)
        return NULL;
    // hm_set_ptr(obj->val.obj, hm_new(obj->val.obj, key), key, strlen(key));
    struct key_pair *pair = hm_get(obj->val.obj, key);
    printf("pair: %p\n",(void*)pair);
    // hm_set(obj->val.obj,hm_find(obj->val.obj, key),)
    if (!pair) {
        pair = malloc(sizeof(struct key_pair));
        pair->val = value;
        pair->key = key;
        printf("hm_set %s:%p\n",key,(void*)value);
        print_value(value);
        printf("|value\n");
        hm_debugx(obj->val.obj, pair_printer);
        hm_set(obj->val.obj, NULL, pair);
        hm_debugx(obj->val.obj, pair_printer);
        printf("end hm_set\n");
    } else {
        free_object(pair->val);
        pair->val = value;
    }
    return pair->val;
}
bool jobj_del(struct jvalue *value, const char *key) {
    if (!value)
        return false;
    return hm_delete(value->val.obj, key, strlen(key));
}
char *jstr_get(struct jvalue *value) {
    if (value->type != JSTR)
        return NULL;
    return value->val.str;
}
bool jbool_get(struct jvalue *value) { return value->val.boolean; }
jnumber *jnumber_get(struct jvalue *value) { return &value->val.number; }
struct jvalue *jarray_get(struct jvalue *value, size_t index) {
    if (value->type != JARRAY || index >= value->val.array.len) {
        return NULL;
    }
    return value->val.array.arr[index];
}
const size_t *jarray_len(struct jvalue *value) {
    if (value->type != JARRAY) {
        return NULL;
    }
    return &value->val.array.len;
}

void consume(size_t *index, struct jerr *err, size_t count) {
    (*index) += count;
    err->pos += count;
}
void newline(struct jerr *err, size_t *index) {
    err->line++;
    err->last_nl = *index - 1;
}
bool unget(char c, FILE *f) { return ungetc((char)c, f) == EOF; }
// c to change
// index is 1 ahead of c
// index may be at EOF
// req:
// index should point at first char of data(or space)
// what it do:
// if not space then don't move
// set c to first non space
// set index to c
// steps:
//- c=[*index]
//- while (isspace(c)):c=[*index++]
//- if (c!=[*index]): *index--
bool ws(char *str, size_t str_len, size_t *index, char *c, struct jerr *err) {
    if (*index >= str_len)
        return true;
    do {
        if (*index == str_len) {
            break;
        }
        *c = str[(*index)++];
        if (*c == '\n') {
            newline(err, index);
        }
    } while (isspace(*c));
    (*index)--;
#if dbp
    printf("ws end at %zu\n", *index);
#endif
    // now c==[*index]
    return false;
}

struct jvalue *jvalue_clone(struct jvalue *j) {
    struct jvalue *new = malloc(sizeof(struct jvalue));
    if (!new)
        return NULL;
    new->type = j->type;
    switch (new->type) {
        case UNKNOWN:
        case JNUMBER:
        case JBOOL:
        case JNULL: {
            new->val = j->val;
            break;
        }
        case JSTR: {
            new->val.str = strdup(j->val.str);
            break;
        }
        case JARRAY: {
            jarray *arr = &new->val.array;
            jarray *j_arr = &j->val.array;
            arr->len = j_arr->len;
            arr->arr = malloc(sizeof(struct jvalue *) * arr->len);
            for (size_t i = 0; i < arr->len; i++) {
                arr->arr[i] = jvalue_clone(j_arr->arr[i]);
            }
            break;
        }
        case JOBJECT: {
            // copies the struct key_pair's
            // also includes ptr to old jvalue(pair->val)
            //TODO give it a duplicate fn bc it won't duplicate strings right i think
            new->val.obj = hm_clone(j->val.obj);
            jobj new_obj = new->val.obj;
            for (size_t i = 0; i < j->val.obj->len; i++) {
                struct key_pair *pair = j->val.obj->nodes[i].val;
                struct key_pair *new_pair = new_obj->nodes[i].val;
                // the pair is copied over including the old pair->val
                // so replace pair->val with a newly allocated jvalue
                new_pair->val = jvalue_clone(pair->val);
            }
            break;
        }
    }
    return new;
}
size_t jerr_get_col(struct jerr *err) { return err->pos - err->last_nl; }
void jerr_print_str(struct jerr *err, const char *str) {
    if (!err->iserr) {
        return;
    }
    size_t col = jerr_get_col(err);
    size_t start = err->last_nl + 1;
    // size_t col = err->pos - start - 1;
    const char *line_str = &str[start];
    char *end = NULL;
    size_t len = 0;
    if (str) {
        end = strchr(line_str, '\n');
        if (end) {
            len = end - line_str;
            if (col > len) {
                col = len + 1;
            }
            printf("found newline at %zu\n", end - line_str);
        } else {
            len = start - strlen(str);
        }
    }
    fprintf(stderr, "len:%d col:%d start: %zu\n", (int)len, (int)col, start);

#if dbp != 0
    if (str)
        printf("line_str:[%s]\n", line_str);
#endif
    fprintf(stderr, "%.*s\n%*c\n", (int)len, line_str, (int)col, '^');
    fprintf(stderr, "Error parsing %s | Position:%zu(line:%zu col:%zu) | Expected: ",
            jtype_to_str(err->type), err->pos - err->last_nl, err->line, col);
    // print each expectd
    for (int i = 0; i < 3; i++) {
        char c = err->expected[i];
        if (c == '\0') {
            break;

            fprintf(stderr, "'%c' ", c);
        }
        if (err->got) {
            fprintf(stderr, "but got '%c'", err->got);
        }
        fprintf(stderr, "\n");
    }
}

const char *jtype_to_str(enum jtype type) {
    switch (type) {
        case UNKNOWN:
            return "UNKNOWN";
        case JOBJECT:
            return "Object";
        case JSTR:
            return "String";
        case JNUMBER:
            return "Number";
        case JBOOL:
            return "Boolean";
        case JNULL:
            return "Null";
        case JARRAY:
            return "Array";
        default:
            return "type_to_str(Error unknown enum type)";
    }
}

// parse
struct jvalue *jparse_null(char *str, size_t str_len, size_t *index, struct jerr *err) {
    if (*index + 3 >= str_len || str[(*index)] != 'n') {
        err->iserr = true;
        err->type = JNULL;
        err->pos = *index;
        err->expected[0] = 'n';
        return NULL;
    }
    // init object
    struct jvalue *j = JSON_MALLOC(sizeof(struct jvalue));
    exit_malloc(JNULL);
    j->type = JNULL;
    j->val.null = '\0';
    // no need to test if it's null
    // it can only be null
    (*index) += 4;
    return j;
}
struct jvalue *jparse_bool(char *str, size_t str_len, size_t *index, struct jerr *err) {
    if (*index >= str_len) {
        goto bool_err;
    }
    // init object
    struct jvalue *j = JSON_MALLOC(sizeof(struct jvalue));
    exit_malloc(JBOOL);
    j->type = JBOOL;
    // now parse
    char c = str[(*index)++];
    if (c == 't') {
        j->val.boolean = true;
        *index += 3;
        return j;
    } else if (c == 'f') {
        j->val.boolean = false;
        *index += 4;
        return j;
    }
bool_err:
    err->iserr = true;
    err->type = JBOOL;
    err->expected[0] = 't';
    err->expected[1] = 'f';
    err->pos = *index;
    return NULL;
}
struct jvalue *jparse_number(char *str, size_t str_len, size_t *index, struct jerr *err) {
    // init object
    struct jvalue *j = JSON_MALLOC(sizeof(struct jvalue));
    exit_malloc(JNUMBER);
    j->type = JNUMBER;
    j->val.number.islong = true;
    // now parse got num

    if (*index >= str_len) {
        goto number_err;
    }
    char c = str[(*index)++];
    char buf[1024] = {0};
    buf[0] = c;
    int i = 0;
    while (isdigit(c) || c == '.' || c == '-') {
        if (*index >= str_len) {
            goto number_err;
        }
        buf[i] = c;
        c = str[(*index)++];
        if (c == '.') {
            j->val.number.islong = false;
        }
        i++;
    }
    if (i == 0) {
        goto number_err;
    } // expected digit,'.', or '-'
    if (j->val.number.islong) {
        long long l;
        errno = 0;
        l = strtoll(buf, NULL, 10); // assume base 10
        if (errno == EINVAL) {
            err->errno_set = true;
            goto number_err;
        }
        j->val.number.num.l = l;
    } else {
        double d;
        char *ptr = buf;
        d = strtod(buf, &ptr);
        if (ptr == buf) {
            goto number_err;
        }
        // note: errno may be set to ERANGE but it's finnne
        j->val.number.num.d = d;
    }
    (*index)--;

    return j;

number_err:
    err->iserr = true;
    err->type = JNUMBER;
    err->pos = *index;
    free_object(j);
    return NULL;
}
struct jvalue *jparse_string(char *str, size_t str_len, size_t *index, struct jerr *err) {
    // init object
    struct jvalue *j = JSON_MALLOC(sizeof(struct jvalue));
    exit_malloc(JSTR);
    j->type = JSTR;
    j->val.str = NULL;
    // now parse got "
    if (*index >= str_len || str[*index] != '\"') {
        goto expected_quote;
    }
    char c = str[(*index)++];
#if dbp
    printf("begin of parse_string: %c\n", c);
#endif

    bool escaped = false;
    size_t len = 0;
    // get length for malloc
#if dbp
    printf("getlen:\"");
#endif
    // get initial letter/whatever
    size_t begining = *index;
    if (*index >= str_len) {
        goto expected_quote;
    }
    c = str[(*index)++];
    while (c != '\"' || escaped) {
#if dbp
        printf("%c", c);
#endif
        // at least this long
        len++;
        if (!escaped && c == '\\') {
            escaped = true;
            if (*index >= str_len) {
                goto string_err;
            }
            c = str[(*index)++];
            continue;
        }
        escaped = false;
        if (*index >= str_len) {
            goto expected_quote;
        }
        c = str[(*index)++];
    }
#if dbp
    printf("\": %zu\n", len);
#endif
    // now we got the literal characters in
    //  actually parse stirng
    *index = begining;
    char *buf = JSON_MALLOC(len + 1); //+1 for '\0'
    j->val.str = buf;
    if (!buf) {
        err->errno_set = true;
        goto string_err;
    }

    // index into buf
    size_t i = 0;
#if dbp
    // TODO fix escape characters in string parsing
    printf("write:\"");
#endif
    c = str[(*index)++];
#if dbp
    if (c != '"') {
        printf("%c", c);
    }
#endif
    escaped = false;
    while (c != '"' || escaped) {
        if (!escaped && c == '\\') {
            escaped = true;
            // NOTE: this should be fine
            c = str[(*index)++];
            continue;
        }

        if (escaped) {
            switch (c) {
                case 'b':
                    buf[i] = '\b';
                    break;
                case 'f':
                    buf[i] = '\f';
                    break;
                case 'n':
                    buf[i] = '\n';
                    break;
                case 'r':
                    buf[i] = '\r';
                    break;
                case 't':
                    buf[i] = '\f';
                    break;
                case '\\':
                    buf[i] = '\\';
                    break;
                case '"':
                    buf[i] = '"';
                    break;
                default:
                    buf[i] = c;
                    break;
            }
            escaped = false;
        } else {
            buf[i] = c;
        }
        // end
        assert(i < len);
        i++;
        if (*index >= str_len) {
            goto expected_quote;
        }
        c = str[(*index)++];
#if dbp
        printf("%c", c);
#endif
    }
#if dbp
    printf("\n");
#endif
    len = i;
    buf[len] = '\0';

#if dbp
    printf("after parse_string: %c. str:%s\n", c, buf);
#endif
    return j;
expected_quote:
    err->expected[0] = '\"';
string_err:
    if (*index < str_len) {
        err->got = str[*index];
    }
    // only free inner string if we have it
    if (j->val.str) {
        free_object(j);
    } else {
        free(j);
    }
    err->iserr = true;
    err->type = JSTR;
    err->pos = *index;
    return NULL;
}

struct jvalue *jparse_array(char *str, size_t str_len, size_t *index, struct jerr *err) {
    // init object
    struct jvalue *j = JSON_MALLOC(sizeof(struct jvalue));
    exit_malloc(JARRAY);
    j->type = JARRAY;
    j->val.array.arr = NULL;
    j->val.array.len = 0;
    size_t *len = &(j->val.array.len);
    // struct jobject* ptr = j->val.dict;//shortcut
    // we at [
    if (*index >= str_len || str[*index] != '[') {
        err->expected[0] = '[';
        goto array_err;
    }
    // go to after [
    (*index)++;
    // and skip whitespace
    char c = '\0';
    if (ws(str, str_len, index, &c, err)) {
        goto array_err;
    }
#if dbp
    printf("after ws after [:%c\n", c);
#endif

    // we at char after [
    while (c != ']') {
#if dbp
        printf("c:%c\n", c);
#endif

        // parse value
        struct jvalue *element = jparse_any(str, str_len, index, err);
        if (!element) {
            free_object(j);
            return NULL;
        }
        (*len)++;
        struct jvalue **ptr;
        // check if we have an array
        if (!j->val.array.arr) {
            ptr = JSON_MALLOC(sizeof(struct jvalue *));
            if (!ptr) {
                err->errno_set = true;
                goto array_err;
            }
            j->val.array.arr = ptr;
        } else {
            ptr = JSON_REALLOC(j->val.array.arr, *len * sizeof(struct jvalue *));
            if (!ptr) {
                err->errno_set = true;
                goto array_err;
            }
            exit_null(j, ptr, "realloc");
            j->val.array.arr = ptr;
        }
#if dbp
        printf("*len(of array)=%zu\n", *len);
#endif
        // len-1 for last element
        ptr[*len - 1] = element;
#if dbp
        printf("element: ");
        print_value(ptr[*len - 1]);
        printf("\n");
#endif
        // may be whitespace after each value
        //  go up to the comma or closing bracket
        if (ws(str, str_len, index, &c, err) || (c != ',' && c != ']')) {
            err->expected[0] = ',';
            err->expected[1] = ']';
            goto array_err;
        }
#if dbp
        printf("after whitespace: %c\n", c);
#endif
        // we at comma or ]
        // skip comma and go to first char of value but discard it for parse_any
        if (c == ',') {
            // skip comma
            (*index)++;
            if (ws(str, str_len, index, &c, err)) {
                err->expected[0] = ']';
                goto array_err;
            }
            // don't increment index
            // so parse_any can read it
        }
    }
    (*index)++;
    return j;
array_err:
    free_object(j);
    err->iserr = true;
    err->type = JARRAY;
    err->pos = *index;
    if (*index < str_len) {
        err->got = str[*index];
    }
    return NULL;
}
struct jvalue *jparse_object(char *str, size_t str_len, size_t *index, struct jerr *err) {
    // init object
    struct jvalue *j = JSON_MALLOC(sizeof(struct jvalue));
    exit_malloc(JOBJECT);
    j->type = JOBJECT;
    j->val.obj = hm_create();
    hashmap_t *hm = j->val.obj; // shortcut
    if (!hm) {
        err->errno_set = true;
        goto object_err;
    }
    // we at {
    if (*index >= str_len || str[*index] != '{') {
        err->expected[0] = '{';
        goto object_err;
    }
    char c = str[(*index)]; //;'{'
//{ ""
#if dbp
    printf("parse object %c\n", c);
#endif
    while (c != '}') {
        // go past whitespace after { or ,
        (*index)++;
        if (ws(str, str_len, index, &c, err)) {
            err->expected[0] = '}';
            goto object_err;
        }
        // this accounts for empty objects
        if (c == '}') {
            break;
        }
#if dbp
        printf("begin of key: %c\n", c);
        fflush(stdout);
#endif
        // parse key
        struct jvalue *s = jparse_string(str, str_len, index, err);
        if (!s) {
            free_object(j);
            return NULL;
        } // err already set
        if (s->type != JSTR) {
            free_object(s);
            err->expected[0] = '"';
            goto object_err;
        }
        char *key = s->val.str; // already malloc'd
        free(s);                // free jobject but not the inner string
        // go to :
        if (*index >= str_len) {
            err->expected[0] = ' ';
            err->expected[1] = ':';
            free(key);
            goto object_err;
        }
        c = str[*index]; // after key
        if (isspace(c)) {
            if (ws(str, str_len, index, &c, err)) {
                free(key);
                err->expected[0] = ':';
                goto object_err;
            }
        }
        // should be at :
        if (*index >= str_len || str[*index] != ':') {
            err->expected[0] = ':';
            free(key);
            goto object_err;
        }
#if dbp
        printf("after key: %c\n", c);
        fflush(stdout);
#endif
        (*index)++;
        // get whitespace and first char of value after :
        if (ws(str, str_len, index, &c, err)) {
            err->expected[0] = 'v';
            free(key);
            goto object_err;
        }

        // parse value
        struct jvalue *child = jparse_any(str, str_len, index, err);
        if (!child) {
            free(key);
            free_object(j);
            return NULL;
        }
        struct key_pair pair = {key, child};
#if dbp
        printf("hm_setx(\"%s\",", key);
        print_value(child);
        printf(")\n");
#endif
        hm_setx(hm, hm_hash(hm, key), &pair, sizeof(pair));
#if dbp
        hm_debugx(hm, pair_printer);
        printf("parsed value\n");
#endif

        // go up to comma or }
        if (ws(str, str_len, index, &c, err)) {
#if dbp
            printf("ws EOF\n");
#endif
            err->expected[0] = '}';
            err->expected[1] = ',';
            goto object_err;
        }
        if (c != '}' && c != ',') {
            err->expected[0] = '}';
            err->expected[1] = ',';
            goto object_err;
        }
#if dbp
        printf("obj: %c/%d\n", c, c);
        fflush(stdout);
#endif
    }
    (*index)++;
    return j;
object_err:
    free_object(j);
    err->iserr = true;
    err->type = JOBJECT;
    err->pos = *index;
    if (*index < str_len) {
        err->got = str[*index];
    }
    return NULL;
}

enum jtype jfind_type(char *str, size_t str_len, size_t *index) {
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
// parse any type
struct jvalue *jparse_any(char *str, size_t str_len, size_t *index, struct jerr *err) {
    switch (jfind_type(str, str_len, index)) {
        case JOBJECT:
            return jparse_object(str, str_len, index, err);
        case JSTR:
            return jparse_string(str, str_len, index, err);
        case JNUMBER:
            return jparse_number(str, str_len, index, err);
        case JARRAY:
            return jparse_array(str, str_len, index, err);
        case JBOOL:
            return jparse_bool(str, str_len, index, err);
        case JNULL:
            return jparse_null(str, str_len, index, err);
        case UNKNOWN:
        default:
            err->iserr = true;
            err->type = UNKNOWN;
            err->pos = *index;
            return NULL;
            // fprintf(stderr, "Unknown type. pos:%ld", ftell(f));
    }
}
// sets errno
struct jvalue *load_file(FILE *f, char **str_buf, size_t *str_len, struct jerr *err) {
    // load whatever kinda object this is
    *str_len = 0;

    fseek(f, 0, SEEK_END);
    size_t len = (size_t)ftell(f);
#if dbp
    printf("len:%zu\n", len);
#endif
    fseek(f, 0, SEEK_SET);
    *str_buf = JSON_MALLOC(len + 1);
    if (!(*str_buf)) {
        err->iserr = true;
        err->errno_set = true;
        return NULL;
    }
    *str_len = len;
    fread(*str_buf, 1, len, f);
    (*str_buf)[len] = '\0';
    size_t index = 0;

    return jparse_any(*str_buf, len, &index, err);
}
// set errno
struct jvalue *load_filename(const char *fn, char **str_buf, size_t *str_len, struct jerr *err) {
    FILE *f = fopen(fn, "r");
    if (!f) {
        err->iserr = true;
        err->errno_set = true;
        return NULL;
    }

    struct jvalue *result = load_file(f, str_buf, str_len, err);

    if (fclose(f) == EOF) {
        free_object(result);
        return NULL;
    }
    return result;
}

// frees j and its value(s) recursively
void free_object(struct jvalue *j) {
    switch (j->type) {
        case JOBJECT:;
            // free every value
            struct hashmap_node *node = j->val.obj->nodes;
            while (node) {
                // each node is a key-value pair. the key ptr and value ptr need to be
                // freed
                struct key_pair *pair = (node->val);
                // the STRING key
                JSON_FREE(pair->key);
                free_object(pair->val);
                node = node->next;
            }
            // after that, the key-value pair structs themselves are freed
            hm_free(j->val.obj);
            break;
        case JARRAY:
            // free each object
            for (size_t i = 0; i < (j->val.array.len); i++) {
                free_object(j->val.array.arr[i]);
            }
            // free vec
            JSON_FREE(j->val.array.arr);
            break;
        case JSTR:
            JSON_FREE(j->val.str);
            break;
        case UNKNOWN: // we'll accept this
        case JNUMBER:
        case JBOOL:
        case JNULL:
            // not malloc'd
            break;
    }
    JSON_FREE(j);
}
void print_value(struct jvalue *j) { fprint_value(stdout, j); }
// true if success
bool serialize(const char *fn, struct jvalue *j) {
    FILE *f = fopen(fn, "w");
    if (!f) {
        return false;
    }
    fprint_value(f, j);
    fclose(f);
    return true;
}
#define exit_neg(v, msg)                                                                           \
    if (v < 0) {                                                                                   \
        return false;                                                                              \
    }

void fprint_string(FILE *f, const char *str) {
    // https://stackoverflow.com/a/3201478

    size_t len = strlen(str);
    for (size_t i = 0; i < len + 1; i++) {
        char ch = str[i];
        switch (ch) {
            case '\"':
                fprintf(f, "\\\"");
                break;
            case '\\':
                fprintf(f, "\\\\");
                break;
            case '\n':
                fprintf(f, "\\n");
                break;
            case '\t':
                fprintf(f, "\\t");
                break;
            default:
                fputc(ch, f);
        }
    }
}
bool fprint_value(FILE *f, struct jvalue *j) {
    switch (j->type) {
        case JSTR:
            fprintf(f, "\"");
            fprint_string(f, j->val.str);
            fprintf(f, "\"");
            break;
        case UNKNOWN:
            exit_neg(fprintf(f, "UNKNOWN"), "fprint_object");
            break;
        case JNUMBER:
            if (j->val.number.islong) {
                exit_neg(fprintf(f, "%lld", j->val.number.num.l), "fprint_object");
            } else {
                exit_neg(fprintf(f, "%lf", j->val.number.num.d), "fprint_object");
            }
            break;
        case JBOOL:
            if (j->val.boolean) {
                exit_neg(fprintf(f, "true"), "fprint_object");
            } else {
                exit_neg(fprintf(f, "false"), "fprint_object");
            }
            break;
        case JNULL:
            exit_neg(fprintf(f, "null"), "fprint_object");
            break;
        case JOBJECT:
            exit_neg(fprintf(f, "{"), "fprint_object");
            struct hashmap_node *node = j->val.obj->nodes;
            while (node) {
                struct key_pair *pair = node->val;
                fprintf(f, "\"");
                fprint_string(f, pair->key);
                fprintf(f, "\":");
                fprint_value(f, pair->val);
                node = node->next;
                if (node) {
                    exit_neg(fprintf(f, ", "), "fprint_object");
                }
            }
            exit_neg(fprintf(f, "}"), "fprint_object");
            break;
        case JARRAY:
            exit_neg(fprintf(f, "["), "fprint_object");
            struct array array = j->val.array;
            for (size_t i = 0; i < array.len; i++) {
                fprint_value(f, array.arr[i]);
                if (i != array.len - 1) {
                    exit_neg(fprintf(f, ", "), "fprint_object");
                }
            }
            fprintf(f, "]");
            break;
    }
    return true;
}
// this function extends a buffer if needed.
// offset is where the data should to go
// returns new extended buffer or old buffer
// starts at i
char *wbuf(char *buf, size_t offset, size_t *buf_len, size_t data_len) {
    if (!buf) {
        return NULL;
    }
    size_t new_len = offset + data_len + 1;
    if (new_len >= (*buf_len)) {
        char *new = JSON_REALLOC(buf, new_len);
        *buf_len = new_len;
        if (!new) {
            free(buf);
            return NULL;
        }
        return new;
    }
    return buf;
}
char *sprint_string(const char *str, char *buf, size_t *offset, size_t *buf_len) {
    // https://stackoverflow.com/a/3201478

    size_t len = strlen(str);
    buf = wbuf(buf, *offset, buf_len, len);
    if (!buf) {
        return NULL;
    }
    for (size_t i = 0; i < len; i++) {
        char ch = str[i];
        switch (ch) {
            case '\"':
#if dbp != 0
                printf("buf_len pre:%zu and at %zu buf:%p\n", *buf_len, *offset, (void *)buf);
#endif
                buf = wbuf(buf, *offset, buf_len, 2);
                if (!buf) {
                    return NULL;
                }
#if dbp != 0
                printf("buf_len after:%zu at %zu\n", *buf_len, *offset);
#endif
                buf[(*offset)++] = '\\';
                buf[(*offset)++] = '"';
                break;
            case '\\':
                buf = wbuf(buf, *offset, buf_len, 2);
                if (!buf) {
                    return NULL;
                }
                buf[(*offset)++] = '\\';
                buf[(*offset)++] = '\\';
                break;
            case '\n':
                buf = wbuf(buf, *offset, buf_len, 2);
                if (!buf) {
                    return NULL;
                }
                buf[(*offset)++] = '\\';
                buf[(*offset)++] = 'n';
                break;
            case '\t':
                buf = wbuf(buf, *offset, buf_len, 2);
                if (!buf) {
                    return NULL;
                }
                buf[(*offset)++] = '\\';
                buf[(*offset)++] = 't';
                break;
            default:
                buf = wbuf(buf, *offset, buf_len, 1);
                if (!buf) {
                    return NULL;
                }
                buf[(*offset)++] = ch;
        }
    }
    buf = wbuf(buf, *offset, buf_len, 1);
    if (!buf) {
        return NULL;
    }
    buf[*offset] = '\0';
    return buf;
}
char *sprint_value_normal(struct jvalue *j) { return sprint_value(j, NULL, NULL, NULL); }
// on success wil return an allocated buffer(char* buf may be invalid(ralloc'd) at this point)
// that isn't- guaranteed to have a null terminator offset will contain the new length on error
// returns null, but offset will be amount of characters successful (and the buffer is freed)
char *sprint_value(struct jvalue *j, char *buf, size_t *offset, size_t *buf_len) {
    size_t offset_temp = 0;
    size_t buf_len_temp = 0;
    if (!offset) {
        offset = &offset_temp;
    }
    if (!buf_len) {
        buf_len = &buf_len_temp;
    }
    assert(offset);
    if (!buf) {
        buf = JSON_MALLOC(8);
        if (!buf) {
            return NULL;
        }
        *buf_len = 8;
    }

    char *s;
    size_t len;
    char num_buf[1024] = {0};

#if dbp != 0
    printf("j:%p\n",(void*)j);
    printf("sprint a %s aka %d %d\n", jtype_to_str(j->type), j->type, __LINE__);
#endif
    switch (j->type) {
        case JSTR:
            s = j->val.str;
#if dbp != 0
            printf("buf_len:%zu %d\n", *buf_len, __LINE__);
#endif
            // 2 extra char's
            buf = wbuf(buf, *offset, buf_len, 3);
            if (!buf) {
                return NULL;
            }
            // this is the first extra char
            buf[(*offset)++] = '"';
#if dbp != 0
            printf("buf_len:%zu %d\n", *buf_len, __LINE__);
#endif
            buf = sprint_string(s, buf, offset, buf_len);
            if (!buf)
                return NULL;
            buf = wbuf(buf, *offset, buf_len, 2);
            if (!buf) {
                return NULL;
            }
#if dbp != 0
            printf("buf_len:%zu %d\n", *buf_len, __LINE__);
#endif
            // replace '\0' with "
            buf[(*offset)++] = '"';
            // this is the second extra char
            buf[(*offset)] = '\0';
            return buf;
        case UNKNOWN:
            s = "UNKNOWN";
            len = strlen(s);
            buf = wbuf(buf, *offset, buf_len, len + 1);
            if (!buf) {
                return NULL;
            }
            strcat(buf, s);
            *offset += len;
            return buf;
        case JNUMBER:
            if (j->val.number.islong) {
                len = snprintf(num_buf, 1024, "%lld", j->val.number.num.l);
            } else {
                len = snprintf(num_buf, 1024, "%lf", j->val.number.num.d);
            }
            buf = wbuf(buf, *offset, buf_len, len + 1);
            if (!buf) {
                return NULL;
            }
            stpcpy(buf + *offset, num_buf);
            // memcpy(buf+*offset,num_buf,len+1);
            *offset += len;
            return buf;
        case JBOOL:
            if (j->val.boolean) {
                s = "true";
                len = strlen(s);
                buf = wbuf(buf, *offset, buf_len, len + 1);
                if (!buf) {
                    return NULL;
                }
                stpcpy(buf + *offset, s);
                *offset += len;
                return buf;
            } else {
                s = "false";
                len = strlen(s);
                buf = wbuf(buf, *offset, buf_len, len + 1);
                if (!buf) {
                    return NULL;
                }
                stpcpy(buf + *offset, s);
                *offset += len;
                return buf;
            }
            break;
        case JNULL:
            s = "null";
            len = strlen(s);
            buf = wbuf(buf, *offset, buf_len, len + 1);
            if (!buf) {
                return NULL;
            }
            stpcpy(buf + *offset, s);
            *offset += len;
            return buf;
        case JOBJECT:
            buf = wbuf(buf, *offset, buf_len, 2);
            if (!buf) {
                return NULL;
            }
            buf[(*offset)++] = '{';
            buf[(*offset)] = '\0';

            struct hashmap_node *node = j->val.obj->nodes;
            while (node) {
                struct key_pair *pair = node->val;
                const char *s = pair->key;
                buf = wbuf(buf, *offset, buf_len, 5); //+2 for ", " | +2 for ':', ' '
                if (!buf) {
                    return NULL;
                }
#if dbp != 0
                printf("buf_len:%zu %d\n", *buf_len, __LINE__);
#endif
                buf[(*offset)++] = '"';
                buf = sprint_string(s, buf, offset, buf_len);
                if (!buf)
                    return NULL;
                buf = wbuf(buf, *offset, buf_len, 4);
                if (!buf)
                    return NULL;
                buf[(*offset)++] = '"';

                buf[(*offset)++] = ':';
                buf[(*offset)++] = ' ';
                buf[(*offset)] = '\0';
#if dbp != 0
                printf("buf_len:%zu %d\n", *buf_len, __LINE__);
                printf("pair->val:%p\n",(void*)pair->val);
#endif

                // print object
                buf = sprint_value(pair->val, buf, offset, buf_len);
                if (!buf) {
                    return NULL;
                }
#if dbp != 0
                printf("buf_len:%zu %d\n", *buf_len, __LINE__);
#endif

                node = node->next;
                if (node) {
                    buf = wbuf(buf, *offset, buf_len, 3);
                    if (!buf) {
                        return NULL;
                    }
                    buf[(*offset)++] = ',';
                    buf[(*offset)++] = ' ';
                    buf[(*offset)] = '\0';
                }
            }
            buf = wbuf(buf, *offset, buf_len, 2);
            if (!buf) {
                return NULL;
            }
#if dbp != 0
            printf("buf_len:%zu\n", *buf_len);
#endif
            buf[(*offset)++] = '}';
            buf[(*offset)] = '\0';
            return buf;
        case JARRAY:
            buf = wbuf(buf, *offset, buf_len, 2);
            if (!buf) {
                return NULL;
            }
            buf[(*offset)++] = '[';
            buf[(*offset)] = '\0';
            struct array array = j->val.array;
            for (size_t i = 0; i < array.len; i++) {
                buf = sprint_value(array.arr[i], buf, offset, buf_len);
                if (!buf) {
                    return NULL;
                }
                if (i != array.len - 1) {
                    buf = wbuf(buf, *offset, buf_len, 3);
                    if (!buf) {
                        return NULL;
                    }
                    buf[(*offset)++] = ',';
                    buf[(*offset)++] = ' ';
                    buf[(*offset)] = '\0';
                }
            }
            buf = wbuf(buf, *offset, buf_len, 1);
            if (!buf) {
                return NULL;
            }
            buf[(*offset)++] = ']';
            buf[(*offset)] = '\0';
            return buf;
    }
    free(buf);
    return NULL; // shouldn't reach here
}
