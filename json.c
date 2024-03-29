//debug print
#ifndef dbp
#define dbp 0
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
#define exit_null(v, n)                                                        \
    if (!v) {                                                                    \
        perror(n);                                                                 \
        exit(EXIT_FAILURE);                                                        \
    }
#define exit_v(v, e, n)                                                        \
    if (v == e) {                                                                \
        perror(n);                                                                 \
        exit(EXIT_FAILURE);                                                        \
    }
void unget(char c, FILE *f) { exit_v(ungetc((char)c, f), EOF, "ungetc"); }
#define ws(msg)                                                                \
    do {                                                                         \
        c = fgetc(f);                                                              \
        exit_v(c, EOF, msg);                                                       \
    } while (isspace(c));

void copy_to(struct jobject *dest, struct jobject *src) {
    dest->type = src->type;
    dest->val = src->val;
}


const char* type_to_str(enum type type) {
    switch (type) {
        case UNKNOWN:return "UNKNOWN"; 
        case JOBJECT:return "Object";
        case JSTR:return "String";
        case JNUMBER:return "Number";
        case JBOOL:return "Boolean";
        case JNULL:return "Null";
        case JARRAY:return "Array";
        default:return "type_to_str(Error unknown enum type)";
    }
}

// parse
struct jobject parse_null(FILE *f) {
    // init object
    struct jobject j;
    j.type = JNULL;
    j.val.null = '\0';
    // no need to test if it's null
    // it can only be null
    fseek(f, 4, SEEK_CUR);
    return j;
}
struct jobject parse_bool(FILE *f) {
    // init object
    struct jobject j;
    j.type = JBOOL;
    // now parse
    int c = fgetc(f);
    exit_v(c, EOF, "fgetc");
    if (c == 't') {
        j.val.boolean = true;
        fseek(f, 3, SEEK_CUR);
        return j;
    } else if (c == 'f') {
        j.val.boolean = false;
        fseek(f, 4, SEEK_CUR);
        return j;
    } else {
        char buf[5] = {0};
        exit_null(fgets(buf, 5, f),"fgets");
        fprintf(stderr, "[bool] tf is %c%4s...?\n", c, buf);
        exit(EXIT_FAILURE);
    }
}
struct jobject parse_number(FILE *f) {
    // init object
    struct jobject j;
    j.type = JNUMBER;
    j.val.number.islong = true;
    // now parse got num

    int c = fgetc(f);
    exit_v(c, EOF, "fgetc");
    char buf[1024] = {0};
    buf[0] = c;
    int i = 0;
    while (isdigit(c) || c == '.' || c == '-') {
        if (i > 1024) {
            fprintf(stderr, "WARNING: json number too big at byte %zu", ftell(f));
            break;
        }
        buf[i] = c;
        c = fgetc(f);
        exit_v(c, EOF, "fgetc");
        if (c == '.') {
            j.val.number.islong = false;
        }
        i++;
    }
    if (j.val.number.islong) {
        long long l;
        errno = 0;
        l = strtoll(buf, NULL, 10); // assume base 10
        if (errno != 0) {
            perror("strtoll");
            exit(EXIT_FAILURE);
        }
        j.val.number.num.l = l;
    } else {
        double d;
        errno = 0;
        d = strtod(buf, NULL);
        if (errno != 0) {
            perror("strtod");
            exit(EXIT_FAILURE);
        }
        j.val.number.num.d = d;
    }

    unget(c, f);

    return j;
}
struct jobject parse_string(FILE *f) {
    // init object
    struct jobject j;
    j.type = JSTR;
    // now parse got "
    int c = fgetc(f);
    exit_v(c, EOF, "fgetc");
#if dbp
    printf("begin of parse_string: %c\n", c);
    fflush(stdout);
#endif
    assert(c == '\"');
    // cool now we've verified begining

    bool escaped = false;
    size_t len = 0;
    // get length
#if dbp
    printf("getlen:\"");
#endif
    // get initial letter/whatever
    c = fgetc(f);
    exit_v(c, EOF, "parse_string");
    while (c != '\"') {
#if dbp
        printf("%c", c);
#endif
        if (!escaped && c == '\\') {
            escaped = true;
            continue;
        }
        escaped = false;
        len++;
        c = fgetc(f);
        exit_v(c, EOF, "parse_string");
    }
#if dbp
    printf("\": %zu\n", len);
#endif

    // actually parse stirng
    fseek(f, -len - 1, SEEK_CUR);
    char *buf = malloc(len + 1); //+1 for '\0'
    exit_null(buf, "malloc");
    j.val.str = buf;

    size_t i = 0;
#if dbp
    printf("write:\"");
#endif
    c = fgetc(f);
    exit_v(c, EOF, "parse_string");
#if dbp
    printf("{%c}", c);
#endif
    while (c != '"') {
        if (!escaped && c == '\\') {
            escaped = true;
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
        c = fgetc(f);
        exit_v(c, EOF, "parse_string");
#if dbp
        printf("%c", c);
#endif
    }
#if dbp
    printf("\n");
#endif
    assert(c == '"');
    buf[len] = '\0';

#if dbp
    printf("after parse_string: %c. str:%s\n", c, buf);
#endif
    return j;
}
struct jobject parse_array(FILE *f) {
    // init object
    struct jobject j;
    j.type = JARRAY;
    j.val.array.arr = NULL;
    j.val.array.len = 0;
    size_t *len = &(j.val.array.len);
    // struct jobject* ptr = j.val.dict;//shortcut
    // we at [
    int c = fgetc(f);
    exit_v(c, EOF, "fgetc");
    assert(c == '[');
    // printf("first char of list %c\n",c);
    ws("after [");
    // c=fgetc(f);exit_v(c,EOF,"fgetc");
    while (c != ']') {
        // go past whitespace after { or ,
        unget(c, f);
        if (isspace(c)) {
            ws("parse_array");
            unget(c, f);
        }
#if dbp
        printf("c:%c\n", c);
#endif

        // parse value
        struct jobject element = parse_any(f);
        (*len)++;
        struct jobject *ptr;
        if (!j.val.array.arr) {
            ptr = malloc(sizeof(struct jobject));
            exit_null(ptr, "malloc");
            j.val.array.arr = ptr;
        } else {
            ptr = realloc(j.val.array.arr, *len * sizeof(struct jobject));
            exit_null(ptr, "realloc");
            j.val.array.arr = ptr;
        }
#if dbp
        printf("*len(of array)=%zu\n",*len);
#endif
        ptr[*len - 1].type = element.type;
        ptr[*len - 1].val = element.val;
#if dbp
        printf("element: ");
        print_object(&ptr[*len - 1]);
        printf("\n");
#endif
        // go up to the coomma or closing bracket
        ws("parse_array");
#if dbp
        printf("after whitespace: %c\n", c);
#endif
        if (c == ']') {
            break;
        }
        // assert((c==',')||(c==']'));
        // we at comma or }
        c = fgetc(f);
        exit_v(c, EOF, "fgetc");
    }
    return j;
}
struct jobject parse_object(FILE *f) {
    // init object
    struct jobject j;
    j.type = JOBJECT;
    j.val.dict = hm_create();
    hashmap_t *hm = j.val.dict; // shortcut
                                // we at {
    int c = fgetc(f);
    exit_v(c, EOF, "fgetc");
    assert(c == '{');
    c = fgetc(f);
    exit_v(c, EOF, "fgetc");
#if dbp
    printf("parse object\n");
#endif
    while (c != '}') {
        // go past whitespace after { or ,
        unget(c, f);
        ws("parse_object");
        unget(c, f);
#if dbp
        printf("begin of key: %c\n", c);
        fflush(stdout);
#endif
        // parse key
        struct jobject s = parse_string(f);
        char *str = s.val.str;//already malloc'd
                              // get : and whitespace
        ws("parse_object");
#if dbp
        printf("after key: %c\n", c);
        fflush(stdout);
#endif
        assert(c == ':');
        // get whitespace and first char of value after :
        ws("parse_object");
        // put back the value for parsing
        unget(c, f);

        // parse value
        struct jobject child = parse_any(f);
        struct key_pair pair = {str, child};
#if dbp
        printf("hm_setx(\"%s\",", str);
        print_object(&child);
        printf(")\n");
#endif
        hm_setx(hm, hm_new(hm, str), &pair, sizeof(pair));
#if dbp
        hm_debug(hm);
        printf("parsed value\n");
#endif

        // check for comma and skip it
        ws("parse_object");
#if dbp
        printf("obj: %c/%d\n", c, c);
#endif
        fflush(stdout);
        assert(c == ',' || c == '}');
        if (c == '}') {
            break;
        }
        // we at comma or }
        c = fgetc(f);
        exit_v(c, EOF, "fgetc");
    }
    return j;
}
enum type find_type(FILE *f) {
    int c = fgetc(f);
    exit_v(c, EOF, "fgetc");
    exit_v(ungetc((char)c, f), EOF, "ungetc");
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
struct jobject parse_any(FILE *f) {
    struct jobject j;
    j.type = find_type(f);
    switch (j.type) {
        case JOBJECT:
            return parse_object(f);
        case JSTR:
            return parse_string(f);
        case JNUMBER:
            return parse_number(f);
        case JARRAY:
            return parse_array(f);
        case JBOOL:
            return parse_bool(f);
        case JNULL:
            return parse_null(f);
        case UNKNOWN:
            fprintf(stderr, "Unknown type. pos:%ld", ftell(f));
            exit(EXIT_FAILURE);
    }
    j.type = UNKNOWN;
    return j;
}
struct jobject load_file(FILE *f) {
    // load whatever kinda object this is
    return parse_any(f);
}
struct jobject load_fn(char *fn) {
    FILE *f = fopen(fn, "r");
    exit_null(f, "fopen");

    struct jobject result = load_file(f);

    exit_v(fclose(f), EOF, "fclose");
    return result;
}

// frees j's value(s) recursively
void free_object(struct jobject *j) {
    switch (j->type) {
        case JOBJECT:;
                     // free every value
                     struct hashmap_node *node = j->val.dict->nodes;
                     while (node) {
                         // each node is a key-value pair. the key ptr and value ptr need to be
                         // freed
                         struct key_pair *pair = (node->val);
                         free(pair->key);
                         free_object(&pair->val);
                         node = node->next;
                     }
                     // after that, the key-value pair structs themselves are freed
                     hm_free(j->val.dict);
                     break;
        case JARRAY:
                     for (size_t i=0; i<j->val.array.len; i++) {
                         free_object(&j->val.array.arr[i]);
                     }
                     free(j->val.array.arr);
                     break;
        case JSTR:
                     free(j->val.str);
                     break;
        case UNKNOWN: // we'll accept this
        case JNUMBER:
        case JBOOL:
        case JNULL:
                     // not malloc'd
                     break;
    }
}
void print_object(struct jobject* j) {
    fprint_object(stdout,j);
}
void serialize(char* fn, struct jobject *j) {
    FILE* f=fopen(fn,"w");
    exit_null(f,"serialize");
    fprint_object(f,j);
    fclose(f);
}
#define exit_neg(v,msg) if (v<0) {perror(msg);exit(EXIT_FAILURE);}
void fprint_object(FILE* f,struct jobject *j) {
    switch (j->type) {
        case JSTR:
            exit_neg(fprintf(f,"\"%s\"", j->val.str),"fprint_object");
            break;
        case UNKNOWN:
            exit_neg(fprintf(f,"UNKNOWN"),"fprint_object");
            break;
        case JNUMBER:
            if (j->val.number.islong) {
                exit_neg(fprintf(f,"%lld", j->val.number.num.l),"fprint_object");
            } else {
                exit_neg(fprintf(f,"%lf", j->val.number.num.d),"fprint_object");
            }
            break;
        case JBOOL:
            if (j->val.boolean) {
                exit_neg(fprintf(f,"true"),"fprint_object");
            } else {
                exit_neg(fprintf(f,"false"),"fprint_object");
            }
            break;
        case JNULL:
            exit_neg(fprintf(f,"null"),"fprint_object");
            break;
        case JOBJECT:
            exit_neg(fprintf(f,"{"),"fprint_object");
            struct hashmap_node *node = j->val.dict->nodes;
            while (node) {
                struct key_pair *pair = node->val;
                exit_neg(fprintf(f,"\"%s\":", pair->key),"fprint_object");
                fprint_object(f,&pair->val);
                node = node->next;
                if (node) {
                    exit_neg(fprintf(f,", "),"fprint_object");
                }
            }
            exit_neg(fprintf(f,"}"),"fprint_object");
            break;
        case JARRAY:
            exit_neg(fprintf(f,"["),"fprint_object");
            struct array array = j->val.array;
            for (size_t i = 0; i < array.len; i++) {
                fprint_object(f,&array.arr[i]);
                if (i != array.len - 1) {
                    exit_neg(fprintf(f,", "),"fprint_object");
                }
            }
            fprintf(f,"]");
            break;
    }
}
//this function extends a buffer if needed.
//offset is where the data should to go
//returns new extended buffer or old buffer
//starts at i
char* wbuf(char* buf,size_t offset,size_t* buf_len,size_t data_len) {
    if (!buf){return NULL;}
    if (offset+data_len>(*buf_len)-1) {
        char* new = realloc(buf,offset+data_len);
        *buf_len+=data_len;
        if (!new) {free(buf);return NULL;}
        return new;
    }
    return buf;
}
//offset can't be null and should be set to 0
//on success wil return an allocated buffer(char* buf may be invalid(ralloc'd) at this point) that isn't- 
//guaranteed to have a null terminator
//offset will contain the new length
//on error returns null, but offset will be amount of characters successful
//(and the buffer is freed)
char* sprint_object(struct jobject *j,char* buf,size_t* offset,size_t *buf_len) {
    assert(offset);
    if (!buf) {buf=malloc(8);*buf_len=8;if (!buf) {return NULL;}}

    char* s;
    size_t len;
    char num_buf[1024]={0};

    switch (j->type) {
        case JSTR:
            s = j->val.str;
            len = strlen(s);
            buf=wbuf(buf,*offset,buf_len,len+3);
            if (!buf){return NULL;}
            buf[(*offset)++]='"';
            buf[(*offset)]='\0';
            strcat(buf, s);
            *offset=strlen(buf);
            buf[(*offset)++]='"';
            buf[(*offset)]='\0';
            return buf;
        case UNKNOWN:
            s = "UNKNOWN";
            len = strlen(s);
            buf=wbuf(buf,*offset,buf_len,len+1);
            if (!buf){return NULL;}
            strcat(buf,s);
            *offset+=len;
            return buf;
        case JNUMBER:
            if (j->val.number.islong) {
                len=snprintf(num_buf,1024,"%lld", j->val.number.num.l);
            } else {
                len=snprintf(num_buf,1024,"%lf", j->val.number.num.d);
            }
            buf=wbuf(buf,*offset,buf_len,len+1);
            if (!buf){return NULL;}
            stpcpy(buf+*offset,num_buf);
            //memcpy(buf+*offset,num_buf,len+1);
            *offset+=len;
            return buf;
        case JBOOL:
            if (j->val.boolean) {
                s = "true";
                len=strlen(s);
                buf=wbuf(buf,*offset,buf_len,len+1);
                if (!buf){return NULL;}
                stpcpy(buf+*offset,s);
                *offset+=len;
                return buf;
            } else {
                s = "false";
                len=strlen(s);
                buf=wbuf(buf,*offset,buf_len,len+1);
                if (!buf){return NULL;}
                stpcpy(buf+*offset,s);
                *offset+=len;
                return buf;
            }
            break;
        case JNULL:
            s = "null";
            len=strlen(s);
            buf=wbuf(buf,*offset,buf_len,len+1);
            if (!buf){return NULL;}
            stpcpy(buf+*offset,s);
            *offset+=len;
            return buf;
        case JOBJECT:
            buf=wbuf(buf,*offset,buf_len,2);
            if (!buf){return NULL;}
            buf[(*offset)++]='{';
            buf[(*offset)]='\0';

            struct hashmap_node *node = j->val.dict->nodes;
            while (node) {
                struct key_pair *pair = node->val;
                char* s = pair->key;
                size_t len = strlen(s);
                buf=wbuf(buf,*offset,buf_len,len+2);//+2 for ""
                if (!buf){return NULL;}
                buf[(*offset)++]='"';
                stpcpy(buf+*offset, s);
                *offset+=len;
                buf=wbuf(buf,*offset,buf_len,4);
                if (!buf){return NULL;}
                buf[(*offset)++]='"';

                buf[(*offset)++]=':';
                buf[(*offset)++]=' ';
                buf[(*offset)]='\0';

                //print object
                buf=sprint_object(&pair->val,buf,offset,buf_len);
                if (!buf){return NULL;}


                node = node->next;
                if (node) {
                    buf=wbuf(buf,*offset,buf_len,3);
                    if (!buf){return NULL;}
                    buf[(*offset)++]=',';
                    buf[(*offset)++]=' ';
                    buf[(*offset)]='\0';
                }
            }
            buf=wbuf(buf,*offset,buf_len,2);
            if (!buf){return NULL;}
            buf[(*offset)++]='}';
            buf[(*offset)]='\0';
            return buf;
        case JARRAY:
            buf=wbuf(buf,*offset,buf_len,2);
            if (!buf){return NULL;}
            buf[(*offset)++]='[';
            buf[(*offset)]='\0';
            struct array array = j->val.array;
            for (size_t i = 0; i < array.len; i++) {
                buf=sprint_object(&array.arr[i],buf,offset,buf_len);
                if (!buf){return NULL;}
                if (i != array.len - 1) {
                    buf=wbuf(buf,*offset,buf_len,3);
                    if (!buf){return NULL;}
                    buf[(*offset)++]=',';
                    buf[(*offset)++]=' ';
                    buf[(*offset)]='\0';
                }
            }
            buf=wbuf(buf,*offset,buf_len,2);
            if (!buf){return NULL;}
            buf[(*offset)++]=']';
            buf[(*offset)]='\0';
            return buf;
    }
    free(buf);
    return NULL;//shouldn't reach here
}

