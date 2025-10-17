#include <getopt.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define THREAD_TYPE int
#define THREAD_CREATE(thread, attr, func, arg) 0
#define THREAD_JOIN(thread, retval) 0
#else
#include <pthread.h>
#define THREAD_TYPE pthread_t
#define THREAD_CREATE(thread, attr, func, arg)                                 \
  pthread_create(thread, attr, func, arg)
#define THREAD_JOIN(thread, retval) pthread_join(thread, retval)
#endif

typedef struct {
  uint64_t parts[8];
} BigInt;

typedef struct {
  BigInt *arr;
  size_t start, end;
  BigInt partial_sum;
} ThreadData;

void add_bigint_inplace(BigInt *a, const BigInt *b) {
  uint64_t transfer_between_digits = 0;
  for (int i = 7; i >= 0; i--) {
    uint64_t sum = a->parts[i] + b->parts[i] + transfer_between_digits;
    transfer_between_digits =
        (sum < a->parts[i]) || (transfer_between_digits && sum == a->parts[i]);
    a->parts[i] = sum;
  }
}

int read_512hex(FILE *f, BigInt *out) {
  char buf[130];
  if (!fgets(buf, sizeof(buf), f))
    return 0;
  buf[strcspn(buf, "\n")] = 0;

  char clean[129] = {0};
  int pos = 0;
  for (int i = 0; buf[i] && pos < 128; i++) {
    if ((buf[i] >= '0' && buf[i] <= '9') || (buf[i] >= 'A' && buf[i] <= 'F') ||
        (buf[i] >= 'a' && buf[i] <= 'f')) {
      clean[pos++] = buf[i];
    }
  }

  if (pos < 128) {
    memmove(clean + (128 - pos), clean, pos);
    memset(clean, '0', 128 - pos);
  }

  for (int i = 0; i < 8; i++) {
    char part[17] = {0};
    memcpy(part, clean + i * 16, 16);
    char *endptr;
    out->parts[i] = strtoull(part, &endptr, 16);
    if (endptr == part) {
      return -1;
    }
  }
  return 1;
}

int divide_bigint_round(const BigInt *dividend, uint64_t divisor,
                        BigInt *quotient) {
  if (divisor == 0) {
    fprintf(stderr, "Деление на ноль невозможно\n");
    return -1;
  }

  uint64_t remainder = 0;
  BigInt temp_quotient = {0};

  for (int i = 0; i < 8; i++) {
    __uint128_t temp = ((__uint128_t)remainder << 64) | dividend->parts[i];
    __uint128_t div_result = temp / divisor;
    remainder = temp % divisor;
    temp_quotient.parts[i] = (uint64_t)div_result;
  }

  if (remainder >= (divisor + 1) / 2) {
    BigInt one = {0};
    one.parts[7] = 1;
    add_bigint_inplace(&temp_quotient, &one);
  }
  *quotient = temp_quotient;
  return 0;
}

void *thread_func(void *arg) {
  ThreadData *td = (ThreadData *)arg;
  BigInt zero = {0};
  td->partial_sum = zero;

  for (size_t i = td->start; i < td->end; ++i) {
    add_bigint_inplace(&td->partial_sum, &td->arr[i]);
  }

  return NULL;
}

void print_bigint(const BigInt *num) {
  for (int i = 0; i < 8; i++) {
    printf("%016lX", num->parts[i]);
  }
  printf("\n");
}

int main(int argc, char **argv) {
  BigInt *arr = NULL;
  THREAD_TYPE *threads = NULL;
  ThreadData *thread_data = NULL;
  FILE *f = NULL;

  int max_threads = 1;
  long memory_mb = 100;
  char *file = NULL;

  int opt;

  while ((opt = getopt(argc, argv, "t:m:f:")) != -1) {
    switch (opt) {
    case 't':
      max_threads = atoi(optarg);
      if (max_threads <= 0) {
        fprintf(stderr, "Количество потоков должно быть > 0\n");
        return 1;
      }
      break;
    case 'm':
      memory_mb = atol(optarg);
      if (memory_mb <= 0) {
        fprintf(stderr, "Лимит памяти должен быть > 0\n");
        return 1;
      }
      break;
    case 'f':
      file = optarg;
      break;
    default:
      fprintf(stderr,
              "Использование: %s -t <количесвто потоков> -m <количество "
              "памяти> -f <имя файла>\n",
              argv[0]);
      return 1;
    }
  }

  if (!file) {
    fprintf(stderr, "Не указан файл с данными\n");
    return 1;
  }

  f = fopen(file, "r");
  if (!f) {
    perror("Ошибка открытия файла");
    goto cleanup;
  }

  size_t max_numbers = ((memory_mb - 10) * 1024 * 1024) / sizeof(BigInt);
  if (max_numbers < 10) {
    fprintf(stderr, "Недостаточно памяти, нужно увеличить лимит\n");
    goto cleanup;
  }

  arr = malloc(max_numbers * sizeof(BigInt));
  if (!arr) {
    perror("Ошибка выделения памяти для чисел");
    goto cleanup;
  }

  size_t total = 0;
  int r;
  while ((r = read_512hex(f, &arr[total])) > 0) {
    total++;
    if (total >= max_numbers) {
      printf("Достигнут лимит памяти %ld MB\n", memory_mb);
      break;
    }
  }

  if (r < 0) {
    fprintf(stderr, "Ошибка формата в файле\n");
    goto cleanup;
  }

  if (total == 0) {
    fprintf(stderr, "Нет чисел для обработки\n");
    goto cleanup;
  }

  printf("Загружено чисел: %zu\n", total);

  int threads_n = (total < (size_t)max_threads) ? (int)total : max_threads;
  threads = malloc(threads_n * sizeof(THREAD_TYPE));
  thread_data = malloc(threads_n * sizeof(ThreadData));

  if (!threads || !thread_data) {
    perror("Ошибка выделения памяти для потоков");
    goto cleanup;
  }

  size_t base = total / threads_n;
  size_t remains = total % threads_n;
  size_t current_position = 0;

  printf("Создается потоков: %d\n", threads_n);
  for (int i = 0; i < threads_n; ++i) {
    size_t block = base + (i < (int)remains);
    thread_data[i].arr = arr;
    thread_data[i].start = current_position;
    thread_data[i].end = current_position + block;
    printf("Поток %d: элементы %zu-%zu (%zu элементов)\n", i,
           thread_data[i].start, thread_data[i].end - 1, block);
    current_position += block;

    if (THREAD_CREATE(&threads[i], NULL, thread_func, &thread_data[i]) != 0) {
      perror("Ошибка создания потока");
      goto cleanup;
    }
  }

  BigInt total_sum = {0};
  size_t count = 0;
  for (int i = 0; i < threads_n; ++i) {
    THREAD_JOIN(threads[i], NULL);
    BigInt temp = total_sum;
    add_bigint_inplace(&temp, &thread_data[i].partial_sum);
    total_sum = temp;
    count += thread_data[i].end - thread_data[i].start;
  }

  if (count == 0) {
    fprintf(stderr, "Нет чисел для подсчёта среднего\n");
    goto cleanup;
  }
  if (count > UINT64_MAX) {
    fprintf(stderr, "Слишком много чисел для деления\n");
    goto cleanup;
  }

  BigInt average = {0};
  if (divide_bigint_round(&total_sum, (uint64_t)count, &average) != 0) {
    goto cleanup;
  }

  printf("\n \n");
  printf("Обработано чисел: %zu\n", count);
  printf("Среднее арифметическое с учётом округления: ");
  print_bigint(&average);

  int result = 0;
  goto success;

cleanup:
  result = 1;

success:
  if (f)
    fclose(f);
  if (arr)
    free(arr);
  if (threads)
    free(threads);
  if (thread_data)
    free(thread_data);

  return result;
}