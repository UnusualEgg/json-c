#include <stdbool.h>
#include <hashmap.h>
#include <stdio.h>

//abbreviated: 
//- STRING->STR (except parse_string)
//- BOOLEAN->BOOL (except jobject.val.boolean)
//- VALUE->VAL (ALWAYS)

//types
enum type {
    UNKNOWN=0,
    JOBJECT,
    JSTR,
    JNUMBER,
    JBOOL,
    JNULL,
    JARRAY
};
//underlying structs
//(attribute with same name is abbr.
struct number {
    bool islong;
    union {
        long long l;
        double d;
    } num;
};
struct jobject;
struct array {
    size_t len;
    struct jobject** arr;
};
//typedefs
typedef hashmap_t* jdict;//<any key,struct key_pair value>
typedef char* jstr;
typedef struct number jnumber;
typedef bool jbool;
typedef char jnull;
typedef struct array jarray;

//this is a wrapper for each type
//so you can pass it between functions 
//instead of the individual types
struct jobject {
    enum type type;
    union {
        jdict dict;
        jstr str;
        jnumber number;
        jbool boolean;
        jnull null;
        jarray array;
    } val;
};
//so we have the (unhashed) key string
struct key_pair {
    char* key;
    struct jobject* val;
};
//errors
struct jerr {
    bool iserr;
    bool errno_set;
    enum type type; //when parsing this type
    size_t pos;
    char expected[3];//max number of different expected chars
    char got;
    size_t line;
    size_t last_nl;//for calculating pos
};

//functions
void print_jerr(struct jerr* err);
const char* type_to_str(enum type type);
void copy_to(struct jobject* dest, struct jobject* src);

//parsing
//these are recursive
//Required initialized: str, str_len, index, err
//index is for keeping positon in the string str
//err will be set in case of an error
struct jobject* parse_any(char* str, size_t str_len, size_t* index, struct jerr* err); 
struct jobject* parse_null(char* str, size_t str_len, size_t* index, struct jerr* err); 
struct jobject* parse_bool(char* str, size_t str_len, size_t* index, struct jerr* err); 
struct jobject* parse_number(char* str, size_t str_len, size_t* index, struct jerr* err); 
struct jobject* parse_string(char* str, size_t str_len, size_t* index, struct jerr* err); 
struct jobject* parse_array(char* str, size_t str_len, size_t* index, struct jerr* err); 
struct jobject* parse_object(char* str, size_t str_len, size_t* index, struct jerr* err); 

//may return UNKNOWN
enum type find_type(char* str, size_t str_len, size_t* index);

//load file
struct jobject* load_file(FILE *f,char** str_buf,size_t* str_len,struct jerr* err);
//set errno
struct jobject* load_fn(char *fn,char** str_buf,size_t* str_len,struct jerr* err);
//unload
void free_object(struct jobject* j);

//output object
void print_object(struct jobject* j);
//true if error
bool fprint_object(FILE* f,struct jobject *j);
bool serialize(char* fn, struct jobject *j);
//see json.c comments for details
char* sprint_object(struct jobject *j,char* buf,size_t* offset,size_t* buf_len);


