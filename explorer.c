#include "json.h"
#include <errno.h>
#include <menu.h>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BACK "Back"

void handler(void) { endwin(); }
void *exit_null(void *v, const char *msg) {
    if (!v) {
        endwin();
        perror(msg);
        exit(EXIT_FAILURE);
    }
    return v;
}
const char *menu_str(int errnum) {
    switch (errnum) {
        case E_OK:
            return "OK";
        case E_SYSTEM_ERROR:
            return strerror(errno);
        case E_NOT_CONNECTED:
            return "No items are connected to the menu.";
        case E_BAD_ARGUMENT:
            return "Routine detected an incorrect or out-of-range argument.";
        case E_POSTED:
            return "The menu has already been posted.";
        case E_CONNECTED:
            return "Item is connected to a menu.";
        default:
            return "Unknown menu_errnum";
    }
}
void err_menu(int v, const char *msg) {
    if (v != E_OK) {
        endwin();
        fprintf(stderr, "%s: %s", msg, menu_str(v));
        fflush(stderr);
        exit(EXIT_FAILURE);
    }
}
void *err_ptr(void *v, const char *msg) {
    if (!v) {
        endwin();
        fprintf(stderr, "%s: %s", msg, menu_str(errno));
        fflush(stderr);
        exit(EXIT_FAILURE);
    }
    return v;
}

// append val to array items at index *item_i (possibly extend strings)
// doesn't inc item_i
ITEM *append_item(ITEM ***items, size_t *items_len, int item_i, ITEM *val) {
    if (((size_t)item_i) + 1 >= *items_len) {
        ITEM **new = realloc(*items, (item_i + 2) * sizeof(ITEM *));
        exit_null(new, "realloc");
        *items = new;

        new[item_i] = val;

        new[item_i + 1] = NULL;

        (*items_len)++;
        return val;
    }
    (*items)[item_i] = val;
    (*items)[(item_i) + 1] = NULL;
    return val;
}
// append val to array strings at index *item_i (possibly extend strings)
// doesn't inc item_i
char *append_str(char ***strings, size_t *strings_len, int item_i, char *val) {
    if ((size_t)item_i >= *strings_len) {
        char **new = realloc(*strings, (item_i + 2) * sizeof(char *));
        exit_null(new, "realloc");

        new[item_i] = val;

        *strings = new;
        (*strings_len)++;
        return val;
    }
    (*strings)[item_i] = val;
    return val;
}
void free_items(ITEM **items, size_t items_len, char **strings, size_t strings_len, int *item_i) {
    for (size_t i = 0; i < items_len && i < (size_t)*item_i; i++) {
        if (!items[i]) {
            fprintf(stderr,
                    "ERROR:(item_len{%zu}!=actual len{%zu}, item_i:%d)items[%zu]==NULL(%p)\n",
                    items_len, i, *item_i, i, (void *)items[i]);
            break;
        }
        err_menu(free_item(items[i]), "free_item");
        items[i] = NULL;
    }
    for (size_t i = 0; i < strings_len && i < (size_t)item_i; i++) {
        free(strings[i]);
        strings[i] = NULL;
    }
    *item_i = 0;
}
// ITEM** = array of pointers to allocated ITEMs
// ITEM*** = pointer to array
// add the value of jobject
void add_items(struct jvalue *curr, ITEM ***items_array, size_t *items_len, char ***strings_array,
               size_t *strings_len, int *item_i) {
    char *buf, *src;
    char **strings = *strings_array;
    size_t buf_len = COLS;

    switch (curr->type) {
        case JSTR:
            src = curr->val.str;
            if (strlen(src) == 0) {
                src = " ";
            }
            buf = malloc(buf_len);
            exit_null(buf, "malloc");
            buf = sprint_value(curr, buf, NULL, &buf_len);
            exit_null(buf, "sprint_value");
            append_str(strings_array, strings_len, *item_i, buf);
            strings = *strings_array;
            // debug
            // fprintf(stderr,"str:\"%s\"\n",strings[*item_i]);
            append_item(items_array, items_len, *item_i, new_item(strings[*item_i], ""));
            err_ptr((*items_array)[*item_i], "new_item(str)");
            (*item_i)++;
            break;
        case JNUMBER:
        case JBOOL:
        case JNULL:
        case UNKNOWN:;
            // just use sprint_object to get string representation
            size_t offset = 0;
            buf = malloc(buf_len);
            exit_null(buf, "malloc");
            buf = exit_null(sprint_value(curr, buf, &offset, &buf_len), "sprint_object");
            // exit_null((buf[0]),"buf[0]");
            buf[buf_len - 1] = 0;
            // debug
            // fprintf(stderr,"buf(%zu):\"%s\"\n",buf_len,buf);

            append_str(strings_array, strings_len, *item_i, buf);

            strings = *strings_array;
            append_item(items_array, items_len, *item_i, new_item(strings[*item_i], ""));
            err_ptr((*items_array)[*item_i], "new_item(number/bool/null/unknown)");
            (*item_i)++;
            break;

        case JOBJECT:
            // just in case it's empty
            strings = *strings_array;

            // print keys
            struct hashmap_node *node = curr->val.obj->nodes;
            while (node) {
                struct key_pair *pair = (struct key_pair *)node->val;
                src = pair->key;
                if (*src == '\0') {
                    src = " ";
                }
                size_t len = strlen(src);
                buf = malloc(len + 1 + 8 + 1 + 1); //(type)\0
                exit_null(buf, "malloc");
                memcpy(buf, src, len + 1);
                buf[len] = '(';
                buf[len + 1] = '\0';
                strncat(buf, type_to_str(pair->val->type), 8);
                strcat(buf, ")");
                append_str(strings_array, strings_len, *item_i, buf);
                strings = *strings_array;

                append_item(items_array, items_len, *item_i, new_item(strings[*item_i], ""));
                err_ptr((*items_array)[*item_i], "new_item");

                /*
                printw("i:%d errno:%d=%s bad_arg:%d system_err:%d conn:%d key:\"%s\"",
                        item_i,errno,strerror(errno),
                        errno==E_BAD_ARGUMENT,errno==E_SYSTEM_ERROR,errno==E_CONNECTED,
                        ((struct key_pair*)node->val)->key);
                refresh();
                //sleep(10);
                */
                (*item_i)++;
                node = node->next;
            }
            break;
        case JARRAY:;
            // just in case it's empty
            strings = *strings_array;
            jarray arr = curr->val.array;
            for (unsigned int i = 0; i < arr.len; i++) {
                buf = malloc(COLS);
                exit_null(buf, "malloc");
                switch (arr.arr[i]->type) {
                    case JOBJECT:
                        snprintf(buf, COLS, "%u: object", i);
                        break;
                    case JARRAY:
                        snprintf(buf, COLS, "%u: array[%zu]", i, arr.arr[i]->val.array.len);
                        break;
                    case JSTR:;
                        size_t len = strlen(arr.arr[i]->val.str);
                        if (len < (size_t)COLS - 4) {
                            snprintf(buf, COLS, "%u: \"%s\"", i, arr.arr[i]->val.str);
                        } else {
                            snprintf(buf, COLS, "%u: string[%zu]", i, len);
                        }
                        break;
                    case JNUMBER:
                    case JBOOL:
                    case JNULL:
                    case UNKNOWN:;
                        size_t offset = 0;
                        size_t buf_len = COLS;
                        char *buf_str = malloc(buf_len);
                        exit_null(buf_str, "malloc");
                        buf_str = exit_null(sprint_value(arr.arr[i], buf_str, &offset, &buf_len),
                                            "sprint_object");
                        snprintf(buf, COLS - 1, "%u: %s", i, buf_str);
                        free(buf_str);
                        break;
                }
                append_str(strings_array, strings_len, *item_i, buf);
                strings = *strings_array;

                append_item(items_array, items_len, *item_i, new_item(strings[*item_i], ""));
                err_ptr((*items_array)[*item_i], "new_item(add_itmes)");
                (*item_i)++;
            }
            break;
    }
    strings[*item_i] = NULL;
    (*items_array)[*item_i] = NULL;
}

void add_type(enum type type, ITEM ***items, size_t *items_len, char ***strings,
              size_t *strings_len, int *item_i) {
    char *type_buf = strdup(type_to_str(type));
    exit_null(type_buf, "strdup");
    append_str(strings, strings_len, *item_i, type_buf);
    append_item(items, items_len, *item_i, new_item("type", (*strings)[*item_i]));
    err_ptr((*items)[*item_i], "new_item");
    err_menu(item_opts_off((*items)[*item_i], O_SELECTABLE), "item_opts_off(selectable)");
    (*item_i)++;
}
void add_back(ITEM ***items, size_t *items_len, char ***strings, size_t *strings_len, int *item_i) {
    char *buf = strdup(BACK);
    exit_null(buf, "strdup");

    append_str(strings, strings_len, *item_i, buf);
    append_item(items, items_len, *item_i, new_item((*strings)[*item_i], ""));
    err_ptr((*items)[*item_i], "new_item");
    (*item_i)++;
}

// levels is a pointer to a list of addrs(pointers) each to a jobject
struct jvalue *append_level(struct jvalue ***levels, size_t *levels_len, int *level,
                            struct jvalue *val) {
    struct jvalue **buf = *levels;
    if ((size_t)*level >= *levels_len) {
        buf = realloc(*levels, (*level + 1) * sizeof(struct jvalue *));
        exit_null(buf, "realloc");
        *levels = buf;
        *levels_len = ((size_t)*levels) + 1;
    }
    buf[*level] = val;
    (*level)++;
    return val;
}

void redraw(WINDOW *wm, MENU *m, struct jvalue *curr, int *item_i, ITEM ***items, size_t *items_len,
            char ***strings, size_t *strings_len, int level) {
    // clear da menu
    err_menu(unpost_menu(m), "unpost_menu");
    ITEM **empty = {NULL};
    err_menu(set_menu_items(m, empty), "set_menu_items(empty)");
    free_items(*items, *items_len, *strings, *strings_len, item_i);
    // reprint the menu
    if (level > 0) {
        add_back(items, items_len, strings, strings_len, item_i);
    }
    add_type(curr->type, items, items_len, strings, strings_len, item_i);
    add_items(curr, items, items_len, strings, strings_len, item_i);
    // debug
    // fprintf(stderr,"last:%p\n",(void*)items[item_i]);
    err_menu(set_menu_items(m, *items), "set_menu_items(new)");
    // set_menu_format(m,LINES,COLS);

    err_menu(post_menu(m), "post_menu");
    wrefresh(wm);
}
// json explore and editor
int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "%s takes 1 arg.", argv[0]);
        return EXIT_FAILURE;
    }
    // start curses mode
    initscr();
    atexit(handler);

    printw("Loading file");
    refresh();

    // parsing and such
    char *buf = NULL;
    size_t len = 0;
    struct jerr err = {0};
    struct jvalue *j = load_filename(argv[1], &buf, &len, &err);
    if (!j) {
        endwin();
        if (err.errno_set) {
            print_jerr_str(&err, buf);
            perror("load_fn");
        } else {
            print_jerr_str(&err, buf);
        }
        free(buf);
        exit(EXIT_FAILURE);
    }
    free(buf);
    if (!j) {
        endwin();
        perror("load_fn");
        exit(EXIT_FAILURE);
    }
    size_t levels_len = 10;
    // list of pointers
    struct jvalue **levels = calloc(levels_len, sizeof(struct jvalue *));
    int level = 0;
    struct jvalue *curr = j;
    // only when going down
    // struct jobject* curr = append_level(&levels, &levels_len, &level, &j);

    // init
    size_t strings_len = 10;
    char **strings = calloc(strings_len, sizeof(char *));
    size_t items_len = 10;
    ITEM **items = (ITEM **)calloc(items_len, sizeof(ITEM *));
    // menu
    MENU *m = new_menu(items);
    exit_null(m, "new_menu");
    int lines = LINES - 3;
    int cols = COLS - 4;
    err_menu(set_menu_mark(m, "> "), "set_menu_mark");
    err_menu(set_menu_spacing(m, 1, 1, 1), "set_menu_mark");
    // subwindow
    keypad(stdscr, TRUE);
    WINDOW *wm = newwin(lines, cols, 1, 1);
    keypad(wm, TRUE);
    cbreak();
    noecho();
    err_menu(set_menu_win(m, wm), "set_menu_win");
    err_menu(set_menu_sub(m, derwin(wm, lines - 1, cols - 2, 1, 1)), "set_menu_sub");
    // TODO figure out why only liek 15 items show up at a time
    // then it srolls
    set_menu_format(m, lines - 2, COLS);

    mvprintw(0, 0, "Json Explorer");
    box(wm, 0, 0);
    refresh();
    // print the menu
    int item_i = 0;
    // no bc we at top level
    // add_back(items,&items_len,strings, &strings_len,&item_i);
    add_type(curr->type, &items, &items_len, &strings, &strings_len, &item_i);
    // we need a list to go back
    add_items(curr, &items, &items_len, &strings, &strings_len, &item_i);
    err_menu(set_menu_items(m, items), "set_menu_items");
    err_menu(post_menu(m), "post_menu");
    wrefresh(wm);

    // loop
    int c;
    ITEM *curr_item;
    while ((c = getch()) != 'q') {
        switch (c) {
            case KEY_DOWN:
            case 's':
                menu_driver(m, REQ_DOWN_ITEM);
                break;
            case KEY_UP:
            case 'w':
                menu_driver(m, REQ_UP_ITEM);
                break;
            case KEY_BACKSPACE:
            case 'b':
            case KEY_LEFT:
                if (level > 0) {
                    levels[level--] = NULL;
                    curr = levels[level];
                    redraw(wm, m, curr, &item_i, &items, &items_len, &strings, &strings_len, level);
                }
                break;
            case 10:
            case 32:
            case KEY_RIGHT: // enter or space
                // move into
                curr_item = current_item(m);
                exit_null(curr_item, "current_item");
                int index = item_index(curr_item);
                if (index == ERR) {
                    endwin();
                    perror("item_ined");
                    exit(EXIT_FAILURE);
                }

                // make back button -1
                // and type 0
                if (level > 0) {
                    index--;
                }
                if (index == 0) {
                    break;
                } // type

                if (index == -1 && level > 0) {
                    // debug
                    // if (level==0) {fprintf(stderr,"hit back at level 0\n");break;}
                    levels[level--] = NULL;
                    curr = levels[level];
                } else {
                    // make items start at 0
                    index--;
                    // do JARRAY and JOBJECT
                    if (curr->type == JARRAY) {
                        // free(buf);//this is the type
                        append_level(&levels, &levels_len, &level, curr);
                        curr = curr->val.array.arr[index];
                    } else if (curr->type == JOBJECT) {
                        struct hashmap_node *node = curr->val.obj->nodes;
                        for (int i = 0; i < index; i++) {
                            node = node->next;
                        }
                        exit_null(node, "get object from menu");
                        append_level(&levels, &levels_len, &level, curr);
                        curr = ((struct key_pair *)(node->val))->val;
                    } else {
                        break;
                    }
                }
                if (level < 0 || (size_t)level >= levels_len) {
                    endwin();
                    fprintf(stderr, "level:%d, levels_len:%zu\n", level, levels_len);
                    exit(EXIT_FAILURE);
                }
                for (size_t i = 0; i < levels_len; i++) {
                    // debug
                    // fprintf(stderr,"%p ",(void*)levels[i]);
                }
                // debug
                // fprintf(stderr,"\ncurr:%p\n",(void*)curr);
                redraw(wm, m, curr, &item_i, &items, &items_len, &strings, &strings_len, level);
                break;
        }
        wrefresh(wm);
    }
    err_menu(unpost_menu(m), "unpost_menu");
    err_menu(free_menu(m), "free_menu");
    free_items(items, items_len, strings, strings_len, &item_i);

    free(levels);
    free_object(j);

    free(items);
    free(strings);

    // end curses mode
    // endwin();

    return 0;
}
