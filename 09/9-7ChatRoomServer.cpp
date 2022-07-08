#define _GNU_SOURCE 1
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
#include <poll.h>
#include <fcntl.h>

#define USER_LIMIT 5
#define BUFFER_SIZE 64
#define FD_LIMIT 65535

// 客户数据
struct client_data {
  sockaddr_in address;    // 客户端socket地址
  char *write_buf;        // 待写到客户端数据的位置
  char buf[BUFFER_SIZE];  // 从客户端读入的数据
};

int setnoblocking(int fd) {
  int old_option = fcntl(fd, F_GETFL);
  int new_option = old_option | O_NONBLOCK;
  fcntl(fd, F_SETFL, new_option);
  return old_option;
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

  // 创建users数组，分配FD_LIMIT个client_data对象
  client_data *users = new client_data[FD_LIMIT];
  pollfd fds[USER_LIMIT + 1];
  int user_conter = 0;
  for(int i = 1; i <= USER_LIMIT; ++i) {
    fds[i].fd = -1;
    fds[i].events = 0;
  }
  fds[0].fd = listenfd;
  fds[0].events = POLLIN | POLLERR;
  fds[0].revents = 0;

  while(true) {
    ret = poll(fds, user_conter + 1, -1);
    if(ret < 0) {
      printf("poll failure\n");
      break;
    }
    for(int i = 0; i < user_conter + 1; ++i) {
      if(fds[i].fd == listenfd && (fds[i].revents & POLLIN)) {
        struct sockaddr_in client_address;
        socklen_t client_addrlength = sizeof(client_address);
        int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_addrlength);
        if(connfd < 0) {
          printf("errno is: %d\n", errno);
          continue;
        }
        // 请求过多
        if(user_conter >= USER_LIMIT) {
          const char *info = "too many users\n";
          printf("%s", info);
          send(connfd, info, strlen(info), 0);
          close(connfd);
          continue;
        }
        user_conter++;
        users[connfd].address = client_address;
        setnoblocking(connfd);
        fds[user_conter].fd = connfd;
        fds[user_conter].events = POLLIN | POLLRDHUP | POLLERR;
        fds[user_conter].revents = 0;
        printf("comes a new user, now have %d users\n", user_conter);
      } else if (fds[i].revents & POLLERR) {
        printf("get an error from %d\n", fds[i].fd);
        char errors[100];
        memset(errors, '\0', 100);
        socklen_t length = sizeof(errors);
        if(getsockopt(fds[i].fd, SOL_SOCKET, SO_ERROR, &errors, &length) < 0) {
          printf("get socket option failed\n");
        }
        continue;
      } else if(fds[i].revents & POLLRDHUP) {
        users[fds[i].fd] = users[fds[user_conter].fd];
        close(fds[i].fd);
        fds[i] = fds[user_conter];
        i--;
        user_conter--;
        printf("a client left\n");
      } else if(fds[i].revents & POLLIN) {
        int connfd = fds[i].fd;
        memset(users[connfd].buf, '\0', BUFFER_SIZE);
        ret = recv(connfd, users[connfd].buf, BUFFER_SIZE - 1, 0);
        printf("get %d bytes of client data %s from %d\n", ret, users[connfd].buf, connfd);
        if(ret < 0) {
          if(errno != EAGAIN) {
            close(connfd);
            users[fds[i].fd] = users[fds[user_conter].fd];
            fds[i] = fds[user_conter];
            i--;
            user_conter--;
          }
        } else if(ret == 0) {

        } else {
          // 派发数据
          for(int j = 1; j <= user_conter; j++) {
            if(fds[j].fd == connfd) {
              continue;
            }
            fds[j].events |= ~POLLIN;
            fds[j].events |= POLLOUT;
            users[fds[j].fd].write_buf = users[connfd].buf;
          }
        }
      } else if(fds[i].revents & POLLOUT) {
        int connfd = fds[i].fd;
        if(!users[connfd].write_buf) {
          continue;
        }
        ret = send(connfd, users[connfd].write_buf, strlen(users[connfd].write_buf), 0);
        users[connfd].write_buf = NULL;
        // 重新注册可读事件
        fds[i].events |= ~POLLOUT;
        fds[i].events |= POLLIN;
      }
    }
  }
  delete[] users;
  close(listenfd);
  return 0;
}
