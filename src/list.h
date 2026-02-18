#ifndef LIST_H
#define LIST_H

#include <stddef.h>

typedef struct node_t node_t;

typedef struct node_t {
    node_t* next;
    void* data;
} node_t;

typedef struct {
    node_t* first;
    node_t* last;
    size_t size;
} list_t;

list_t* list_create(void);
void list_init(list_t* list);
void list_push_front(list_t* list, void* data);
void list_push_back(list_t* list, void* data);
void list_pop_front(list_t* list);
void list_pop_back(list_t* list);
node_t* list_front(list_t* list);
node_t* list_back(list_t* list);
node_t* list_at(list_t* list, int index);
void list_iterate(list_t* list, void (*func)(void*));
void list_destroy(list_t* list);

#endif