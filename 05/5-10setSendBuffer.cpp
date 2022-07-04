#include <netinet/in.h>
#include <iostream>
#include <assert.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#define BUFFER_SIZE 512

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

  int sendbuf = atoi(argv[3]);
  int len = sizeof(sendbuf);
  // 设置TCP发送缓冲区大小
  setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &sendbuf, sizeof(sendbuf));
  getsockopt(sock, SOL_SOCKET, SO_SNDBUF, &sendbuf, (socklen_t*)&len);
  std::cout << "the tcp sendbuf size after setting is " << sendbuf << std::endl;

  if(connect(sock, (struct sockaddr*)&address, sizeof(address)) != -1) {
    char buffer[BUFFER_SIZE];
    memset(buffer, 'a', BUFFER_SIZE);
    send(sock, buffer, BUFFER_SIZE, 0);
  }
  close(sock);
  return 0;
}

