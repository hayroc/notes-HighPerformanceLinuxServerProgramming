#include <cstdlib>
#include <pthread.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <wait.h>

pthread_mutex_t mutex;

void *another(void *arg) {
  printf("in child thread, lock the mutex\n");
  pthread_mutex_lock(&mutex);
  sleep(5);
  pthread_mutex_unlock(&mutex);
}

void prepare() {
  pthread_mutex_lock(&mutex);
}

void infork() {
  pthread_mutex_unlock(&mutex);
}

int main() {
  pthread_mutex_init(&mutex, NULL);
  pthread_t id;
  pthread_create(&id, NULL, another, NULL);
  sleep(1);
  // 加入下一行代码后正常
  // pthread_atfork(prepare, infork, infork);
  int pid = fork();
  if(pid < 0) {
    pthread_join(id, NULL);
    pthread_mutex_destroy(&mutex);
    return 1;
  } else if(pid == 0) {
    printf("in chidl, want to get the lock\n");
    // 子进程继承了父进程的状态
    // 下面加锁会一直阻塞
    pthread_mutex_lock(&mutex);
    printf("i can not run to here, oop..\n");
    pthread_mutex_unlock(&mutex);
    exit(0);
  } else {
    wait(NULL);
  }
  pthread_join(id, NULL);
  pthread_mutex_destroy(&mutex);
  return 0;
}
