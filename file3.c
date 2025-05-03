#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

volatile sig_atomic_t flag1 = 0;
volatile sig_atomic_t flag2 = 0;
int pipe_fd[2];

// Обработчик сигнала 
void sig_handler(int signo) {
    printf("\nget SIGINT; %d\n", signo);
    flag1 = 1;  
    flag2 = 1;  
    close(pipe_fd[0]);  
    close(pipe_fd[1]);  
    exit(0);  
}

void* func1(void* arg) {
    char buffer[256];
    while (!flag1) {
        long host_id = gethostid();
        snprintf(buffer, sizeof(buffer), "Host ID: %ld", host_id);
        ssize_t bytes_written = write(pipe_fd[1], buffer, strlen(buffer) + 1);
        if (bytes_written == -1) {
            perror("write");
        }
        sleep(1);
    }
    return NULL;
}

void* func2(void* arg) {
    char buffer[256];
    while (!flag2) {
        memset(buffer, 0, sizeof(buffer));
        ssize_t bytes_read = read(pipe_fd[0], buffer, sizeof(buffer));
        if (bytes_read == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(100000);  
            } else {
                perror("read");  // Другие ошибки выводим
            }
        } else if (bytes_read > 0) {
            printf("Received: %s\n", buffer);
        }
    }
    return NULL;
}

int main(int argc, char* argv[]) {

    int mode = atoi(argv[1]);
    if (mode < 1 || mode > 3) {
        fprintf(stderr, "Неверный режим. Выберите 1, 2, 3.\n");
        exit(EXIT_FAILURE);
    }
    switch (mode) {
        case 1:
            printf("Режим 1: pipe с блокировкой\n");
            if (pipe(pipe_fd) == -1) {
                perror("pipe");
                exit(EXIT_FAILURE);
            }
            break;
        case 2:
            printf("Режим 2: pipe2 без блокировок\n");
            if (pipe2(pipe_fd, O_NONBLOCK) == -1) {
                perror("pipe2");
                exit(EXIT_FAILURE);
            }
            break;
        case 3:
            printf("Режим 3: pipe + fcntl без блокировок\n");
            if (pipe(pipe_fd) == -1){
                perror("pipe");
                exit(EXIT_FAILURE);
            }
            if (fcntl(pipe_fd[0], F_SETFL, O_NONBLOCK) == -1 || fcntl(pipe_fd[1], F_SETFL, O_NONBLOCK) == -1){
                perror("fcntl");
                exit(EXIT_FAILURE);
            }
            break;
    }


    pthread_t thread1, thread2;
    if (pthread_create(&thread1, NULL, func1, NULL) != 0) {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }
    if (pthread_create(&thread2, NULL, func2, NULL) != 0) {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    // Установка обработчика сигнала SIGINT
    if (signal(SIGINT, sig_handler) == SIG_ERR) {
        perror("signal");
        exit(EXIT_FAILURE);
    }

    printf("Нажмите Ctrl+c для завершения...\n");
    getchar();

    flag1 = 1;
    flag2 = 1;

    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    close(pipe_fd[0]);
    close(pipe_fd[1]);

    return 0;
}
