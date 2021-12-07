#include <sys/mman.h> // mmap
#include <unistd.h> // fork, usleep
#include <stdio.h> // printf, perror
#include <stdlib.h> // exit
#include <sys/wait.h> // waitpid
#include <semaphore.h> // sem_open, sem_close, sem_unlink
#include <fcntl.h> // O_* constants
#include <sys/stat.h> // mode constants

#include <fstream>
#include <iostream>
#include <chrono>
#include <string>

#define SEM_PREFIX "/my_semaphore"
#define SEM_PERMS (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)

#define ITERATION 10
#define PERIOD_MS 100
#define NODES_NUM 5
#define MSG_SIZE_BYTE 10 * 1024 * 1024 // 10MiB

void write_message(int *to, int *from, unsigned int num) {
  for (int i = 0; i < num; i++) to[i] = from[i];
}

void read_message(int *to, int *from, unsigned int num) {
  for (int i = 0; i < num; i++) to[i] = from[i];
}

void child_func(int node_idx);
void start_node();
void end_node();
void middle_node(int node_idx);

pid_t pids[NODES_NUM];
int* shmems[NODES_NUM - 1];
sem_t* sems[NODES_NUM - 1];

int main() {
  for (int i = 0; i < NODES_NUM - 1; i++) {
    shmems[i] = (int *)mmap(NULL, MSG_SIZE_BYTE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (shmems[i] == MAP_FAILED) {
      perror("mmap error");
      exit(EXIT_FAILURE);
    }
  }

  for (int i = 0; i < NODES_NUM - 1; i++) {
    sems[i] = sem_open((SEM_PREFIX + std::to_string(i)).c_str(), O_CREAT | O_EXCL, SEM_PERMS, 0);
    if (sems[i] == SEM_FAILED) {
      sems[i] = sem_open((SEM_PREFIX + std::to_string(i)).c_str(), SEM_PERMS, 0);
      if (sems[i] == SEM_FAILED) {
        perror("sem_open error at main");
        exit(EXIT_FAILURE);
      }
    }
    sem_close(sems[i]); // not used by parent process
  }

  for (int i = 0; i < NODES_NUM; i++) {
    pid_t pid = fork();
    if (pid < 0) {
      perror("fork error");
      exit(EXIT_FAILURE);
    }

    if (pid == 0) {
      child_func(i);
      exit(EXIT_FAILURE);
    } else {
      pids[i] = pid;
    }
  }

  for (int i = 0; i < NODES_NUM; i++) {
    if (waitpid(pids[i], NULL, 0) < 0) {
      perror("waitpid error");
      exit(EXIT_FAILURE);
    }
  }

  for (int i = 0; i < NODES_NUM - 1; i++) {
    if (sem_unlink((SEM_PREFIX + std::to_string(i)).c_str()) < 0) {
      perror("sem_unlink error");
      exit(EXIT_FAILURE);
    }
  }

  return 0;
}

void child_func(int node_idx) {
  for (int i = 0; i < NODES_NUM - 1; i++) {
    if (i == node_idx - 1 || i == node_idx) continue;

    if (munmap(shmems[i], MSG_SIZE_BYTE) < 0) {
      perror("munmap error");
      exit(EXIT_FAILURE);
    }
  }

  if (node_idx == 0) start_node();
  else if (node_idx == NODES_NUM - 1) end_node();
  else middle_node(node_idx);
}

void start_node() {
  sem_t *sem_pub = sem_open((SEM_PREFIX + std::to_string(0)).c_str(), O_RDWR);
  if (sem_pub == SEM_FAILED) {
    perror("sem_open error at start_node");
    exit(EXIT_FAILURE);
  }

  int* data = (int *) mmap(NULL, MSG_SIZE_BYTE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  for (int i = 0; i < MSG_SIZE_BYTE / sizeof(int); i++) data[i] = 1;

  for (int i = 0; i < ITERATION; i++) {
    usleep(PERIOD_MS * 1000);
    printf("---- %d th iteration starts at start node ---- \n", i);
    write_message(shmems[0], data, MSG_SIZE_BYTE / sizeof(int));

    if (sem_post(sem_pub) < 0) {
      perror("sem_post error");
      exit(EXIT_FAILURE);
    }
  }

  if (sem_close(sem_pub) < 0) {
    perror("sem_close error");
    exit(EXIT_FAILURE);
  }
}

void middle_node(int node_idx) {
  sem_t *sem_pub = sem_open((SEM_PREFIX + std::to_string(node_idx)).c_str(), O_RDWR);
  sem_t *sem_sub = sem_open((SEM_PREFIX + std::to_string(node_idx - 1)).c_str(), O_RDWR);
  if (sem_pub == SEM_FAILED || sem_sub == SEM_FAILED) {
    perror("sem_open error at middle_node");
    exit(EXIT_FAILURE);
  }

  int* buffer = (int *)mmap(NULL, MSG_SIZE_BYTE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  for (int i = 0; i < ITERATION; i++) {
    if (sem_wait(sem_sub) < 0) {
      perror("sem_wait error");
      exit(EXIT_FAILURE);
    }

    printf("node idx %d, %d th iteration\n", node_idx, i);

    read_message(buffer, shmems[node_idx - 1], MSG_SIZE_BYTE / sizeof(int));

    /* Process Data  */
    for (int i = 0; i < MSG_SIZE_BYTE / sizeof(int); i++) buffer[i] *= 2;
    /* To here */

    write_message(shmems[node_idx], buffer, MSG_SIZE_BYTE / sizeof(int));
    if (sem_post(sem_pub) < 0) {
      perror("sem_post error");
      exit(EXIT_FAILURE);
    }
  }

  if (sem_close(sem_pub) < 0 || sem_close(sem_sub) < 0) {
    perror("sem_close error");
    exit(EXIT_FAILURE);
  }
}

void end_node() {
  sem_t *sem_sub = sem_open((SEM_PREFIX + std::to_string(NODES_NUM - 2)).c_str(), O_RDWR);
  if (sem_sub == SEM_FAILED) {
    perror("sem_open error at end_node");
    exit(EXIT_FAILURE);
  }

  int* buffer = (int *)mmap(NULL, MSG_SIZE_BYTE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  for (int i = 0; i < ITERATION; i++) {
    if (sem_wait(sem_sub) < 0) {
      perror("sem_wait error");
      exit(EXIT_FAILURE);
    }

    read_message(buffer, shmems[NODES_NUM - 2], MSG_SIZE_BYTE / sizeof(int));
    printf("%d th iteration ends at end node\n", i);
  }

  if (sem_close(sem_sub) < 0) {
    perror("sem_close error");
    exit(EXIT_FAILURE);
  }
}
