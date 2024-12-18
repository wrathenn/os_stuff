#include <stdlib.h>
#include "list.h"

/*
* Пустой список
 */
void list_init(struct list_t *list) {
  list->prev = list;
  list->next = list;
}

static int _list_size(struct list_t *list, struct list_t *begin, int count) {
  if (list != begin) {
    return _list_size(list->prev, begin, count + 1);
  } else {
    return count;
  }
}

int list_size(struct list_t *list) {
  return _list_size(list->prev, list, 0);
}

/*
 * Добавить в конец
 */
void list_push(struct list_t *beginning, struct list_t *new_last) {
  struct list_t *prev_last = beginning->prev;
  new_last->prev = prev_last;
  new_last->next = beginning;
  prev_last->next = new_last;
  beginning->prev = new_last;
}

/*
 * Удалить элемент из списка
 */
void list_remove(struct list_t *entry) {
  struct list_t *next = entry->next;
  struct list_t *prev = entry->prev;
  prev->next = next;
  next->prev = prev;
}

/*
 * Удалить и вернуть элемент
 * NULL, если список "пустой"
 */
struct list_t *list_pop(struct list_t *list) {
  struct list_t *back = list->prev;
  if (back == list) return NULL;
  list_remove(back);
  return back;
}

static uint8_t _list_contains(struct list_t *list, struct list_t *this, struct list_t *begin) {
  if (list == this) {
    return 1;
  }
  if (list == begin) {
    return 0;
  }
  return _list_contains(list->next, this, list);
}

uint8_t list_contains(struct list_t *list, struct list_t *this) {
  return _list_contains(list, this, list);
}
