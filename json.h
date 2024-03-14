#include <stdbool.h>
#include <hashmap.h>
#include <stdio.h>

//types
enum type {
    UNKNOWN,
    JOBJECT,
    JSTR,
    JNUMBER,
    JBOOL,
    JNULL,
    JARRAY
};
//underlying structs
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
    struct jobject* arr;
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
    struct jobject val;
};


//functions
const char* type_to_str(enum type type);
void copy_to(struct jobject* dest, struct jobject* src);

//parsing
//this will call one of the other functions
struct jobject parse_any(FILE *f);
struct jobject parse_bool(FILE *f);
struct jobject parse_number(FILE *f);
struct jobject parse_string(FILE *f);
struct jobject parse_array(FILE *f);
struct jobject parse_object(FILE *f);

enum type find_type(FILE *f);

//load file
struct jobject load_file(FILE *f);
struct jobject load_fn(char *fn);
//unload
void free_object(struct jobject* j);

//output object
void print_object(struct jobject* j);
void fprint_object(FILE* f,struct jobject *j);
void serialize(char* fn, struct jobject *j);
//see json.c comments for details
char* sprint_object(struct jobject *j,char* buf,size_t* offset,size_t* buf_len);


