#include <cstdlib>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iostream>

bool damonize() {
  // 创建子进程，关闭父进程，使程序在后台运行
  pid_t pid = fork();
  if(pid < 0) {
    return false;
  } else if(pid > 0) {
    exit(0);
  }

  // 创建文件权限掩码，当进程创建新文件（open(const char* pathname, int flags, mode_t mode)），文件的权限将为mode & 0777  
  umask(0);

  // 创建新会话
  pid_t sid = setsid();
  if(sid < 0) {
    return false;
  }

  // 切换工作目录
  if(chdir("/") < 0) {
    return false;
  }

  // 关闭标准输入输出错误输出设备
  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);

  // 关闭其他已经打开的文件描述符

  // 将标准输入等都定向到/dev/null文件
  open("/dev/null", O_RDONLY);
  open("/dev/null", O_RDONLY);
  open("/dev/null", O_RDONLY);
}

int main() {

}
