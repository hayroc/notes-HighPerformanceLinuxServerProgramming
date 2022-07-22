#include <sys/signal.h>
#include <event.h>
#include <cstdio>

// 信号回调
void signal_cb(int fd, short event, void *argc) {
  struct event_base *base = (event_base*)argc;
  struct timeval delay = {2, 0};
  printf("Caught an interrupt signal; exiting cleanly in two seconds...\n");
  event_base_loopexit(base, &delay);
}

// 超时回调
void timeout_cb(int fd, short event, void *argc) {
  printf("timeout\n");
}

int main() {
  // 创建一个Reactor实例
  struct event_base *base = event_init();

  // 注册一个信号事件处理器
  struct event *signal_event = evsignal_new(base, SIGINT, signal_cb, base);
  event_add(signal_event, NULL);

  // 注册一个超时事件处理器
  timeval tv = {1, 0};
  struct event *timeout_event = evtimer_new(base, timeout_cb, NULL);
  event_add(timeout_event, &tv);

  event_base_dispatch(base);

  // 释放资源
  event_free(timeout_event);
  event_free(signal_event);
  event_base_free(base);
}
