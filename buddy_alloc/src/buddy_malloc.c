#define _POSIX_C_SOURCE 200111L
#include <memory.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include "list.h"

// Заголовок каждого выделенного блока. Будет хранить размер.
#define HEADER_SIZE 8

// Минимальный размер -- 16 байт, 8 из которых Заголовок.
#define MIN_ALLOC_LOG2 4
#define MIN_ALLOC ((size_t)1 << MIN_ALLOC_LOG2)

// Размер кучи -- 2Гб. Максимальный размер блока памяти будет 1 Гб.
#define MAX_ALLOC_LOG2 31
#define MAX_ALLOC ((size_t)1 << MAX_ALLOC_LOG2)

// Блоки памяти выделяются размерами в степенях двойки -- от MIN_ALLOC до MAX_ALLOC
// Каждому из размеров сопоставляется бакет, в котором хранится список свободных блоков для этого размера.
// 
// Тогда зная индекс бакета, размер блока можно посчитать как:
// (size_t)1 << (MAX_ALLOC_LOG2 - bucket)
#define BUCKET_COUNT (MAX_ALLOC_LOG2 - MIN_ALLOC_LOG2 + 1)

// Массив бакетов для каждого размера блока.
static struct list_t buckets[BUCKET_COUNT];

// Как правило аллокатору дается один большой блок памяти и затем он дробится.
// Но эффективнее будет начинать наоборот с самых маленьких блоков памяти,
//   которые затем будут склеиваться при необходимости.
// bucket_limit содержит максимальный размер блока памяти. 
// bucket_limit уменьшается -- допустимый размер больше.
static size_t bucket_limit;

/*
 * Линеаризированное дерево бит.
 * Каждому выделенному блоку сопоставляется узел в этом дереве.
 *
 * Если имеем индекс текущего узла:
 * -- перейти к родителю: (i - 1) / 2
 * -- перейти к левому ребенку: (i * 2) + 1
 * -- перейти к правому ребенку: (i * 2) + 2
 * -- перейти к соседнему узлу: ((i - 1) ^ 1) + 1
 *
 * Каждый узел -- UNUSED (оба не используется) | SPLIT (один не используется) | USED (оба ребенка используются)
 * По контексту можно понять, используются ли оба ребенка, поэтому храним только признак SPLIT.
 * 
 * Не нужно хранить информацию для блоков минимального размера, поскольку они не могут быть SPLIT.
 */
static uint8_t node_status[(1 << (BUCKET_COUNT - 1)) / 8];

// Начальный адрес для аллокатора.
// Каждый выделенный адрес будет оффсетом от этого указателя (от 0 до MAX_ALLOC)
static uint8_t *base_ptr;

// Максимальный адрес, когда либо использованный аллокатором.
// Нужен чтобы понять, когда вызывать "brk" для запроса большего количества памяти.
static uint8_t *max_ptr;

#include <sys/mman.h>
// Проверить что все адресное пространство до new_value может быть использовано.
// Необходимо поскольку все 2Гб не выделяются заранее, а используются по мере необходимости
// и запрашиваются посредством вызова этой функции.
// false, если память недоступна
#include <errno.h>
#include <string.h>
static int update_max_ptr(uint8_t *new_value) {
  if (new_value > max_ptr) {
    if (sbrk(new_value - max_ptr) == (void *) -1) {
      printf("sbrk not working\n");
      printf("Error: %s\n", strerror(errno));
      return 0;
    }
    max_ptr = new_value;
  }
  return 1;
}

// Адрес для узла. Бакет можно вывести из индекса в цикле, но так эффективнее.
static uint8_t *ptr_for_node(size_t index, size_t bucket) {
  return base_ptr + ((index - (1 << bucket) + 1) << (MAX_ALLOC_LOG2 - bucket));
}

// Узел для адреса. Бакет необходим, ибо несколько узлов могут указывать на один адрес.
static size_t node_for_ptr(uint8_t *ptr, size_t bucket) {
  return ((ptr - base_ptr) >> (MAX_ALLOC_LOG2 - bucket)) + (1 << bucket) - 1;
}

static int parent_is_split(size_t index) {
  index = (index - 1) / 2;
  return (node_status[index / 8] >> (index % 8)) & 1;
}

static void flip_parent_is_split(size_t index) {
  index = (index - 1) / 2;
  node_status[index / 8] ^= 1 << (index % 8);
}

// Индекс наименьшего бакета, который вместит request байт
static size_t bucket_for_request(size_t request) {
  size_t bucket = BUCKET_COUNT - 1;
  size_t size = MIN_ALLOC;

  while (size < request) {
    bucket--;
    // size *= 2;
    size <<= 1;
  }

  return bucket;
}


// Уменьшить bucket_limit до bucket, увеличив размер максимального блока.
static int lower_bucket_limit(size_t bucket) {
  while (bucket < bucket_limit) {
    /// printf("Уменьшение bucket_limit\n");
    size_t root = node_for_ptr(base_ptr, bucket_limit);
    uint8_t *right_child;

    // Если родитель текущего корня не SPLIT, значит корень UNUSED => все пространство свободно.
    // Очистить free-список корня, уменьшить bucket_limit и добавить бакет для нового корня
    if (!parent_is_split(root)) {
      /// printf("Родитель корня не SPLIT, значит корень UNUSED\n");
      list_remove((struct list_t *)base_ptr);
      list_init(&buckets[--bucket_limit]);
      list_push(&buckets[bucket_limit], (struct list_t *)base_ptr);
      continue;
    }

    // Узел используется, родитель уже SPLIT, соседа справа надо добавить в список свободных
    right_child = ptr_for_node(root + 1, bucket_limit);
    if (!update_max_ptr(right_child + sizeof(struct list_t))) {
      return 0;
    }
    list_push(&buckets[bucket_limit], (struct list_t *)right_child);
    list_init(&buckets[--bucket_limit]);

    // Выставить у пра-родителя SPLIT, чтобы можно было затем определить, что родитель используется
    root = (root - 1) / 2;
    if (root != 0) {
      flip_parent_is_split(root);
    }
  }

  return 1;
}

void *malloc_1(size_t request) {
  // Проверка на размер request
  if (request + HEADER_SIZE > MAX_ALLOC) {
    /// printf("Размер превосходит максимальный!\n");
    return NULL;
  }

  // Инициализация, если еще ни разу не вызывали
  if (base_ptr == NULL) {
    /// printf("Инициализация аллокатора...\n");
    // base_ptr = max_ptr = mmap(NULL, 1, PROT_READ | PROT_WRITE, MAP_PRIVATE, -1, 0);
    base_ptr = max_ptr = (uint8_t *)sbrk(0);
    bucket_limit = BUCKET_COUNT - 1;
    // выделение под первый узел
    update_max_ptr(base_ptr + sizeof(struct list_t));
    // инициализация верхнего списка бакетов
    list_init(&buckets[BUCKET_COUNT - 1]);
    // начало памяти как первый элемент в списке бакетов
    list_push(&buckets[BUCKET_COUNT - 1], (struct list_t *)base_ptr);
  }

  // Индекс бакета, в который поместится request
  size_t bucket = bucket_for_request(request + HEADER_SIZE);
  /// printf("Индекс бакета -- %zu\n", bucket);
  size_t original_bucket = bucket;

  // Поиск бакета с непустым free-списком
  while (bucket + 1 != 0) {
    size_t size, bytes_needed, i;
    uint8_t *ptr;

    // Увеличить дерево до bucket, если надо
    if (!lower_bucket_limit(bucket)) {
      return NULL;
    }

    // Попытка взять уже имеющийся свободный адрес. Иначе придется сплитить блок сверху
    /// printf("Размер списка бакетов был: %d\n", list_size(&buckets[27]));
    ptr = (uint8_t *)list_pop(&buckets[bucket]);
    if (!ptr) {
      /// printf("В бакете %zu не найден свободный узел\n", bucket);
      // Если не в корне дерева или если не достигли бакет-лимита,
      // то переходим к следующему бакету
      if (bucket != bucket_limit || bucket == 0) {
        /// printf("Корень не достигнут, переход к следующему бакету\n");
        bucket--;
        continue;
      }

      /// printf("Достигнут корень, дерево необходимо увеличить\n");
      // Иначе увеличить дерево и гарантированно взять свободный блок (правый ребенок).
      if (!lower_bucket_limit(bucket - 1)) {
        return NULL;
      }
      /// printf("Размер списка бакетов стал: %d\n", list_size(&buckets[27]));
      ptr = (uint8_t *)list_pop(&buckets[bucket]);
    }

    /// printf("Размер списка бакетов -- : %d\n", list_size(&buckets[27]));
    /// printf("Получили адрес: %p!\n", ptr);

    // Увеличить адресное пространство
    size = (size_t)1 << (MAX_ALLOC_LOG2 - bucket);
    bytes_needed = (bucket < original_bucket)
                   // ???
                   ? (size / 2 + sizeof(struct list_t))
                   // ???
                   : size;
    if (!update_max_ptr(ptr + bytes_needed)) {
      // Не получилось -- вернуть блок в бакет и вернуть ошибку
      /// printf("Не получилось -- вернуть блок в бакет и вернуть ошибку\n");
      list_push(&buckets[bucket], (struct list_t *)ptr);
      return NULL;
    }

    // До этого узел был взят из списка свободных,
    // значит надо поменять статус в дереве с UNUSED на USED.
    // Для этого надо перевернуть флаг у родительского узла.
    // Пра-родитель не .
    i = node_for_ptr(ptr, bucket);
    if (i != 0) {
      flip_parent_is_split(i);
    }

    // Если узел больше, чем необходим, то происходит деление его до необходимого размера,
    // при этом новые неиспользуемые узлы добавляются в списки сводобных в своих бакетах.
    while (bucket < original_bucket) {
      // переходим к левому ребенку
      i = i * 2 + 1;
      bucket++;
      // сплитим родителя
      flip_parent_is_split(i);
      // а правого соседа в список свободных
      list_push(&buckets[bucket], (struct list_t *)ptr_for_node(i + 1, bucket));
    }

    // Адрес получен, надо записать размер блока в его заголовок
    *(size_t *)ptr = request;
    ///printf("\n");
    return ptr + HEADER_SIZE;
  }

  return NULL;
}

static void debug_buckets(void) {
  for (int i = bucket_limit; i < BUCKET_COUNT; i++) {
    /// printf("(%d - %d) ", i, list_size(&buckets[i]));
  }
  /// printf("\n");
}

void free_1(void *ptr) {
  size_t bucket, i;
  
  // NULL нельзя!
  if (!ptr) {
    return;
  }

  // malloc возвращает адрес смещенный на размер заголовка
  ptr = (uint8_t *)ptr - HEADER_SIZE;
  // *(size_t *)ptr -- заголовок, содержащий размер блока
  bucket = bucket_for_request(*(size_t *)ptr + HEADER_SIZE);
  i = node_for_ptr((uint8_t *)ptr, bucket);

  // Проход до корня дерева со сменой USED блоков на UNUSED и объединением UNUSED соседей
  while (i != 0) {
    // Узел стал UNUSED => смена флага сплита родителя
    flip_parent_is_split(i);

    // Если родитель сейчас SPLIT, то соседей объединять не надо.
    // + остановиться, если дошли до корневого узла
    if (parent_is_split(i) || bucket == bucket_limit) {
      break;
    }

    // Сосед -- UNUSED, можно объединять. Убрать соседа из списка свободных, и проходим по дереву дальше.
    list_remove((struct list_t *)ptr_for_node(((i - 1) ^ 1) + 1, bucket));
    i = (i - 1) / 2;
    bucket--;
  }

  // Добавить освобожденный (или объединенный узел) в список свободных
  // ! В начало, чтобы выделенные узлы были плотнее расположены, поскольку выделение берет узел из начала списка.
  list_push(&buckets[bucket], (struct list_t *)ptr_for_node(i, bucket));
}

// --------------

// #include <stdio.h>

/*
 * Recursively print the state of the tree from a given node.
 */
void print_tree(size_t index, size_t bucket, char *prefix) {
  uint8_t is_split = (node_status[index / 8] >> (index % 8)) & 1;
  uint8_t *ptr = ptr_for_node(index, bucket);
  uint8_t is_free = list_contains(&buckets[bucket], (struct list_t*) ptr);
  size_t size = (size_t)1 << (MAX_ALLOC_LOG2 - bucket); // Block size

  // Output node information
  if (bucket == BUCKET_COUNT) {
    printf("  Node[%zu]: %hhu %hhu, Size: %zu bytes\n", index, is_split, is_free, size);
  } else {
    printf("Node[%zu]: %hhu %hhu, Size: %zu bytes\n", index, is_split, is_free, size);
    // Recursively print children if the node is split
    if (bucket > 0) {
      print_tree(index * 2 + 1, bucket + 1, prefix); // Left child
      print_tree(index * 2 + 2, bucket + 1, prefix); // Right child
    }
  }
}

/*
 * Entry point for printing the allocation tree.
 */
void buddy_debug_print_tree(void) {
  debug_buckets();
    // size_t root = node_for_ptr(base_ptr, bucket_limit);
    printf("Buddy Allocator Tree:\n");
    print_tree(0, bucket_limit, "--"); // Start from the root node
}

