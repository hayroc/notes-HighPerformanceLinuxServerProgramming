#include <cstdio>
#include <iostream>

void byteorder() {
  union {
    short val;
    char union_bytes[sizeof(short)];
  } test;
  test.val = 0x0102;
  if(test.union_bytes[0] == 1 && test.union_bytes[1] == 2) {
    std::cout << "big endian" << std::endl;
  } else if(test.union_bytes[0] == 2 && test.union_bytes[1] == 1) {
    std::cout << "little endian" << std::endl;
  } else {
    std::cout << "unkown" << std::endl;
  }
}

int main() {
  byteorder();
}
