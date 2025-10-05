#include "general.h"

int main() {
  float numbers[100];
  char buffer[1024];
  int count_num;
  float result;
  int error = 0;

  while (fgets(buffer, sizeof(buffer), stdin) != NULL && !error) {
    buffer[strcspn(buffer, "\n")] = 0;

    count_num = 0;
    char *token = strtok(buffer, " ");

    while (token != NULL && count_num < 100) {
      numbers[count_num] = atof(token);
      count_num++;
      token = strtok(NULL, " ");
    }

    if (count_num < 2) {
      printf("Недостаточно чисел в строке\n");
      fflush(stdout);
      continue;
    }

    result = numbers[0];

    for (int i = 1; i < count_num; i++) {
      if (numbers[i] == 0.0f) {
        printf("Ошибка: деление на ноль\n");
        fflush(stdout);
        error = 1;
        break;
      }
      result /= numbers[i];
    }

    if (!error) {
      printf("%.2f\n", result);
      fflush(stdout);
    }
  }
  return error ? EXIT_FAILURE : EXIT_SUCCESS;
}