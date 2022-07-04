#include <netinet/in.h>
#include <iostream>
#include <assert.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#define BUFFER_SIZE 1024

int main(int argc, char** argv) {
  if(argc <= 3) {
    std::cout << "usage: " << basename(argv[0]) << " ip_address port_number buffer_size" << std::endl;
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

  int recvbuf = atoi(argv[3]);
  int len = sizeof(recvbuf);
  // 设置TCP发送缓冲区大小
  setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &recvbuf, sizeof(recvbuf));
  getsockopt(sock, SOL_SOCKET, SO_SNDBUF, &recvbuf, (socklen_t*)&len);
  std::cout << "the tcp recvbuf size after setting is " << recvbuf << std::endl;

  int ret = bind(sock, (struct sockaddr*)&address, sizeof(address));
  assert(ret != -1);

  ret = listen(sock, 5);
  assert(ret != -1);

  struct sockaddr_in client;
  socklen_t client_addrlenght = sizeof(client);
  int connfd = accept(sock, (struct sockaddr*)&client, &client_addrlenght);
  if(connfd < 0) {
    std::cout << "errno is: " << errno << std::endl;
  } else {
    char buffer[BUFFER_SIZE];
    memset(buffer, '\0', BUFFER_SIZE);
    while(recv(connfd, buffer, BUFFER_SIZE-1, 0) > 0) {
      std::cout << buffer << std::endl;
    }
    close(connfd);
  }
  close(sock);

  return 0;
}

