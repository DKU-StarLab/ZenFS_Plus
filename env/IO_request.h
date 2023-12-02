//
// Created by root on 21. 1. 4..
//

#ifndef ZOAD_IO_REQUEST_H
#define ZOAD_IO_REQUEST_H

#define WORKERS 1
#define WRITE 0x00
#define READ 0x01
#define DELETE 0x02

#include <queue>
#include <mutex>
#include <atomic>

class worker_arg{
public:
    std::queue<class IO_request*>* io_queue;
    std::mutex *io_queue_lock;
    unsigned int* status;
    std::mutex *status_lock;

    worker_arg(){};
};

class IO_request {
public:
    unsigned int rw;
    std::atomic_flag read_lock =ATOMIC_FLAG_INIT;
    bool read_complete;
    char* buf;
    unsigned int size;
    unsigned int offset;
    class Userdata* data;

    IO_request(char mode, class Userdata* userdata);
    IO_request(char mode, char* buffer, unsigned int buf_size, class Userdata* userdata);
    IO_request(char mode, char* buffer, unsigned int buf_size, unsigned int r_offset, class Userdata* userdata);
};

void* worker_job(class worker_arg* arg);

#endif //ZOAD_IO_REQUEST_H