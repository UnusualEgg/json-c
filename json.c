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
//eh it works
#define exit_null(j,v, n)                                                        \
    if (!v) {                                                                    \
        free_object(j);                                                                 \
        return NULL;                                                             \
    }
#define exit_v(j,v, e, n)                                                        \
    if (v == e) {                                                                \
        free_object(j);                                                                 \
        return NULL;                                                        \
    }
#define exit_malloc(type_t) \
    if (!j) { \
        err->iserr=true;\
        err->type=type_t;\
        err->pos=*index;\
        err->errno_set=true;\
        return NULL;\
    }
bool unget(char c, FILE *f) { return ungetc((char)c, f)==EOF; }
//c to change
//index is 1 ahead of c
//index may be at EOF
//req:
//index should point at first char of data(or space)
//what it do:
//if not space then don't move
//set c to first non space
//set index to c
//steps:
//- c=[*index]
//- while (isspace(c)):c=[*index++] 
//- if (c!=[*index]): *index--
bool ws(char* str, size_t str_len, size_t* index,char* c,struct jerr* err) {
    do {
        if (*index>=str_len) {return true;}                                        
        *c = str[(*index)++];
        if (*c=='\n') {err->line++;err->last_nl=*index-1;}
    } while (isspace(*c));
    (*index)--;
    //now c==[*index]
    return false;
}

void copy_to(struct jobject *dest, struct jobject *src) {
    dest->type = src->type;
    dest->val = src->val;
}


void print_jerr(struct jerr* err) {
    if (!err->iserr) {return;}
    fprintf(stderr,"Error parsing %s | Position:%zu(line:%zu) | Expected: ",type_to_str(err->type),err->pos-err->last_nl,err->line+1);
    //print each expectd
    for (int i=0;i<3;i++) {
        char c=err->expected[i];
        if (c=='\0') {break;}
        fprintf(stderr, "'%c' ", c);
    }
    if (err->got) {printf("but got '%c'",err->got);}
    printf("\n");
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
struct jobject* parse_null(char* str, size_t str_len, size_t* index, struct jerr* err) {
    if (*index+3>=str_len||str[(*index)]!='n') {
        err->iserr=true;
        err->type=JNULL;
        err->pos=*index;
        err->expected[0]='n';
        return NULL;
    }
    // init object
    struct jobject* j=malloc(sizeof(struct jobject));
    exit_malloc(JNULL);
    j->type = JNULL;
    j->val.null = '\0';
    // no need to test if it's null
    // it can only be null
    (*index)+=4;
    return j;
}
struct jobject* parse_bool(char* str, size_t str_len, size_t* index, struct jerr* err) {
    // init object
    struct jobject* j=malloc(sizeof(struct jobject));
    exit_malloc(JBOOL);
    j->type = JBOOL;
    // now parse
    if (*index>=str_len) {goto bool_err;}
    char c=str[(*index)++];
    if (c == 't') {
        j->val.boolean = true;
        *index+=3;
        return j;
    } else if (c == 'f') {
        j->val.boolean = false;
        *index+=4;
        return j;
    }
bool_err:
    err->iserr=true;
    err->type=JBOOL;
    err->expected[0]='t';
    err->expected[1]='f';
    err->pos=*index;
    free_object(j);
    return NULL;
    
}
struct jobject* parse_number(char* str, size_t str_len, size_t* index, struct jerr* err) {
    // init object
    struct jobject* j=malloc(sizeof(struct jobject));
    exit_malloc(JNUMBER);
    j->type = JNUMBER;
    j->val.number.islong = true;
    // now parse got num

    if (*index>=str_len) {goto number_err;}
    char c=str[(*index)++];
    char buf[1024] = {0};
    buf[0] = c;
    int i = 0;
    while (isdigit(c) || c == '.' || c == '-') {
        if (*index>=str_len) {goto number_err;}
        buf[i] = c;
        c=str[(*index)++];
        if (c == '.') {
            j->val.number.islong = false;
        }
        i++;
    }
    if (i==0) {goto number_err;}//expected digit,'.', or '-'
    if (j->val.number.islong) {
        long long l;
        errno = 0;
        l = strtoll(buf, NULL, 10); // assume base 10
        if (errno == EINVAL) {
            err->errno_set=true;
            goto number_err;
        }
        j->val.number.num.l = l;
    } else {
        double d;
        char * ptr=buf;
        d = strtod(buf, &ptr);
        if (ptr==buf) {
            goto number_err;
        }
        //note: errno may be set to ERANGE but it's finnne
        j->val.number.num.d = d;
    }
    (*index)--;

    return j;

number_err:
    err->iserr=true;
    err->type=JNUMBER;
    err->pos=*index;
    free_object(j);
    return NULL;
}
//TODO finish converting to string
struct jobject* parse_string(char* str, size_t str_len, size_t* index, struct jerr* err) {
    // init object
    struct jobject* j=malloc(sizeof(struct jobject));
    exit_malloc(JSTR);
    j->type = JSTR;
    j->val.str=NULL;
    // now parse got "
    if (*index>=str_len||str[*index]!='\"') {goto expected_quote;}
    char c=str[(*index)++];
#if dbp
    printf("begin of parse_string: %c\n", c);
#endif

    bool escaped = false;
    size_t len = 0;
    // get length
#if dbp
    printf("getlen:\"");
#endif
    // get initial letter/whatever
    if (*index>=str_len) {goto expected_quote;}
    c=str[(*index)++];
    while (c != '\"') {
#if dbp
        printf("%c", c);
#endif
        if (!escaped && c == '\\') {
            escaped = true;
            if (*index>=str_len) {goto string_err;}
            c=str[(*index)++];
            continue;
        }
        escaped = false;
        len++;
        if (*index>=str_len) {goto expected_quote;}
        c=str[(*index)++];
    }
#if dbp
    printf("\": %zu\n", len);
#endif
    //now we got the literal characters in 
    // actually parse stirng
    *index-=(len+1);//+1 bc " I think
    char *buf = malloc(len + 1); //+1 for '\0'
    if (!buf) {err->errno_set=true;goto string_err;}
    exit_null(j,buf, "malloc");
    j->val.str = buf;

    size_t i = 0;
#if dbp
    printf("write:\"");
#endif
    c=str[(*index)++];
#if dbp
    if (c!='"') {
        printf("%c", c);
    }
#endif
    while (c != '"') {
        if (!escaped && c == '\\') {
            escaped = true;
            //NOTE: this should be fine
            c=str[(*index)++];
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
        if (*index>=str_len) {goto expected_quote;}
        c=str[(*index)++];
#if dbp
        printf("%c", c);
#endif
    }
#if dbp
    printf("\n");
#endif
    buf[len] = '\0';

#if dbp
    printf("after parse_string: %c. str:%s\n", c, buf);
#endif
    return j;
expected_quote:
    err->expected[0]='\"';
string_err:
    //only free inner string if we have it
    if (j->val.str) {
        free_object(j);
    } else {
        free(j);
    }
    err->iserr=true;
    err->type=JSTR;
    err->pos=*index;
    return NULL;
}

struct jobject* parse_array(char* str, size_t str_len, size_t* index, struct jerr* err) {
    // init object
    struct jobject* j=malloc(sizeof(struct jobject));
    exit_malloc(JARRAY);
    j->type = JARRAY;
    j->val.array.arr = NULL;
    j->val.array.len = 0;
    size_t *len = &(j->val.array.len);
    // struct jobject* ptr = j->val.dict;//shortcut
    // we at [
    if (*index>=str_len||str[*index]!='[') {
        err->expected[0]='[';
        goto array_err;
    }
    //go to after [
    (*index)++;
    //and skip whitespace
    char c='\0';
    if (ws(str,str_len,index, &c, err)) {goto array_err;}
#if dbp
    printf("after ws after [:%c\n", c);
#endif

    //we at char after [
    while (c != ']') {
#if dbp
        printf("c:%c\n", c);
#endif

        // parse value
        struct jobject* element = parse_any(str,str_len,index,err);
        if (!element){free_object(j);return NULL;}
        (*len)++;
        struct jobject **ptr;
        //check if we have an array
        if (!j->val.array.arr) {
            ptr = malloc(sizeof(struct jobject*));
            if (!ptr) {err->errno_set=true;goto array_err;}
            j->val.array.arr = ptr;
        } else {
            ptr = realloc(j->val.array.arr, *len * sizeof(struct jobject*));
            if (!ptr){err->errno_set=true;goto array_err;}
            exit_null(j,ptr, "realloc");
            j->val.array.arr = ptr;
        }
#if dbp
        printf("*len(of array)=%zu\n",*len);
#endif
        //len-1 for last element
        ptr[*len -1]=element;
#if dbp
        printf("element: ");
        print_object(ptr[*len - 1]);
        printf("\n");
#endif
        //may be whitespace after each value
        // go up to the comma or closing bracket
        if (ws(str,str_len,index, &c, err)||(c!=','&&c!=']')) {
            err->expected[0]=',';
            err->expected[1]=']';
            goto array_err;
        }
#if dbp
        printf("after whitespace: %c\n", c);
#endif
        // we at comma or ]
        //skip comma and go to first char of value but discard it for parse_any
        if (c==',') {
            //skip comma
            (*index)++;
            if (ws(str,str_len,index, &c, err)) {
                err->expected[0]=']';
                goto array_err;
            }
            //don't increment index
            //so parse_any can read it
        }
    }
    (*index)++;
    return j;
array_err:
    free_object(j);
    err->iserr=true;
    err->type=JARRAY;
    err->pos=*index;
    return NULL;
}

struct jobject* parse_object(char* str, size_t str_len, size_t* index, struct jerr* err) {
    // init object
    struct jobject* j=malloc(sizeof(struct jobject));
    exit_malloc(JOBJECT);
    j->type = JOBJECT;
    j->val.dict = hm_create();
    hashmap_t *hm = j->val.dict; // shortcut
    if (!hm) {
        err->errno_set=true;
        goto object_err;
    }
    // we at {
    if (*index>=str_len||str[*index]!='{') {err->expected[0]='{';goto object_err;}
    char c=str[(*index)];//;'{'
//{ ""
#if dbp
    printf("parse object %c\n",c);
#endif
    while (c != '}') {
        // go past whitespace after { or ,
        (*index)++;
        if (ws(str,str_len,index, &c, err)) {err->expected[0]='}';goto object_err;}
#if dbp
        printf("begin of key: %c\n", c);
        fflush(stdout);
#endif
        // parse key
        struct jobject* s = parse_string(str,str_len,index,err);
        if (!s) {free_object(j);return NULL;}//err already set
        if (s->type!=JSTR) {free_object(s);err->expected[0]='"';goto object_err;}
        char *key = s->val.str;//already malloc'd
        free(s);//free jobject but not the inner string
        // go to :
        if (*index>=str_len) {err->expected[0]=' ';err->expected[1]=':';free(key);goto object_err;}
        c=str[*index];//;'{'
        if (isspace(c)) {
            if (ws(str,str_len,index, &c, err)) {free(key);goto object_err;}
        }
        //should be at :
        if (*index>=str_len||str[*index]!=':') {err->expected[0]=':';free(key);goto object_err;}
#if dbp
        printf("after key: %c\n", c);
        fflush(stdout);
#endif
        (*index)++;
        // get whitespace and first char of value after :
        if (ws(str,str_len,index, &c, err)) {err->expected[0]='v';free(key);goto object_err;}

        // parse value
        struct jobject* child = parse_any(str,str_len,index,err);
        if (!child) {free(key);free_object(j);return NULL;}
        struct key_pair pair = {key, child};
#if dbp
        printf("hm_setx(\"%s\",", key);
        print_object(child);
        printf(")\n");
#endif
        hm_setx(hm, hm_new(hm, key), &pair, sizeof(pair));
#if dbp
        hm_debug(hm);
        printf("parsed value\n");
#endif

        // go up to comma or }
        if (ws(str,str_len,index, &c, err)) {err->expected[0]='}';err->expected[1]=',';goto object_err;}
        if (c!='}'&&c!=',') {err->expected[0]='}';err->expected[1]=',';goto object_err;}
#if dbp
        printf("obj: %c/%d\n", c, c);
        fflush(stdout);
#endif
    }
    (*index)++;
    return j;
object_err:
    free_object(j);
    err->iserr=true;
    err->type=JOBJECT;
    err->pos=*index;
    if (*index<str_len) {err->got=str[*index];}
    return NULL; 
}

enum type find_type(char* str, size_t str_len, size_t* index) {
    if (*index>=str_len) {return UNKNOWN;}                                        
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
struct jobject* parse_any(char* str, size_t str_len, size_t* index, struct jerr* err) {
    switch (find_type(str,str_len,index)) {
        case JOBJECT:
            return parse_object(str,str_len,index,err);
        case JSTR:
            return parse_string(str,str_len,index,err);
        case JNUMBER:
            return parse_number(str,str_len,index,err);
        case JARRAY:
            return parse_array(str,str_len,index,err);
        case JBOOL:
            return parse_bool(str,str_len,index,err);
        case JNULL:
            return parse_null(str,str_len,index,err);
        case UNKNOWN:default:
            err->iserr=true;
            err->type=UNKNOWN;
            err->pos=*index;
            return NULL;
            //fprintf(stderr, "Unknown type. pos:%ld", ftell(f));
    }
}
//sets errno
struct jobject* load_file(FILE *f,char** str_buf,size_t* str_len,struct jerr* err) {
    // load whatever kinda object this is
    *str_len=0;

    fseek(f,0,SEEK_END);
    size_t len=(size_t)ftell(f);
    printf("len:%zu\n",len);
    fseek(f,0,SEEK_SET);
    *str_buf=malloc(len);
    if (!(*str_buf)) {err->iserr=true;err->errno_set=true;return NULL;}
    *str_len=len;
    fread(*str_buf,1,len,f);
    size_t index=0;
    
    return parse_any(*str_buf,len,&index,err);
}
//set errno
struct jobject* load_fn(char *fn,char** str_buf,size_t* str_len,struct jerr* err) {
    FILE *f = fopen(fn, "r");
    if (!f) {err->iserr=true;err->errno_set=true;return NULL;}

    struct jobject* result = load_file(f,str_buf,str_len,err);

    if (fclose(f)==EOF) {return NULL;}
    return result;
}

// frees j and its value(s) recursively
void free_object(struct jobject *j) {
    switch (j->type) {
        case JOBJECT:;
                     // free every value
                     struct hashmap_node *node = j->val.dict->nodes;
                     while (node) {
                         // each node is a key-value pair. the key ptr and value ptr need to be
                         // freed
                         struct key_pair *pair = (node->val);
                         //the STRING key
                         free(pair->key);
                         free_object(pair->val);
                         node = node->next;
                     }
                     // after that, the key-value pair structs themselves are freed
                     hm_free(j->val.dict);
                     break;
        case JARRAY:
                     //free each object
                     for (size_t i=0; i<(j->val.array.len); i++) {
                         free_object(j->val.array.arr[i]);
                     }
                     //free vec
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
    free(j);
}
void print_object(struct jobject* j) {
    fprint_object(stdout,j);
}
//true if success
bool serialize(char* fn, struct jobject *j) {
    FILE* f=fopen(fn,"w");
    if (!f) {return false;}
    fprint_object(f,j);
    fclose(f);
    return true;
}
#define exit_neg(v,msg) if (v<0) {return false;}
bool fprint_object(FILE* f,struct jobject *j) {
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
                fprint_object(f,pair->val);
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
                fprint_object(f,array.arr[i]);
                if (i != array.len - 1) {
                    exit_neg(fprintf(f,", "),"fprint_object");
                }
            }
            fprintf(f,"]");
            break;
    }
    return true;
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
                buf=sprint_object(pair->val,buf,offset,buf_len);
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
                buf=sprint_object(array.arr[i],buf,offset,buf_len);
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

