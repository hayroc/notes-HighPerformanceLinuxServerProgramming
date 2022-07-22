// 15-1
#pragma once

#include <asm-generic/errno-base.h>
#include <csignal>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>

// 描述子进程的类，m_pid目标子进程pid，m_pipefd父子进程通信管道
class process {
public:
  process() : m_pid(-1) {}

public:
  pid_t m_pid;
  int m_pipefd[2];
};

// 进程池类，模板参数是处理逻辑任务的类
template <typename T>
class processpool {
private:
  // 仅能通过create创建实例
  processpool(int listenfd, int process_number = 8);
public:
  // 单体模式，程序正确处理信号的必要条件
  static processpool<T>* create(int listenfd, int process_number = 8) {
    if(!m_instance) {
      m_instance = new processpool<T>(listenfd, process_number);
    }
    return m_instance;
  }
  ~processpool() {
    delete [] m_sub_process;
  }
  // 启动进程池
  void run();

private:
  void setup_sig_pipe();
  void run_parent();
  void run_child();

private:
  // 进程池允许的最大子进程数量
  static const int MAX_PROCESS_NUMBER = 16;
  // 每个子进程最多能处理的客户数量
  static const int USER_PER_NUMBER = 65536;
  // epoll 最多能处理的事件数
  static const int MAX_EVENT_NUMBER = 10000;
  // 进程池的进程总数
  int m_process_number;
  // 子进程在池中的序号
  int m_idx;
  // 每个进程都有一个epoll内核事件表，用m_epollfd标识
  int m_epollfd;
  // 监听socket
  int m_listenfd;
  // 子进程通过m_stop决定是否停止
  int m_stop;
  // 保存所有子进程信息
  process *m_sub_process;
  // 进程池静态实例
  static processpool<T> *m_instance;
};
template<typename T>
processpool<T>* processpool<T>::m_instance = nullptr;

// 信号管道
static int sig_pipefd[2];

static int setnonblocking(int fd) {
  int old_option = fcntl(fd, F_GETFL);
  int new_option = old_option | O_NONBLOCK;
  fcntl(fd, F_SETFL, new_option);
  return old_option;
}

static void addfd(int epollfd, int fd) {
  epoll_event event;
  event.data.fd = fd;
  event.events = EPOLLIN | EPOLLET;
  epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
  setnonblocking(fd);
}

// 从epollfd标识的epoll内核事件表删除fd上所有注册事件
static void removefd(int epollfd, int fd) {
  epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
  close(fd);
}

static void sig_handler(int sig) {
  int save_errno = errno;
  int msg = sig;
  send(sig_pipefd[1], (char*)&msg, 1, 0);
  errno = save_errno;
}

static void addsig(int sig, void(handler)(int), bool restart = true) {
  struct sigaction sa;
  memset(&sa, '\0', sizeof(sa));
  sa.sa_handler = handler;
  if(restart) {
    sa.sa_flags |= SA_RESTART;
  }
  sigfillset(&sa.sa_mask);
  assert(sigaction(sig, &sa, NULL) != -1);
}

// listenfd是监听socket，必须在创建进程池前被创建
template<typename T>
processpool<T>::processpool(int listenfd, int process_number)
  : m_listenfd(listenfd), m_process_number(process_number), m_idx(-1), m_stop(false) {
  assert(process_number > 0 && process_number <= MAX_PROCESS_NUMBER);

  m_sub_process = new process[process_number];
  assert(m_sub_process);

  // 创建process_number个子进程，并建立管道
  for(int i = 0; i < process_number; ++i) {
    int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_sub_process[i].m_pipefd);
    assert(ret == 0);

    m_sub_process[i].m_pid = fork();
    assert(m_sub_process[i].m_pid >= 0);
    if(m_sub_process[i].m_pid > 0) {
      close(m_sub_process[i].m_pipefd[1]);
      continue;
    } else {
      close(m_sub_process[i].m_pipefd[0]);
      m_idx = i;
      break;
    }
  }
}

// 统一事件源
template<typename T>
void processpool<T>::setup_sig_pipe() {
  // 创建epoll事件监听表和信号管道
  m_epollfd = epoll_create(5);
  assert(m_epollfd != -1);

  int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, sig_pipefd);
  assert(ret != -1);

  setnonblocking(sig_pipefd[1]);
  addfd(m_epollfd, sig_pipefd[0]);

  // 设置信号处理函数
  addsig(SIGCHLD, sig_handler);
  addsig(SIGTERM, sig_handler);
  addsig(SIGINT, sig_handler);
  addsig(SIGPIPE, SIG_IGN);
}

// 父进程m_idx为1, 子进程m_idx>=0
template<typename T>
void processpool<T>::run() {
  if(m_idx != -1) {
    run_child();
    return ;
  }
  run_parent();
}

template<typename T>
void processpool<T>::run_child() {
  setup_sig_pipe();

  // 每个子进程都通过其在进程池中序号值m_idx找到父进程通信的管道
  int pipefd = m_sub_process[m_idx].m_pipefd[1];
  // 子进程需要监听管道文件描述符pipefd
  // 父进程通过它来通知子进程accpet新连接
  addfd(m_epollfd, pipefd);

  epoll_event events[MAX_EVENT_NUMBER];
  T *users = new T[USER_PER_NUMBER];
  assert(users);
  int number = 0;
  int ret = -1;

  while(!m_stop) {
    number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
    if(number < 0 && errno != EINTR) {
      printf("epoll failure\n");
      break;
    }
    for(int i = 0; i < number; i++) {
      int sockfd = events[i].data.fd;
      if(sockfd == pipefd && events[i].events & EPOLLIN) {
        int client = 0;
        // 从管道读取数据，将结果保存到client中
        // 如果读取成功说明有新客户连接到来
        ret = recv(sockfd, (char*)&client, sizeof(client), 0);
        if((ret < 0 && errno != EAGAIN) || ret == 0) {
          continue;
        } else {
          struct sockaddr_in client_address;
          socklen_t client_addrlength = sizeof(client_address);
          int connfd = accept(m_listenfd, (struct sockaddr*)&client_address, &client_addrlength);
          if(connfd < 0) {
            printf("errno is: %d\n", errno);
            continue;
          }
          addfd(m_epollfd, connfd);
          // 模板类T必须实现init方法，以初始化一个客户连接
          // 我们直接使用connfd来索引逻辑处理对象（T类型对象）以提高效率
          users[connfd].init(m_epollfd, connfd, client_address);
        }
      } else if(sockfd == sig_pipefd[0] && (events[i].events & EPOLLIN)) { // 处理子进程接收到的信号
        int sig;
        char signals[1024];
        ret = recv(sig_pipefd[0], signals, sizeof(signals), 0);
        if(ret <= 0) {
          continue;
        } else {
          for(int i = 0; i < ret; ++i) {
            switch (signals[i]) {
              case SIGCHLD:
                pid_t pid;
                int stat;
                while((pid = waitpid(-1, &stat, WNOHANG)) > 0) {
                  continue;
                }
                break;
              case SIGTERM:
              case SIGINT:
                m_stop = true;
                break;
              default:
                break;
            }
          }
        }

      } else if(events[i].events & EPOLLIN) { // 其他可读数据即客户请求
        users[sockfd].process();
      } else {
        continue;
      }
    }
  }
  delete [] users;
  users = nullptr;
  close(pipefd);
  // close(m_listenfd); 应该由创建者来关闭
  close(m_epollfd);
}

template<typename T>
void processpool<T>::run_parent() {
  setup_sig_pipe();

  // 父进程监听m_listenfd
  addfd(m_epollfd, m_listenfd);

  epoll_event events[MAX_EVENT_NUMBER];
  int sub_process_counter = 0;
  int new_conn = 1;
  int number = 0;
  int ret = -1;

  while(!m_stop) {
    number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
    if(number < 0 && errno != EINTR) {
      printf("epoll failure\n");
      break;
    }
    for(int i = 0; i < number; ++i) {
      int sockfd = events[i].data.fd;
      if(sockfd == m_listenfd) {
        // RoundRobin
        int i = sub_process_counter;
        do {
          if(m_sub_process[i].m_pid != -1) {
            break;
          }
          i = (i + 1) % m_process_number;
        } while(i != sub_process_counter);
        if(m_sub_process[i].m_pid == -1) {
          m_stop = true;
          break;
        }
        sub_process_counter = (i + 1) % m_process_number;
        send(m_sub_process[i].m_pipefd[0], (char*)&new_conn, sizeof(new_conn), 0);
        printf("send request to child %d\n", i);
      } else if(sockfd == sig_pipefd[0] && (events[i].events & EPOLLIN)) { // 处理父进程接收到的信号
        int sig;
        char signals[1024];
        ret = recv(sig_pipefd[0], signals, sizeof(signals), 0);
        if(ret <= 0) {
          continue;
        } else {
          for(int i = 0; i < ret; ++i) {
            switch (signals[i]) {
              case SIGCHLD:
                pid_t pid;
                int stat;
                while((pid = waitpid(-1, &stat, WNOHANG)) > 0) {
                  for(int i = 0; i < m_process_number; ++i) {
                    // 如果进程池第i个子进程退出了，则主进程关闭通信管道
                    // 设置相应的m_pid为-1
                    if(m_sub_process[i].m_pid == pid) {
                      printf("child %d join\n", i);
                      close(m_sub_process[i].m_pipefd[0]);
                      m_sub_process[i].m_pid = -1;
                    }
                  }
                }
                // 所有子进程退出则父进程也退出
                m_stop = true;
                for(int i = 0; i < m_process_number; ++i) {
                  if(m_sub_process[i].m_pid != -1) {
                    m_stop = false;
                  }
                }
                break;
              case SIGTERM:
              case SIGINT:
                // 父进程收到终止信号，那么杀死所有子进程
                printf("kill all the child now\n");
                for(int i = 0; i < m_process_number; ++i) {
                  int pid = m_sub_process[i].m_pid;
                  if(pid != -1) {
                    kill(pid, SIGTERM);
                  }
                }
                break;
              default:
                break;
            }
          }
        }
      } else {
        continue;
      }
    }
  }
  // close(m_listenfd) 由创建者关闭
  close(m_epollfd);
}
