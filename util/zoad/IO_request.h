//
// Created by root on 21. 1. 4..
//

#ifndef ZOAD_IO_REQUEST_H
#define ZOAD_IO_REQUEST_H

#define WORKERS 1
#define WRITE 0x00
#define READ 0x01
#define READ_RANDOM 0x02
#define DELETE 0x03

#include <queue>
#include <mutex>
#include <atomic>
#include <condition_variable>

class worker_arg{
public:
    std::queue<class IO_request*>* io_queue;
    std::mutex *io_queue_lock;
    unsigned int* status;
    std::mutex *status_lock;

    std::mutex *mtx;
    std::condition_variable *cv;
    bool *submit;

    worker_arg(){};
};

class IO_request {
public:
    unsigned int rw;
    std::atomic_flag read_lock =ATOMIC_FLAG_INIT;
    std::mutex mtx;
    std::condition_variable cv;
    bool read_complete;
    const char* w_buf;
    char* r_buf;
    unsigned int size;
    unsigned int offset;
    class Userdata* data;

    IO_request(char mode, class Userdata* userdata);
    IO_request(char mode, const char* buffer, unsigned int buf_size, class Userdata* userdata);
    IO_request(char mode, char* buffer, unsigned int r_offset, unsigned int buf_size, class Userdata* userdata);
};

void* worker_job(class worker_arg* arg);

#endif //ZOAD_IO_REQUEST_H
