//
// Created by root on 21. 1. 4..
//

#include "IO_request.h"
#include "Userdata.h"
#include <queue>
#include <string.h>
#include "Partition.h"

struct timeval sstart = {};
struct timeval eend = {};
float ttime = 0;

IO_request::IO_request(char mode, class Userdata* userdata){
    rw = mode;
    read_complete = false;
    data = userdata;
}
IO_request::IO_request(char mode, const char* buffer, unsigned int buf_size, class Userdata* userdata){
    rw = mode;
    read_complete = false;
    w_buf = buffer;
    size = buf_size;
    data = userdata;
}
IO_request::IO_request(char mode, char* buffer, unsigned int r_offset, unsigned int buf_size, class Userdata* userdata){
    rw = mode;
    read_complete = false;
    r_buf = buffer;
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

	//std::unique_lock<std::mutex> lock(*(arg->mtx));
	//while (!io_queue->empty())
    	//{
	//      arg->cv->wait(lock);
    	//}

        if(!io_queue->empty()) {
            auto req = io_queue->front();
            io_queue->pop();
            io_queue_lock->unlock();
            io_queue_locked = false;
            
            if (req->rw == WRITE) {
                auto usrdata = req->data;
                usrdata->Append(req->w_buf, req->size);
                delete req;
            } else if (req->rw == READ) {
                auto usrdata = req->data;
                usrdata->Read(req->r_buf, req->size);
                while (req->read_lock.test_and_set(std::memory_order_acquire));
                req->read_complete = true;
                req->read_lock.clear(std::memory_order_release);
            } else if (req->rw == READ_RANDOM) {
	    	//gettimeofday(&sstart, NULL);
		//std::cout << "RR Receive Req : " << sstart.tv_sec << " " <<  sstart.tv_usec * 0.000001 << std::endl;
    		//std::unique_lock<std::mutex> lck(req->mtx);
		auto usrdata = req->data;
                usrdata->Read(req->r_buf, req->offset, req->size);
                while (req->read_lock.test_and_set(std::memory_order_acquire));
                req->read_complete = true;
  		//req->cv.notify_all();
		//gettimeofday(&eend, NULL);
		//ttime = (sstart.tv_sec) + (sstart.tv_usec * 0.000001);
        	//ttime = (eend.tv_sec - sstart.tv_sec) + ((eend.tv_usec - sstart.tv_usec) * 0.000001);
	    	//std::cout << "RR IO_REQ : " << ttime << std::endl;
                req->read_lock.clear(std::memory_order_release);
            } else if (req->rw == DELETE) {
                auto usrdata = req->data;
                usrdata->Delete();
                delete usrdata;
                delete req;
            }
	    else{
		std::cout << "WHAT??" << std::endl;
		exit(0);
	    }
            //io_queue_lock->lock();
	    //io_queue_locked = true;
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
            usrdata->Append(req->w_buf, req->size);
	    delete req->w_buf;
            delete req;
        } else if (req->rw == READ) {
            auto usrdata = req->data;
            usrdata->Read(req->r_buf, req->size);
            while (req->read_lock.test_and_set(std::memory_order_acquire));
            req->read_complete = true;
            req->read_lock.clear(std::memory_order_release);
	} else if (req->rw == READ_RANDOM) {
            auto usrdata = req->data;
            usrdata->Read(req->r_buf, req->offset, req->size);
            //while (req->read_lock.test_and_set(std::memory_order_acquire));
            req->read_complete = true;
            //req->read_lock.clear(std::memory_order_release);
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
        //io_queue_lock->lock();
    }

    io_queue_lock->unlock();

    return NULL;
}
