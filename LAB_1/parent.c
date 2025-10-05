#include "general.h"

int main() {
  char name_file[100];
  int pipefd[2];
  pid_t child_pid;
  int file_fd;
  char buffer[1024];
  int child_status = 0;

  printf("Введите имя файла с командами (родительский процесс): ");
  scanf("%s", name_file);

  file_fd = open(name_file, O_RDONLY);
  CHECK_ERROR(file_fd == -1, "Ошибка открытия файла")

  CHECK_ERROR(pipe(pipefd) == -1, "Ошибка создания pipe")

  child_pid = fork();
  CHECK_ERROR(child_pid == -1, "Ошибка создания процесса")

  if (child_pid == 0) {
    close(pipefd[0]);

    CHECK_ERROR(dup2(file_fd, STDIN_FILENO) == -1,
                "Ошибка перенаправления ввода")

    CHECK_ERROR(dup2(pipefd[1], STDOUT_FILENO) == -1,
                "Ошибка перенаправления вывода")

    close(file_fd);
    close(pipefd[1]);

    execl("./child", "child", (char *)NULL);

    perror("Ошибка запуска дочерней программы");
    exit(EXIT_FAILURE);

  } else {
    printf("Родительский процесс создал дочерний процесс\n");

    close(file_fd);
    close(pipefd[1]);

    printf("Результаты вычислений: \n");
    ssize_t bytes_read;
    while ((bytes_read = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
      buffer[bytes_read] = '\0';
      printf("%s", buffer);
      fflush(stdout);
    }

    int status;
    waitpid(child_pid, &status, 0);

    if (WIFEXITED(status)) {
      child_status = WEXITSTATUS(status);
      if (child_status == 0) {
        printf("Дочерний процесс завершился успешно (родительский процесс)\n");
      } else {
        printf("Дочерний процесс завершился с кодом ошибки %d (родительский "
               "процесс)\n",
               child_status);
        close(pipefd[0]);
        exit(EXIT_FAILURE);
      }
    }
    close(pipefd[0]);
    printf("Работа родительского процесса завершена\n");
  }
  return 0;
}