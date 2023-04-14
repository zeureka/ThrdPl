#pragma once
#include <functional>
#include <pthread.h>
#include <queue>
#include <vector>

namespace iaee {
#define DEFAULT_THREAD_ADD_DEL_NUM 2
#define THRPL_DEBUG 0

// 定义任务结构体
// using callback = void (*)(void *);
using callback = std::function<void(void *)>;

struct Task {
  Task() : _function(nullptr), _arg(nullptr) {}
  Task(callback func, void *arg) : _function(func), _arg(arg) {}

  callback _function;
  void *_arg;
};

//  定义任务队列
class TaskQueue {
public:
  TaskQueue();  // 构造函数
  ~TaskQueue(); // 析构函数

  void addTask(const Task &task);         // 添加任务
  void addTask(callback func, void *arg); // 添加任务
  Task takeTask();                        // 取出一个任务
  inline int taskNumber(); // 获取当前队列中的任务个数

private:
  pthread_mutex_t m_mutex;  // 互斥锁
  std::queue<Task> m_taskQ; // 任务队列
};

class ThreadPool {
public:
  ThreadPool(const int &minNum, const int &maxNum); // 构造函数
  ~ThreadPool();                                    // 析构函数
  void addTask(const Task &task);                   // 添加任务

private:
  static void *worker(void *arg); // 工作线程的任务函数
  static void *adjust(void *arg); // 管理者线程的任务函数
  void threadExit();              // 单个线程退出函数

private:
  TaskQueue *_taskQ;          // 任务队列对象
  pthread_mutex_t _mutexPool; // 锁整个线程池
  pthread_mutex_t _mutexBusy; // 锁忙的线程数
  pthread_cond_t _notEmpty;   // 任务队列不为空的条件变量
  pthread_cond_t _exeDestroy; // 调用析构函数后是否立马向下执行
  std::vector<pthread_t> _threads; // 线程数组
  pthread_t _admin;                // 管理者线程
  int _minNum;                     // 线程池最小线程数
  int _maxNum;                     // 线程池最大线程数
  int _busyNum;                    // 线程池忙的线程数
  int _aliveNum;                   // 线程池存活的线程数
  int _exitNum;                    // 需要退出的线程数
  bool _shutdown;                  // 关闭线程池标志
};

} // namespace iaee
