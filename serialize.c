#include <stdio.h>
#include <hashmap.h>
#include "json.h"

#define PROJECT_NAME "json"

int main(int argc, char **argv) {
    if(argc != 3) {
        printf("%s takes 2 arguments.\n", argv[0]);
        return 1;
    }
    printf("This is project %s.\n", PROJECT_NAME);
    char* buf=NULL;
    size_t len=0;
    struct jerr err={0}; 
    struct jobject* j = load_fn(argv[1],&buf,&len,&err);
    free(buf);
    if (!j) {
        if (err.errno_set) {
            print_jerr(&err);
            perror("load_fn");
        } else {
            print_jerr(&err);
        }
        exit(EXIT_FAILURE);
    }
    serialize(argv[2],j);
    free_object(j);

    return 0;
}
