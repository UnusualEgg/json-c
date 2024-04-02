#include <stdio.h>
#include <hashmap.h>
#include "json.h"

#define PROJECT_NAME "json"

int main(int argc, char **argv) {
    if(argc != 2) {
        printf("%s takes 2 arguments.\n", argv[0]);
        return 1;
    }
    printf("This is project %s.\n", PROJECT_NAME);
    struct jobject* j = load_fn(argv[1]);
    if (!j) {perror("load_fn");exit(EXIT_FAILURE);}
    print_object(j);
    printf("\n");
    free_object(j);

    return 0;
}
