#include "json.h"
#include <hashmap.h>
#include <stdbool.h>
#include <stdio.h>

#define PROJECT_NAME "json"

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("%s takes 2 arguments.\n", argv[0]);
        return 1;
    }
    printf("This is project %s.\n", PROJECT_NAME);
    char *buf = NULL;
    size_t len = 0;
    struct jerr err = {.iserr = false};
    struct jvalue *j = load_filename(argv[1], &buf, &len, &err);
    if (!j) {
        if (err.errno_set) {
            jerr_print_str(&err, buf);
            perror("load_fn");
        } else {
            jerr_print_str(&err, buf);
        }
        free(buf);
        exit(EXIT_FAILURE);
    }
    // print_value(j);
    printf("\n");
    char *out_buf = sprint_value_normal(j);
    printf("%s\n", out_buf);
    free_object(j);
    free(buf);
    free(out_buf);

    return 0;
}
