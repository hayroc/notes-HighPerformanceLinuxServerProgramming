#include <sys/poll.h>
#include <sys/epoll.h>

void difference() {
  int ret = poll(fds, MAX_EVENT_NUMBER, -1);
  // 必须遍历所有已注册文件描述符找到其中就绪者
  for(int i = 0; i < MAX_EVENT_NUMBER; ++i) {
    if(fds[i].revents & POLLIN) {
      int sockfd = fds[i].fd;
      // 处理
    }
  }

  int ret = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
  // 直接遍历就绪文件描述符
  for(int i = 0; i < ret; i++) {
    int sockfd = events[i].data.fd;
    // 处理
  }
}
