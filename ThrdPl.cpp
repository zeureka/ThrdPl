#include "ThrdPl.h"
#include <iostream>
#include <pthread.h>
#include <unistd.h>

namespace iaee {
/*************** TaskQueue.cpp ***************/
TaskQueue::TaskQueue() { pthread_mutex_init(&m_mutex, NULL); }

TaskQueue::~TaskQueue() {
  pthread_mutex_lock(&m_mutex);
  pthread_mutex_destroy(&m_mutex);
}

void TaskQueue::addTask(const Task &task) {
  pthread_mutex_lock(&m_mutex);
  m_taskQ.push(task);
  pthread_mutex_unlock(&m_mutex);
}

void TaskQueue::addTask(callback func, void *arg) {
  pthread_mutex_lock(&m_mutex);
  m_taskQ.push(Task(func, arg));
  pthread_mutex_unlock(&m_mutex);
}

Task TaskQueue::takeTask() {
  Task task;
  pthread_mutex_lock(&m_mutex);

  // 如果队列不为空则取出一个任务
  if (!m_taskQ.empty()) {
    task = m_taskQ.front();
    m_taskQ.pop();
  }

  pthread_mutex_unlock(&m_mutex);
  return task;
}

int TaskQueue::taskNumber() { return m_taskQ.size(); }
/*************** threadPool.cpp ***************/

ThreadPool::ThreadPool(const int &minNum, const int &maxNum) {
  do {
    // 实例化任务队列
    _taskQ = new TaskQueue;
    if (nullptr == _taskQ) {
      perror("new _taskQ fail...");
      break;
    }

    // 重新设置线程数组的容量
    _threads.reserve(maxNum + 10);
    for (int i = 0; i < maxNum; ++i) {
      _threads[i] = 0;
    }

    // 初始化数据成员
    _minNum = minNum;
    _maxNum = maxNum;
    _busyNum = 0;
    _aliveNum = minNum;
    _exitNum = 0;
    _shutdown = false;

    // 初始化互斥锁和条件变量
    if (pthread_mutex_init(&_mutexPool, NULL) != 0 ||
        pthread_mutex_init(&_mutexBusy, NULL) != 0 ||
        pthread_cond_init(&_exeDestroy, NULL) != 0 ||
        pthread_cond_init(&_notEmpty, NULL) != 0) {
      perror("init mutex or condition fail...");
      break;
    }

    // 创建管理者线程
    pthread_create(&_admin, NULL, adjust, this);

    // 根据最小线程数创建线程
    for (int i = 0; i < minNum; ++i) {
      pthread_create(&_threads[i], NULL, worker, this);
#if THRPL_DEBUG
      std::cout << "创建子线程, ID: " << std::to_string(_threads[i])
                << std::endl;
#endif
    }

    return;
  } while (0);

  // 如果初始化线程池失败，释放内存
  if (nullptr != _taskQ) {
    delete _taskQ;
    _taskQ = nullptr;
  }
}

ThreadPool::~ThreadPool() {
#if 0
  while (_taskQ->taskNumber() != 0) {
    sleep(1);
  }
#else
  pthread_mutex_lock(&_mutexPool);
  while (_taskQ->taskNumber() != 0) {
    pthread_cond_wait(&_exeDestroy, &_mutexPool);
  }
  pthread_mutex_unlock(&_mutexPool);
#endif

  _shutdown = true;

  // 回收管理者线程
  pthread_join(_admin, NULL);

  // 回收工作线程
  for (int i = 0; i < _aliveNum; ++i) {
    pthread_cond_signal(&_notEmpty);
  }

  // 释放资源
  if (_taskQ) {
    delete _taskQ;
    _taskQ = nullptr;
  }

  // 释放锁资源
  pthread_mutex_lock(&_mutexPool);
  pthread_mutex_destroy(&_mutexPool);
  pthread_mutex_lock(&_mutexBusy);
  pthread_mutex_destroy(&_mutexBusy);
  pthread_cond_destroy(&_exeDestroy);
  pthread_cond_destroy(&_notEmpty);
}

void ThreadPool::addTask(const Task &task) {
  if (_shutdown) {
    return;
  }

  _taskQ->addTask(task);
  pthread_cond_signal(&_notEmpty);
}

void *ThreadPool::worker(void *arg) {
  ThreadPool *self = static_cast<ThreadPool *>(arg);

  while (true) {
    pthread_mutex_lock(&self->_mutexPool);

    // 如果任务队列为空
    while (0 == self->_taskQ->taskNumber() && !self->_shutdown) {
      pthread_cond_signal(&self->_exeDestroy);
      pthread_cond_wait(&self->_notEmpty, &self->_mutexPool);

      // 让线程自行调用 threadExit() 进行销毁
      if (self->_exitNum > 0) {
        self->_exitNum--;
        if (self->_aliveNum > self->_minNum) {
          self->_aliveNum--;
          pthread_mutex_unlock(&self->_mutexPool);
          self->threadExit();
        }
      }
    }

    // 如果线程池关闭，则销毁线程
    if (self->_shutdown) {
      pthread_mutex_unlock(&self->_mutexPool);
      self->threadExit();
    }

    // 取出任务
    Task task = self->_taskQ->takeTask();
    pthread_mutex_unlock(&self->_mutexPool);

    pthread_mutex_lock(&self->_mutexBusy);
    self->_busyNum++;
#if THRPL_DEBUG
    std::cout << "thread " << std::to_string(pthread_self())
              << " start working..." << std::endl;
#endif
    pthread_mutex_unlock(&self->_mutexBusy);

    // 执行任务
    task._function(task._arg);

    pthread_mutex_lock(&self->_mutexBusy);
    self->_busyNum--;
#if THRPL_DEBUG
    std::cout << "thread " << std::to_string(pthread_self())
              << " end working..." << std::endl;
#endif
    pthread_mutex_unlock(&self->_mutexBusy);
  }

  return nullptr;
}

void *ThreadPool::adjust(void *arg) {
  ThreadPool *self = static_cast<ThreadPool *>(arg);
  while (!self->_shutdown) {
    sleep(3);

    pthread_mutex_lock(&self->_mutexPool);
    int qSize = self->_taskQ->taskNumber();
    int aliveNum = self->_aliveNum;
    pthread_mutex_unlock(&self->_mutexPool);

    pthread_mutex_lock(&self->_mutexBusy);
    int busyNum = self->_busyNum;
    pthread_mutex_unlock(&self->_mutexBusy);

    // 如果满足条件，则创建线程
    if (qSize > aliveNum && aliveNum < self->_maxNum) {
#if THRPL_DEBUG
      printf("admin: -[busyNum--%d]-[aliveNum--%d]---add\n", busyNum, aliveNum);
#endif
      pthread_mutex_lock(&self->_mutexPool);
      int addNum = 0;

      for (int i = 0; addNum < DEFAULT_THREAD_ADD_DEL_NUM &&
                      i < self->_maxNum && self->_aliveNum < self->_maxNum;
           ++i) {

        // 找到线程数组中没有记录线程的位置
        if (0 == self->_threads[i]) {
          pthread_create(&self->_threads[i], NULL, worker, self);
          addNum++;
          self->_aliveNum++;
        }
      }

      pthread_mutex_unlock(&self->_mutexPool);
    }

    // 如果满足条件，则销毁线程
    if (busyNum * 2 < aliveNum && aliveNum > self->_minNum) {
#if THRPL_DEBUG
      printf("admin: -[busyNum--%d]-[aliveNum--%d]---del\n", busyNum, aliveNum);
#endif
      pthread_mutex_lock(&self->_mutexPool);
      self->_exitNum = DEFAULT_THREAD_ADD_DEL_NUM;
      pthread_mutex_unlock(&self->_mutexPool);

      // 在线程工作回调函数中销毁
      for (int i = 0; i < DEFAULT_THREAD_ADD_DEL_NUM; ++i) {
        pthread_cond_signal(&self->_notEmpty);
      }
    }
  }

  return nullptr;
}

void ThreadPool::threadExit() {
  pthread_t tid = pthread_self();

  for (int i = 0; i < _maxNum; ++i) {
    if (tid == _threads[i]) {
      _threads[i] = 0;
#if THRPL_DEBUG
      std::cout << "threadExit called, " << std::to_string(tid) << " exit..."
                << std::endl;
#endif
      break;
    }
  }

  // 分离线程
  pthread_detach(tid);
  pthread_exit(NULL);
}

} // namespace iaee
