#define _POSIX_C_SOURCE 200111L
#include <memory.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#include "buddy_malloc_2.h"
#include "list.h"

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

// Как правило, аллокатору дается один большой блок памяти и затем он дробится.
// Но эффективнее будет начинать наоборот с самых маленьких блоков памяти,
//   которые затем будут склеиваться при необходимости.
// bucket_limit содержит максимальный размер блока памяти. 
// bucket_limit уменьшается -- допустимый размер больше.
static size_t bucket_limit;

enum status {
  UNUSED = 0b00,
  SPLIT = 0b01,
  USED = 0b10,
};

/*
 * Линеаризированное дерево по 2 бита.
 * Каждому выделенному блоку сопоставляется узел в этом дереве.
 *
 * Если имеем индекс текущего узла:
 * -- перейти к родителю: (i - 1) / 2
 * -- перейти к левому ребенку: (i * 2) + 1
 * -- перейти к правому ребенку: (i * 2) + 2
 * -- перейти к соседнему узлу: ((i - 1) ^ 1) + 1
 *
 * Каждый узел -- UNUSED (оба не используется)
 *              | SPLIT (один не используется)
 *              | USED (оба ребенка используются)
 */
static uint8_t node_status[(1 << BUCKET_COUNT) / 4];

// Начальный адрес для аллокатора.
// Каждый выделенный адрес будет оффсетом от этого указателя (от 0 до MAX_ALLOC)
static uint8_t *base_ptr;

// Максимальный адрес, когда-либо использованный аллокатором.
// Нужен, чтобы понять, когда вызывать "brk" для запроса большего количества памяти.
static uint8_t *max_ptr;

#include <sys/mman.h>
// Проверить что все адресное пространство до new_value может быть использовано.
// Необходимо, поскольку все 2Гб не выделяются заранее, а используются по мере необходимости
// и запрашиваются посредством вызова этой функции.
// false, если память недоступна
#include <errno.h>
#include <string.h>
static int update_max_ptr(uint8_t *new_value) {
  if (new_value > max_ptr) {
    if (sbrk(new_value - max_ptr) == (void *)-1) {
      printf("sbrk not working\n");
      printf("Error: %s\n", strerror(errno));
      return 0;
    }
    max_ptr = new_value;
  }
  return 1;
}

// Адрес для узла. Бакет можно вывести из индекса в цикле, но так эффективнее.
static uint8_t *ptr_for_node(const size_t index, const size_t bucket) {
  return base_ptr + ((index - (1 << bucket) + 1) << (MAX_ALLOC_LOG2 - bucket));
}

// Узел для адреса. Бакет необходим, ибо несколько узлов могут указывать на один адрес.
static size_t node_for_ptr(const uint8_t *ptr, const size_t bucket) {
  return ((ptr - base_ptr) >> (MAX_ALLOC_LOG2 - bucket)) + (1 << bucket) - 1;
}

static enum status get_node_status(const size_t index) {
  const uint8_t bit_move = (3 - index % 4) * 2;
  return (node_status[index / 4] >> bit_move) & 0b11;
}

static void parent_inc_status(size_t index) {
  index = (index - 1) / 2;
  const uint8_t bit_move = (3 - index % 4) * 2;
  node_status[index / 4] += 1 << bit_move;
}

static void parent_dec_status(size_t index) {
  index = (index - 1) / 2;
  const uint8_t bit_move = (3 - index % 4) * 2;
  node_status[index / 4] -= 1 << bit_move;
}

static void node_set_status(const size_t index, const enum status st) {
  const uint8_t bit_move = (3 - index % 4) * 2;
  const uint8_t value = st << bit_move;
  const uint8_t mask = 0b11111111 ^ (0b00000011 << bit_move);

  node_status[index / 4] = (node_status[index / 4] & mask) | value;
}

static size_t bucket_for_alloced_pointer(const void *ptr) {
  for (int i = BUCKET_COUNT - 1; i >= bucket_limit; i--) {
    const size_t node_i = node_for_ptr(ptr, i);
    const enum status node_status = get_node_status(node_i);
    if (node_status == USED) {
      return i;
    } else if (node_status == SPLIT) {
      printf("???");
    }
  }

  return -1;
}

// Индекс наименьшего бакета, который вместит request байт
static size_t bucket_for_request(const size_t request) {
  size_t bucket = BUCKET_COUNT - 1;
  size_t size = MIN_ALLOC;

  while (size < request) {
    bucket--;
    size <<= 1;
  }

  return bucket;
}


// Уменьшить bucket_limit до bucket, увеличив размер максимального блока.
static int lower_bucket_limit(const size_t bucket) {
  while (bucket < bucket_limit) { // todo is it <= now?
    const size_t root = node_for_ptr(base_ptr, bucket_limit);

    // Если корень UNUSED, все пространство свободно.
    const enum status root_status = get_node_status(root);
    if (root_status == UNUSED) {
      // Очистить free-список корня, уменьшить bucket_limit и добавить бакет для нового корня
      list_remove((struct list_t *)base_ptr);
      list_init(&buckets[--bucket_limit]);
      list_push(&buckets[bucket_limit], (struct list_t *)base_ptr);
      continue;
    }

    // Узел используется, соседа справа при расширении
    // надо будет добавить в список свободных
    uint8_t *right_child = ptr_for_node(root + 1, bucket_limit);
    if (!update_max_ptr(right_child + sizeof(struct list_t))) {
      return 0;
    }
    node_set_status((root - 1) / 2, SPLIT);
    // parent_inc_status(root);
    list_push(&buckets[bucket_limit], (struct list_t *)right_child);
    list_init(&buckets[--bucket_limit]);
  }

  return 1;
}

void *malloc_2(size_t request) {
  // Проверка на размер request
  if (request > MAX_ALLOC) {
    /// printf("Размер превосходит максимальный!\n");
    return NULL;
  }

  // Инициализация, если еще ни разу не вызывали
  if (base_ptr == NULL) {
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
  size_t bucket = bucket_for_request(request);
  /// printf("Индекс бакета -- %zu\n", bucket);
  const size_t original_bucket = bucket;

  // Поиск бакета с непустым free-списком
  while (bucket != -1) {

    // Увеличить дерево до bucket, если надо
    if (!lower_bucket_limit(bucket)) {
      return NULL;
    }

    // Попытка взять уже имеющийся свободный адрес. Иначе придется сплитить блок сверху
    uint8_t *ptr = (uint8_t *)list_pop(&buckets[bucket]);
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
      // Иначе увеличить дерево и гарантированно взять свободный блок (правый
      // ребенок).
      if (!lower_bucket_limit(bucket - 1)) {
        return NULL;
      }
      ptr = (uint8_t *)list_pop(&buckets[bucket]);
      // printf("ptr = %p\n", ptr);
    }

    /// printf("Размер списка бакетов -- : %d\n", list_size(&buckets[27]));
    /// printf("Получили адрес: %p!\n", ptr);

    // Увеличить адресное пространство
    const size_t size = (size_t)1 << (MAX_ALLOC_LOG2 - bucket);
    const size_t bytes_needed = (bucket < original_bucket)
                   ? (size / 2 + sizeof(struct list_t))
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
    size_t i = node_for_ptr(ptr, bucket);
    // size_t old_i = i;
    if (i != 0) {
      // buddy_debug_print_tree_2();
      parent_inc_status(i);
      // buddy_debug_print_tree_2();
    }


    // Если узел больше, чем необходим, то происходит деление его до необходимого размера,
    // при этом новые неиспользуемые узлы добавляются в списки свободных в своих бакетах.
    while (bucket < original_bucket) {
      // переходим к левому ребенку
      i = i * 2 + 1;
      bucket++;
      // сплитим родителя
      parent_inc_status(i);
      // а правого соседа в список свободных
      list_push(&buckets[bucket], (struct list_t *)ptr_for_node(i + 1, bucket));
    }

    node_set_status(i, USED);

    size_t parent_i = (i - 1) / 2;
    while (parent_i != 0) {
      const size_t left_child_i = parent_i * 2 + 1;
      const size_t right_child_i = parent_i * 2 + 2;
      const enum status left_node_status = get_node_status(left_child_i);
      const enum status right_node_status = get_node_status(right_child_i);

      if (left_node_status == USED && right_node_status == USED) {
        node_set_status(parent_i, USED);
        parent_i = (parent_i - 1) / 2;
      } else break;
    }

    return ptr;
  }

  return NULL;
}

void free_2(void *ptr) {
  // NULL нельзя!
  if (!ptr) {
    return;
  }

  // malloc возвращает адрес, смещенный на размер заголовка
  ptr = (uint8_t *)ptr;
  // *(size_t *)ptr -- заголовок, содержащий размер блока
  size_t bucket = bucket_for_alloced_pointer(ptr);
  size_t i = node_for_ptr(ptr, bucket);

  // Проход до корня дерева со сменой USED блоков на UNUSED и объединением UNUSED соседей
  while (i != 0) {
    // Узел стал UNUSED, флаг родителя может поменяться на следующей итерации
    node_set_status(i, UNUSED);

    const size_t parent_i = (i - 1) / 2;
    const size_t left_child_i = parent_i * 2 + 1;
    const size_t other_child_i = (i == left_child_i) ? parent_i * 2 + 2 : left_child_i;

    const enum status buddy_status = get_node_status(other_child_i);
    if (buddy_status != UNUSED || bucket == bucket_limit) {
      parent_dec_status(i);
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

#define PREFIX_SIZE 6
#define PREFIX_MID "├─"
#define PREFIX_LAST "└─"
#define PREFIX_FALL "│ \b "
#define PREFIX_EMPTY " \b  \b "

/*
 * Создание нового префикса
 */
static char *_prefix_create(const char *old_prefix) {
  int add_size = PREFIX_SIZE * 2;
  const int next_prefix_len = strlen(old_prefix) + PREFIX_SIZE;
  // 1 под "\0"
  char *next_prefix = calloc(next_prefix_len + 1, sizeof(char));

  strcpy(next_prefix, old_prefix);
  // Заменить предыдущую часть префикса
  if (next_prefix_len >= add_size) {
    if (strncmp(next_prefix + next_prefix_len - add_size, PREFIX_LAST, PREFIX_SIZE) == 0) {
      strcpy(next_prefix + next_prefix_len - add_size,  PREFIX_EMPTY);
    } else {
      strcpy(next_prefix + next_prefix_len - add_size, PREFIX_FALL);
    }
  }
  // Вставить новую часть префикса
  strcpy(next_prefix + next_prefix_len - PREFIX_SIZE, PREFIX_MID);
  return next_prefix;
}

/*
 * Если элемент был последним, заменить начало префикса на пустое
 */
static void _prefix_cut_if_ended(char *prefix) {
  const int prefix_len = strlen(prefix);
  strcpy(prefix + prefix_len - PREFIX_SIZE, PREFIX_LAST);
}

static void debug_tree_rec(const size_t index, const size_t bucket, char *prefix) {
  if (bucket == BUCKET_COUNT) {
    return;
  }
  const enum status my_status = get_node_status(index);
  const size_t size = 1 << (MAX_ALLOC_LOG2 - bucket);

  if (my_status == UNUSED) {
    printf("%s", prefix);
    printf("UNUSED [%zu b]\n", size);
    return;
  }

  const size_t left_node_i = index * 2 + 1;
  const enum status left_node_status = get_node_status(left_node_i);
  const size_t right_node_i = index * 2 + 2;
  const enum status right_node_status = get_node_status(right_node_i);

  if (my_status == USED) {
    printf("%s", prefix);
    printf("USED [%zu b]\n", size);

    if (left_node_status == UNUSED && right_node_status == UNUSED) {
      return;
    }
  }

  if (my_status == SPLIT) {
    printf("%s", prefix);
    printf("SPLIT [%zu b]\n", size);
  }

  char *next_prefix = _prefix_create(prefix);
  debug_tree_rec(right_node_i, bucket + 1, next_prefix);
  _prefix_cut_if_ended(next_prefix);
  debug_tree_rec(left_node_i, bucket + 1, next_prefix);
  free(next_prefix);
}

void buddy_debug_print_tree_2(void) {
  const size_t root_node_i = node_for_ptr(base_ptr, bucket_limit);
  char *prefix = "";
  debug_tree_rec(root_node_i, bucket_limit, prefix);
}
