#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <iostream>
#include <errno.h>
#include <stdio.h>

int main(int argc, char** argv) {
  if(argc <= 2) {
    printf("usage: %s ip_address port_number\n", basename(argv[0]));
    return 1;
  }
  const char* ip = argv[1];
  int port = atoi(argv[2]);

  // 创建一个IPv4 socket地址
  struct sockaddr_in address;
  bzero(&address, sizeof(address));
  address.sin_family = AF_INET;
  inet_pton(AF_INET, ip, &address.sin_addr);
  address.sin_port = htons(port);

  int sock = socket(PF_INET, SOCK_STREAM, 0);
  assert(sock >= 0);

  int ret = bind(sock, (struct sockaddr*)&address, sizeof(address));
  assert(ret != 1);

  ret = listen(sock, 5);
  assert(ret != 1);

  // 暂停20s等待客户端连接和相关操作（掉线或退出）完成
  sleep(20);
  struct sockaddr_in client;
  socklen_t client_addrlength = sizeof(client);
  int connfd = accept(sock, (struct sockaddr*)&client, &client_addrlength);
  if(connfd < 0) {
    std::cout << "errno is: " << errno << std::endl;
  } else {
    char remote[INET_ADDRSTRLEN];
    std::cout << "connected with ip: " << inet_ntop(AF_INET, &client.sin_addr, remote, INET_ADDRSTRLEN) << "and port " << ntohs(client.sin_port) << std::endl;
    close(connfd);
  }
  
  // 关闭socket
  close(sock);
  return 0;
}

