#ifndef LIST_H
#define LIST_H

/*
 * Цикличный двусвязный список
 */
struct list_t {
  struct list_t *prev, *next;
};


/*
 * Пустой список
 */
void list_init(struct list_t *list);
int list_size(struct list_t *list);

/*
 * Добавить в конец
 */
void list_push(struct list_t *beginning, struct list_t *new_last);

/*
 * Удалить элемент из списка
 */
void list_remove(struct list_t *entry);

/*
 * Удалить и вернуть элемент
 * NULL, если список "пустой"
 */
struct list_t *list_pop(struct list_t *list);

uint8_t list_contains(struct list_t *list, struct list_t *this);

#endif
