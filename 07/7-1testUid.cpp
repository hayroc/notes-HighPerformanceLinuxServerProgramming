#include <unistd.h>
#include <iostream>

int main() {
  uid_t uid = getuid();
  uid_t euid = geteuid();
  std::cout << "userid is: " << uid << ", effective userid is: " << euid << std::endl;
  return 0;
}
