#include "list.h"

#include <stdlib.h>
#include <string.h>

list_t* list_create(void) {
    list_t* list = malloc(sizeof(list_t));

    list_init(list);

    return list;
}

void list_init(list_t* list) {
    list->first = NULL;
    list->last = NULL;
    list->size = 0;
}

void list_push_front(list_t* list, void* data) {
    node_t* node = malloc(sizeof(node_t));

    node->data = data;
    node->next = list->first;

    list->first = node;
    
    if (!list->last)
        list->last = list->first;

    ++list->size;
}

void list_push_back(list_t* list, void* data) {
    node_t* node = malloc(sizeof(node_t));

    node->data = data;
    node->next = NULL;

    if (!list->last) {
        list->first = node;
        list->last = node;
    } else {
        list->last->next = node;
        list->last = node;
    }

    ++list->size;
}

void list_pop_front(list_t* list) {
    if (!list->first)
        return;

    node_t* next = list->first->next;

    free(list->first);

    list->first = next;

    --list->size;
}

void list_pop_back(list_t* list) {
    if (!list->last)
        return;

    node_t* node = list->first;

    while (node->next != list->last)
        node = node->next;

    free(node->next);

    node->next = NULL;

    list->last = node;
}

node_t* list_front(list_t* list) {
    return list->first;
}

node_t* list_back(list_t* list) {
    return list->last;
}

node_t* list_at(list_t* list, int index) {
    if (index > list->size)
        return NULL;

    node_t* node = list->first;

    for (int i = 0; i < index; i++)
        node = node->next;

    return node;
}

void list_iterate(list_t* list, void (*func)(void*)) {
    node_t* node = list->first;

    while (node) {
        func(node->data);

        node = node->next;
    }
}

void list_destroy(list_t* list) {
    node_t* node = list->first;

    while (node) {
        node_t* next = node->next;

        free(node);

        node = next;
    }

    free(list);
}