//
// Created by root on 21. 1. 4..
//

#include "IO_request.h"
#include "Userdata.h"
#include <queue>
#include <string.h>
#include "Partition.h"

//struct timeval sstart = {};
//struct timeval eend = {};
//float ttime = 0;

IO_request::IO_request(char mode, class Userdata* userdata){
    rw = mode;
    read_complete = false;
    data = userdata;
}
IO_request::IO_request(char mode, char* buffer, unsigned int buf_size, class Userdata* userdata){
    rw = mode;
    buf = buffer;
    size = buf_size;
    data = userdata;
}
IO_request::IO_request(char mode, char* buffer, unsigned int buf_size, unsigned int r_offset, class Userdata* userdata){
    rw = mode;
    buf = buffer;
    size = buf_size;
    offset = r_offset;
    data = userdata;
}
void* worker_job(class worker_arg* arg){
    std::queue<class IO_request*>* io_queue = arg->io_queue;
    std::mutex* io_queue_lock = arg->io_queue_lock;
    unsigned int* status = arg->status;
    std::mutex* status_lock = arg->status_lock;
    bool io_queue_locked = false;

    while(true) {
        io_queue_lock->lock();
        io_queue_locked = true;
        if (!io_queue->empty()) {
            auto req = io_queue->front();
            io_queue->pop();
            io_queue_lock->unlock();
            io_queue_locked = false;

            //gettimeofday(&sstart, NULL);
            if (req->rw == WRITE) {
                auto usrdata = req->data;
                usrdata->Append(req->buf, req->size);
                delete req;
            } else if (req->rw == READ) {
                auto usrdata = req->data;
                usrdata->Read(req->buf, req->size);
                while (req->read_lock.test_and_set(std::memory_order_acquire));
                req->read_complete = true;
                req->read_lock.clear(std::memory_order_release);
            } else if (req->rw == DELETE) {
                auto usrdata = req->data;
                usrdata->Delete();
                delete usrdata;
                delete req;
            }
            //gettimeofday(&eend, NULL);
            //ttime += (eend.tv_sec - sstart.tv_sec) + ((eend.tv_usec - sstart.tv_usec) * 0.000001);
        }
        if(io_queue_locked) {
            io_queue_lock->unlock();
        }

        status_lock->lock();
        if(*status == CLOSE){
            status_lock->unlock();
            break;
        }
        status_lock->unlock();
    }

    io_queue_lock->lock();
    while(!io_queue->empty()) {
        auto req = io_queue->front();
        io_queue->pop();
        io_queue_lock->unlock();

        //gettimeofday(&sstart, NULL);
        if (req->rw == WRITE) {
            auto usrdata = req->data;
            usrdata->Append(req->buf, req->size);
            delete req;
        } else if (req->rw == READ) {
            auto usrdata = req->data;
            usrdata->Read(req->buf, req->size);
            while (req->read_lock.test_and_set(std::memory_order_acquire));
            req->read_complete = true;
            req->read_lock.clear(std::memory_order_release);
        } else if (req->rw == DELETE) {
            auto usrdata = req->data;
            usrdata->Delete();
            delete usrdata;
            delete req;
        }
        else{
            std::cout << "Wrong request type" << std::endl;
            std::cout << req->rw << std::endl;
            exit(0);
        }

        //gettimeofday(&eend, NULL);
        //ttime += (eend.tv_sec - sstart.tv_sec) + ((eend.tv_usec - sstart.tv_usec) * 0.000001);
        io_queue_lock->lock();
    }

    io_queue_lock->unlock();

    //std::cout << ttime << std::endl;
    return NULL;
}