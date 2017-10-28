#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dllist.h"


struct dllist *dllist_create()
{
    struct dllist *list = (struct dllist *)malloc(sizeof(struct dllist));
    if (! list) {
        perror("create dllist");
        exit(1);
    }
    memset(list, 0, sizeof(struct dllist));
    return list;
}

void *dllist_new(struct dllist *list, void *item, size_t size, int prepend)
{
    struct dll_entry *new = malloc(size + sizeof(struct dll_entry));
    if (item) {
        memcpy((void *)new->item, item, size);
    }
    if (prepend) {
        new->prev = NULL;
        new->next = list->head;
    } else {
        new->next = NULL;
        new->prev = list->tail;
    }
    if (list->size == 0) {
        list->head = list->tail = new;
    } else if (prepend) {
        list->head->prev = new;
        list->head = new;
    } else {
        list->tail->next = new;
        list->tail = new;
    }
    list->size++;
    return (void *)new->item;
}

void dllist_remove(struct dllist *list, void *item)
{
    struct dll_entry *entry = dllist_entry(item);
    if (list->head == entry) {
        list->head = entry->next;
    }
    if (list->tail == entry) {
        list->tail = entry->prev;
    }
    if (entry->next) {
        entry->next->prev = entry->prev;
    }
    if (entry->prev) {
        entry->prev->next = entry->next;
    }
    list->size--;
    free(entry);
}

void dllist_clear(struct dllist *list)
{
    while (list->head) {
        dllist_remove(list, (void *)list->head->item);
    }
}

void dllist_destroy(struct dllist *list)
{
    dllist_clear(list);
    free(list);
}
