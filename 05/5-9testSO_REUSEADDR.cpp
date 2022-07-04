#include <netinet/in.h>
#include <iostream>
#include <sys/socket.h>
#include <assert.h>
#include <arpa/inet.h>
#include <string.h>

int main(int argc, char** argv) {
  if(argc <= 2) {
    std::cout << "usage: " << basename(argv[0]) << " ip_address port_number" << std::endl;
    return 1;
  }

  const char* ip = argv[1];
  int port = atoi(argv[2]);

  int sock = socket(PF_INET, SOCK_STREAM, 0);
  assert(sock >= 0);

  int reuse = 1;
  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

  // 创建一个IPv4 socket地址
  struct sockaddr_in address;
  bzero(&address, sizeof(address));
  address.sin_family = AF_INET;
  inet_pton(AF_INET, ip, &address.sin_addr);
  address.sin_port = htons(port);
  int ret = bind(sock, (struct sockaddr*)&address, sizeof(address));
}
