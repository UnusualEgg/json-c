#ifndef JSON_MALLOC
#define JSON_MALLOC(s) malloc(s)
#endif
#ifndef JSON_FREE
#define JSON_FREE(p) free(p)
#endif
#ifndef JSON_REALLOC
#define JSON_REALLOC(p, s) realloc(p, s)
#endif
#ifndef HM_MALLOC
#define HM_MALLOC(s) JSON_MALLOC(s)
#endif
#ifndef HM_FREE
#define HM_FREE(p) JSON_FREE(p)
#endif

#include <hashmap.h>
#include <stdbool.h>
#include <stdio.h>


// abbreviated:
//- STRING->STR (except parse_string)
//- BOOLEAN->BOOL (except jobject.val.boolean)
//- VALUE->VAL (ALWAYS)

// types
enum jtype { UNKNOWN = 0, JOBJECT, JSTR, JNUMBER, JBOOL, JNULL, JARRAY };
// underlying structs
//(attribute with same name is abbr.
struct number {
    bool islong;
    union {
        long long l;
        double d;
    } num;
};
struct jvalue;
struct array {
    size_t len;
    struct jvalue **arr;
};
// typedefs
typedef hashmap_t *jobj; //<any key,struct key_pair value>
typedef char *jstr;
typedef struct number jnumber;
typedef bool jbool;
typedef char jnull;
typedef struct array jarray;

// this is a wrapper for each type
// so you can pass it between functions
// instead of the individual types
struct jvalue {
    enum jtype type;
    union {
        jobj obj;
        jstr str;
        jnumber number;
        jbool boolean;
        jnull null;
        jarray array;
    } val;
};
// so we have the (unhashed) key string
struct key_pair {
    // not const because it's owned
    char *key;
    struct jvalue *val;
};
// errors
struct jerr {
    bool iserr;
    bool errno_set;
    enum jtype type; // when parsing this type
    size_t pos;
    char expected[3]; // max number of different expected chars
    char got;
    size_t line;
    size_t last_nl; // for calculating pos
};

// functions
// new
struct jvalue *jbool_new(bool b);
struct jvalue *jnull_new(void);
struct jvalue *jnum_new(double d);
struct jvalue *jnum_new_d(long long l);
struct jvalue *jstr_new(char *str);
struct jvalue *jobj_new(void);
struct jvalue *jarray_new(void);

// print error with original string buffer (usually file buffer) (char *str)
void jerr_print_str(struct jerr *err, const char *str);
#define jerr_print(err) jerr_print_str(err, (void *)0)
const char *jtype_to_str(enum jtype type);
// returns:a cloned allocated jvalue
// everything is
struct jvalue *jvalue_clone(struct jvalue *j);
// manipulation
struct jvalue *jobj_get(struct jvalue *value, const char *key);
// returns true if found and deleted
bool jobj_del(struct jvalue *value, const char *key);
char *jstr_get(struct jvalue *value);
// doesn't allocate
struct jvalue *jobj_set(struct jvalue *obj, const char *key, struct jvalue *value);
struct jvalue *jarray_get(struct jvalue *value, size_t index);
const size_t *jarray_len(struct jvalue *value);

// parsing
// these are recursive
// Required initialized: str, str_len, index, err
// index is for keeping positon in the string str
// err will be set in case of an error
// all params are in and out params
struct jvalue *jparse_any(char *str, size_t str_len, size_t *index, struct jerr *err);
struct jvalue *jparse_null(char *str, size_t str_len, size_t *index, struct jerr *err);
struct jvalue *jparse_bool(char *str, size_t str_len, size_t *index, struct jerr *err);
struct jvalue *jparse_number(char *str, size_t str_len, size_t *index, struct jerr *err);
struct jvalue *jparse_string(char *str, size_t str_len, size_t *index, struct jerr *err);
struct jvalue *jparse_array(char *str, size_t str_len, size_t *index, struct jerr *err);
struct jvalue *jparse_object(char *str, size_t str_len, size_t *index, struct jerr *err);

// figures out the type based on the begining characters in str
//  may return UNKNOWN
enum jtype jfind_type(char *str, size_t str_len, size_t *index);

// load file
// only keep str for errors... and for users
struct jvalue *json_load_file(FILE *f, char **str_buf, size_t *str_len, struct jerr *err);
// set errno
struct jvalue *json_load_filename(const char *fn, char **str_buf, size_t *str_len, struct jerr *err);

// store file
// true if error
bool jvalue_store_filename(const char *fn, struct jvalue* j);

// unload
void jvalue_free(struct jvalue *j);

// output object
void jvalue_print(struct jvalue *j);
// true if error
bool jvalue_fprint(FILE *f, struct jvalue *j);
bool jvalue_store_filename(const char *fn, struct jvalue *j);
// see json.c comments for details
char *jvalue_sprint(struct jvalue *j, char *buf, size_t *offset, size_t *buf_len);
char *json_sprint_string(const char *str, char *buf, size_t *offset, size_t *buf_len);
// will return an allocated string
char *jvalue_sprint_alloc(struct jvalue *j);
