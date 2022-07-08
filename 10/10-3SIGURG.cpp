#include <csignal>
#include <strings.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <signal.h>
#include <pthread.h>

#define BUFFER_SIZE 1024

static int connfd;

// SIGURG处理函数
void sig_urg(int sig) {
  int save_errno = errno;
  char buf[BUFFER_SIZE];
  memset(buf, '\0', BUFFER_SIZE);
  int ret = recv(connfd, buf, BUFFER_SIZE-1, MSG_OOB);
  printf("got %d bytes of oob data: %s\n", ret, buf);
  errno = save_errno;
}

void addsig(int sig, void (*sig_handler)(int)) {
  struct sigaction sa;
  memset(&sa, '\0', sizeof(sa));
  sa.sa_handler = sig_handler;
  sa.sa_flags |= SA_RESTART;
  sigfillset(&sa.sa_mask);
  assert(sigaction(sig, &sa, NULL) != -1);
}

int main(int argc, char **argv) {
  if(argc <= 2) {
    printf("Usage: %s ip_address port_number", basename(argv[0]));
    return 1;
  }
  const char *ip = argv[1];
  int port = atoi(argv[2]);

  struct sockaddr_in address;
  bzero(&address, sizeof(address));
  address.sin_family = AF_INET;
  inet_pton(AF_INET, ip, &address.sin_addr);
  address.sin_port = htons(port);

  int listenfd = socket(PF_INET, SOCK_STREAM, 0);
  assert(listenfd >= 0);

  int ret = bind(listenfd, (struct sockaddr*)&address, sizeof(address));
  assert(ret != -1);

  ret = listen(listenfd, 5);
  assert(ret != -1);

  struct sockaddr_in client;
  socklen_t client_addrlength = sizeof(client);
  connfd = accept(listenfd, (struct sockaddr*)&client, &client_addrlength);
  if(connfd < 0) {
    printf("errno is: %d\n", errno);
  } else {
    addsig(SIGURG, sig_urg);
    // 使用SIGURG前，设置socket的宿主进程或进程组
    fcntl(connfd, F_SETOWN, getpid());
    char buffer[BUFFER_SIZE];
    while(true) { // 循环接收普通数据
      memset(buffer, '\0', BUFFER_SIZE);
      if(ret <= 0) {
        break;
      }
      printf("got %d bytes of normal data: %s\n", ret, buffer);
    }
    close(connfd);
  }
  close(listenfd);
  return 0;
}
