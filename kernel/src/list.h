#pragma once
#include <stddef.h>
#include <stdbool.h>
// NOTE: This implementation is a slightly modified version of that of the linux kernel itself.
// https://github.com/torvalds/linux/blob/b1bc554e009e3aeed7e4cfd2e717c7a34a98c683/tools/firewire/list.h
//
// Things like list_insert, list_append are all literally the exact same, with the exception 
// of list_remove which also closes in the array element removed since the linux version didn't clean it up
struct list {
    struct list *next, *prev;
};
static void list_init(struct list* list) {
    list->next = list;
    list->prev = list;
}
static struct list* list_last(struct list* list) {
    return list->prev != list ? list->prev : NULL;
}

static struct list* list_next(struct list* list) {
    return list->next != list ? list->next : NULL;
}
static inline void list_insert(struct list *new, struct list *link) {
    new->prev = link->prev;
    new->next = link;
    new->prev->next = new;
    new->next->prev = new;
}
static void list_append(struct list *new, struct list *into) {
    list_insert(new, into->next);
}
static void list_remove(struct list *list) {
    list->prev->next = list->next;
    list->next->prev = list->prev;
    list->next = list;
    list->prev = list;
}
static void list_move(struct list *to, struct list *what) {
    to->next = what->next;
    to->prev = what->prev;
    to->prev->next = to;
    to->next->prev = to;
    what->next = what;
    what->prev = what;
}
static bool list_empty(struct list *list) {
    return list->next == list;
}
static size_t list_len(struct list *list) {
    size_t n = 0;
    struct list* first = list;
    list = list->next;
    while(first != list) {
        n++;
        list = list->next;
    }
    return n;
}

