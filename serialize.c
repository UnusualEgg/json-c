#include "json.h"
#include <hashmap.h>
#include <stdio.h>

#define PROJECT_NAME "json"

int main(int argc, char **argv) {
    if (argc != 3) {
        printf("%s takes 2 arguments.\n", argv[0]);
        return 1;
    }
    printf("This is project %s.\n", PROJECT_NAME);
    char *buf = NULL;
    size_t len = 0;
    struct jerr err = {0};
    struct jvalue *j = json_load_filename(argv[1], &buf, &len, &err);
    free(buf);
    if (!j) {
        if (err.errno_set) {
            jerr_print(&err);
            perror("load_fn");
        } else {
            jerr_print(&err);
        }
        exit(EXIT_FAILURE);
    }
    jvalue_store_filename(argv[2], j);
    jvalue_free(j);

    return 0;
}
