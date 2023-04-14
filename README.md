# ThrdPl
线程池在 C++ 中的实现。

## Run an example
这个线程池使用了 [POSIX Threads](https://zh.wikipedia.org/wiki/POSIX%E7%BA%BF%E7%A8%8B )，
所以在编译的时候需要使用 -lpthread
```bash
g++ test.cpp ThrdPl.cpp -o test && ./test
```

## Usage
```bash
git clone https://github.com/zeureka/ThrdPl.gith
```
将ThrdPl.c和ThrdPl.h放在您的项目中，或者生成一个只包含头的文件并将其放在项目中。

## API 
| 函数                                             | 描述                                                                |
|--------------------------------------------------|---------------------------------------------------------------------|
| ThreadPool(const int& minNum, const int& maxNum) | 创建一个线程池对象，可以设置线程池中的最小/最大线程数（默认为3/10） |  |
| void addTask(const Task& task)                   | 向任务队列里添加一个任务                                            |
