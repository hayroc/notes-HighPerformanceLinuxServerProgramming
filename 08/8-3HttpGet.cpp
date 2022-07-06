#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#define BUFFER_SIZE 4096 //读缓存区大小

// 主状态机的两种状态
// 当前正在分析请求行
// 当前正在分析头部字段
enum CHECK_STATE {
  CHECK_STATE_REQUESTLINE = 0,
  CHECK_STATE_HEADER
};

// 从状态机的三种状态
// 读取到一个完整的行
// 行出错
// 行数据暂时不完整
enum LINE_STATUS {
  LINE_OK = 0,
  LINE_BAD,
  LINE_OPEN
};

// 服务器处理HTTP请求的结果
// 请求不完整
// 获得了完整的请求
// 请求有语法错误
// 对资源的访问权限不足
// 服务器内部错误
// 客户端已关闭连接
enum HTTP_CODE {
  NO_REQUEST,
  GET_REQUEST,
  BAD_REQUEST,
  FORBIDDEN_REQUEST,
  INTERNAL_ERROR,
  CLOSED_CONNECTION
};

// 简化应答
static const char* szret[] = { "I get a correct result\n", "Something wrrong\n" };

// 从状态机 解析出一行内容
LINE_STATUS parse_line(char *buffer, int &checked_index, int &read_index) {
  char temp;
  // checked_index 指向buffer中当前正在分析的字节
  // read_index 指向buffer中客户数据尾部的下一字节
  for(; checked_index < read_index; ++checked_index) {
    temp = buffer[checked_index];
    // 如果temp是'\r' 则说明可能为读取到一个完整的行
    if(temp == '\r') {
      if(checked_index + 1 == read_index) {
        return LINE_OPEN;
      } else if(buffer[checked_index + 1] == '\n') { // 读取到一个成功的行
        buffer[checked_index++] = '\0';
        buffer[checked_index++] = '\0';
        return LINE_OK;
      }
      return LINE_BAD; // 否则出现语法错误
    } else if(temp == '\n') {
      if(checked_index > 1 && buffer[checked_index - 1] == '\r') {
        buffer[checked_index - 1] = '\0';
        buffer[checked_index++] = '\0';
        return LINE_OK;
      }
      return LINE_BAD;
    }
  }
  return LINE_OPEN; // 还需要继续读取客户数据
}

// 分析请求行
HTTP_CODE parse_requestline(char *temp, CHECK_STATE &checkstate) {
  char *url = strpbrk(temp, " \t");
  if(!url) { // 若请求行中没有空白字符或'\t'
    return BAD_REQUEST;
  }
  *url++ = '\0';

  char *method = temp;
  if(strcasecmp(method, "GET") == 0) {
    printf("The request method is GET\n");
  } else {
    return BAD_REQUEST;
  }

  url += strspn(url, " \t");
  char *version = strpbrk(url, " \t");
  if(!version) {
    return BAD_REQUEST;
  }
  *version++ = '\0';
  version += strspn(version, " \t");

  if(strcasecmp(version, "HTTP/1.1") != 0) {
    return BAD_REQUEST;
  }
  if(strncasecmp(url, "http://", 7) == 0) {
    url += 7;
    url = strchr(url, '/');
  }

  if(!url || url[0] != '/') {
    return BAD_REQUEST;
  }
  printf("The request URL is: %s\n", url);
  checkstate = CHECK_STATE_HEADER;
  return NO_REQUEST;
}

// 分析头部字段
HTTP_CODE parse_headers(char *temp) {
  if(temp == '\0') {
    return GET_REQUEST;
  } else if(strncasecmp(temp, "Host:", 5) == 0) {
    temp += 5;
    temp += strspn(temp, " \t");
    printf("the request host is: %s\n", temp);
  } else {
    printf("I can not handle this header\n");
  }
  return NO_REQUEST;
}

// 分析HTTP请求的入口函数
HTTP_CODE parse_content(char *buffer, int &checked_index, CHECK_STATE &checkstate, int &read_index, int &start_line) {
  LINE_STATUS linestatus = LINE_OK; // 记录当前行的读取状态
  HTTP_CODE retcode = NO_REQUEST;   // 记录HTTP读取结果
  // 主状态机 用于从buffer中读取行
  while((linestatus = parse_line(buffer, checked_index, read_index)) == LINE_OK) {
    char *temp = buffer + start_line;
    start_line = checked_index; // 下一行起始位置
    switch (checkstate) {
      case CHECK_STATE_REQUESTLINE:   // 分析请求行
        retcode = parse_requestline(temp, checkstate);
        if(retcode == BAD_REQUEST) {
          return BAD_REQUEST;
        }
        break;
      case CHECK_STATE_HEADER:        // 分析头部字段
        retcode = parse_headers(temp);
        if(retcode == BAD_REQUEST) {
          return BAD_REQUEST;
        } else if(retcode == GET_REQUEST) {
          return GET_REQUEST;
        }
        break;
      default:
        return INTERNAL_ERROR;
    }
  }
  if(linestatus == LINE_OPEN) {
    return NO_REQUEST;
  } else {
    return BAD_REQUEST;
  }
}

int main(int argc, char **argv) {
  if(argc <= 2) {
    printf("usage: %s ip_address port_number\n", basename(argv[0]));
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

  struct sockaddr_in client_address;
  socklen_t client_addrlenght = sizeof(client_address);
  int fd = accept(listenfd, (struct sockaddr*)&client_address, &client_addrlenght);
  if(fd < 0) {
    printf("errno is: %d\n", errno);
  } else {
    char buffer[BUFFER_SIZE];
    memset(buffer, '\0', BUFFER_SIZE);
    int data_read = 0;
    int read_index = 0;
    int checked_index = 0;
    int start_line = 0;
    CHECK_STATE checkstate = CHECK_STATE_REQUESTLINE;
    while (true) {
      data_read = recv(fd, buffer + read_index, BUFFER_SIZE - read_index, 0);
      if(data_read == -1) {
        printf("reading failed\n");
        break;
      } else if(data_read == 0) {
        printf("remote client has closed the connection\n");
        break;
      }
      read_index += data_read;
      // 分析目前获取到的数据
      HTTP_CODE result = parse_content(buffer, checked_index, checkstate, read_index, start_line);
      if(result == NO_REQUEST) {  // 尚未得到完整请求
        continue;
      } else if(result == GET_REQUEST) { // 得到一个完整的请求
        send(fd, szret[0], strlen(szret[0]), 0);
        break;
      } else {
        send(fd, szret[1], strlen(szret[1]), 0);
        break;
      }
    }
    close(fd);
  }
  close(listenfd);
  return 0;
}
