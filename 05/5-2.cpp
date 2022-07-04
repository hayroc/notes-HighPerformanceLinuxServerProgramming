#include <iostream>
#include <arpa/inet.h>

int main() {
  in_addr add1;
  inet_aton("1.2.3.4", &add1);
  in_addr add2;
  inet_aton("10.194.71.60", &add2);
  
  char* szVal1 = inet_ntoa(add1);
  char* szVal2 = inet_ntoa(add2);
  std::cout << "address1: " << szVal1 << std::endl; // address1: 10.194.71.60
  std::cout << "address2: " << szVal2 << std::endl; // address2: 10.194.71.60
}
