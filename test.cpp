#include "ThrdPl.h"
#include <iostream>
#include <stdio.h>
#include <unistd.h>
using namespace iaee;
using namespace std;

void taskFanc(void *arg) {
  printf("thread callback function working... num: %d,\n", *(int *)arg);
  delete (int *)arg;
  arg = nullptr;
  sleep(1);
}

int main(int argc, char *argv[]) {

  // 创建线程池
  ThreadPool *pool = new ThreadPool(3, 10);
  for (int i = 0; i < 100; ++i) {
    int *num = new int(i + 100);
    pool->addTask(Task(taskFanc, num));
  }

  sleep(30);

  for (int i = 0; i < 100; ++i) {
    int *num = new int(i + 100);
    pool->addTask(Task(taskFanc, num));
  }

  // sleep(30);

  delete pool;

  return 0;
}
