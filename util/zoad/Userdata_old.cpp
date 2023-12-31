//
// Created by root on 21. 1. 4..
//

#include "Userdata.h"
#include "Partition.h"
#include "Zone.h"
#include <algorithm>


void insert_segment(std::vector<class Segment*> &cont, class Segment* segment){
    auto it = upper_bound(cont.begin(), cont.end(), *segment);
    cont.insert(it, segment);
}

int Userdata::_write(class Segment* segment, const char* buf, unsigned int write_size) {
    auto izg_cnt = partition->get_izg_cnt();
    int target_size = write_size;
    unsigned int free_size = 0;
    unsigned int target_zone_cnt = 0;
    std::vector<class Zone*>::reverse_iterator it;
    std::vector<class Zone*>::reverse_iterator it2;
    std::vector<class Interval*>* interval_list = new std::vector<class Interval*>;
    std::vector<unsigned int>* write_size_vector =  new std::vector<unsigned int>;
    int ret = 0;
    unsigned int cnt = 0;

    if (partition->zone_empty()) {
        for (int i = 0; i < izg_cnt; i++) {
            if (partition->alloc_zone() < 0) {
                std::cout << "_write - allocd_zone fail" << std::endl;
                exit(0);
            }
        }
    } else if (partition->zone_size() < izg_cnt) {
        for (int i = 0; i < izg_cnt - partition->zone_size(); i++) {
            if (partition->alloc_zone() < 0) {
                std::cout << "_write - allocd_zone fail" << std::endl;
                exit(0);
            }
        }
    }
    else{
        it = partition->get_zone_last_iter();
        for (int i = 0; i < izg_cnt; i++, it++) {
     	    auto tmp_zone = *it;
            if(tmp_zone->full()){
                cnt++;
            }
	}

        for (int i = 0; i < cnt; i++, it++) {
	    if (partition->alloc_zone() < 0) {
                std::cout << "_write - allocd_zone fail" << std::endl;
                exit(0);
            }
	}
    }
    
    target_zone_cnt = izg_cnt;
    //std::cout << target_zone_cnt << std::endl;
    it = partition->get_zone_last_iter();
    for (int i = 0; i < target_zone_cnt; i++, it++) {
        class Zone* tmp_zone = *it;
        if(tmp_zone->full()){
            i++;
            continue;
        }
        free_size = tmp_zone->get_free_size();
        target_size -= free_size;
        write_size_vector->push_back(free_size);
    }
    std::reverse(write_size_vector->begin(), write_size_vector->end());
    
    while(target_size > 0){
        for (int i = 0; i < izg_cnt; i++) {
            if (partition->alloc_zone() < 0) {
                std::cout << "_write - allocd_zone fail" << std::endl;
                exit(0);
            }
            target_size -= zns_get_zone_size();
            write_size_vector->push_back(zns_get_zone_size());
        }
        target_zone_cnt += izg_cnt;
    }

    //std::cout << target_zone_cnt << std::endl;
    it = partition->get_zone_last_iter();
    for (int i = 0; i < target_zone_cnt; i++, it++) {
        auto tmp_zone = *it;
        if(tmp_zone->full()){
            i++;
            continue;
        }
        auto interval = new class Interval(tmp_zone->get_zone_number(), segment->get_id(), tmp_zone->get_wp(), tmp_zone->get_wp());
        tmp_zone->insert_interval(interval);
        interval_list->push_back(interval);
    }
    std::reverse(interval_list->begin(), interval_list->end());
    segment->set_interval_list(interval_list);

    ret = izg_write(buf, partition->get_izg_cnt(), target_zone_cnt, interval_list, write_size_vector, write_size);

    write_size_vector->clear();
    delete write_size_vector;

    return ret;
}

int Userdata::_read(class Segment* segment, char* buf, unsigned int start_offset, unsigned int req_size){
    auto izg_cnt = partition->get_izg_cnt();
    unsigned int target_zone_cnt = 0;
    std::vector<class Interval*>::iterator it;
    std::vector<class Interval*>::iterator it_end;
    std::vector<class Interval*>* interval_list;
    class Interval *tmp_interval;
    int ret = 0;

    interval_list = segment->get_interval_list();
    it = interval_list->begin();
    it_end = interval_list->end();
    for(; it != it_end ; it++) {
        target_zone_cnt++;
    }
    
    ret = izg_read(buf, izg_cnt, target_zone_cnt, interval_list, start_offset, req_size);

    return ret;
}

int Userdata::_read(class Segment* segment, char* buf, unsigned int req_size){
    auto izg_cnt = partition->get_izg_cnt();
    unsigned int target_zone_cnt = 0;
    std::vector<class Interval*>::iterator it;
    std::vector<class Interval*>::iterator it_end;
    std::vector<class Interval*>* interval_list;
    class Interval *tmp_interval;
    int ret = 0;

    interval_list = segment->get_interval_list();
    if(req_size/IO_BLOCKS >= izg_cnt){
	    it = interval_list->begin();
	    it_end = interval_list->end();
	    for(; it != it_end ; it++) {
        	target_zone_cnt++;
    	}
    }
    else{
        target_zone_cnt = req_size/IO_BLOCKS;
	if(target_zone_cnt == 0)
	    target_zone_cnt++;
    }

    ret = izg_read(buf, izg_cnt, target_zone_cnt, interval_list, req_size);

    return ret;
}
Userdata::Userdata(class Partition* _partition){
    partition = _partition;
    remain_write_req = 0;
    size = 0;
}
unsigned int Userdata::Get_size(){
    return size;
}

int Userdata::Read(char* buf, unsigned int offset, unsigned int read_size)//application needs to alloc 192kb buffer
{
    class Segment* segment;
    auto izg_cnt = partition->get_izg_cnt();
    unsigned int ori_offset = offset;
    unsigned int ori_read_size = read_size;
    unsigned int start_offset = 0;
    unsigned int segment_offset = 0;
    unsigned int segment_size = 0;
    unsigned int req_size = 0;
    unsigned int buf_offset = 0;
    ssize_t r = 0;
    bool remain_bytes = false;
    char* tmp_buf;
    unsigned int tmp_size;
    /*
    if(remain_write_req != 0){
	    std::cout << "Read early" << std::endl;
    }
    */
    //std::cout << "Req Original offset " << offset << std::endl;
    //std::cout << "Req Original size " << read_size << std::endl;

    read_size /= BLOCK_SIZE;
    if(ori_read_size % BLOCK_SIZE){
        read_size++;
    }

    offset /= BLOCK_SIZE;
    /*
    if(ori_offset > 0 and ori_offset % BLOCK_SIZE){
        offset++;
    }
    */
    if (offset > size)
    {
        return -1;
    }
    
    //std::cout << "Req Block offset " << offset << std::endl;
    //std::cout << "Req Block size " << read_size << std::endl;

    req_size = read_size;
    while (write_lock.test_and_set(std::memory_order_acquire));

    for (auto iter = file_segment.begin(); iter != file_segment.end(); iter++){
        if(!read_size) {
            break;
        }
        segment = *iter;

        segment_offset = segment->get_offset();
        segment_size = segment->get_size();
	//std::cout << "So " << segment_offset << std::endl;
	//std::cout << "Ss " << segment_size << std::endl;
	//std::cout << "Req offset " << offset << std::endl;
        if(offset > segment_offset + segment_size - 1) {
            continue;
        }
        
	if(offset >= segment_offset and offset < segment_offset + segment_size){
            if(offset == segment_offset){
                start_offset = 0;
                if(req_size > segment_size) {
                    req_size = segment_size;
                }
                else {
                    req_size = read_size;
                }
            }
            else{
                start_offset = offset - segment_offset;
                if(start_offset + read_size > segment_size) {
                    req_size = segment_size - start_offset;
                }
                else {
                    req_size = read_size;
                }
            }

	    //std::cout << "H " << offset <<  std::endl;
	    //std::cout << "H2 " << req_size <<  std::endl;
	    //std::cout << "H3 " << start_offset <<  std::endl;

            read_size -= req_size;
            offset += req_size;

	    if((ori_offset > 0 and ori_offset % IO_SIZE * izg_cnt) or remain_bytes or req_size < IO_BLOCKS * izg_cnt){
		tmp_size = req_size/IO_BLOCKS;
		if(req_size%IO_BLOCKS)
			tmp_size++;
		tmp_buf = new char[IO_SIZE * izg_cnt];

	    	r = _read(segment, tmp_buf, start_offset, IO_BLOCKS * izg_cnt);
		if(remain_bytes == false){
		//std::cout << ori_offset%IO_SIZE << std::endl;
		//std::cout << IO_SIZE - (ori_offset%IO_SIZE) << std::endl;
		//std::cout << ori_read_size << std::endl;
			if(req_size < (IO_BLOCKS * izg_cnt)){
				if(ori_read_size <= (IO_SIZE * izg_cnt) - (ori_offset%(IO_SIZE * izg_cnt))){
		  		    memcpy(buf + buf_offset, tmp_buf + ori_offset%(IO_SIZE * izg_cnt), ori_read_size);
				}
				else{
				    memcpy(buf + buf_offset, tmp_buf + ori_offset%(IO_SIZE * izg_cnt), (IO_SIZE * izg_cnt) - (ori_offset%(IO_SIZE * izg_cnt)));
				}
			}
			else{
			    memcpy(buf + buf_offset, tmp_buf + ori_offset%(IO_SIZE * izg_cnt), (IO_SIZE * izg_cnt) - (ori_offset%(IO_SIZE * izg_cnt)));
			}
		}
		else{
		    memcpy(buf + buf_offset, tmp_buf, ori_read_size);
		}
		start_offset += ((IO_SIZE * izg_cnt) - (ori_offset%(IO_SIZE * izg_cnt)))/BLOCK_SIZE;
		//start_offset += IO_BLOCKS;
		//start_offset -= start_offset&IO_BLOCKS;
		
		if(req_size >= IO_BLOCKS * izg_cnt){
		    req_size -= ((IO_SIZE * izg_cnt) - (ori_offset%(IO_SIZE * izg_cnt)))/BLOCK_SIZE;
   		    if(((IO_SIZE * izg_cnt) - (ori_offset%(IO_SIZE * izg_cnt)))%BLOCK_SIZE){
		        req_size--;
		    }
		}
		else{
		    req_size = 0;
		}

	        //std::cout << "HHH2 " << req_size << std::endl;
	        //std::cout << "HHH2.5 " << start_offset << std::endl;
		
		if(remain_bytes){
		    remain_bytes = false;
		}
		else{
   		    if(ori_read_size > (IO_SIZE * izg_cnt) - (ori_offset%(IO_SIZE * izg_cnt)) and req_size == 0 and read_size == 0){
		        ori_read_size -= (IO_SIZE * izg_cnt) - (ori_offset%(IO_SIZE * izg_cnt));
			offset = segment_offset + segment_size;
	    	        read_size = ori_read_size / BLOCK_SIZE;
    			if(ori_read_size % BLOCK_SIZE){
		            read_size++;
			}
			req_size = read_size;
		        remain_bytes = true;
		        //std::cout << "remain bytes : " << ori_read_size << std::endl;
		    }
		}
		
		buf_offset += (IO_SIZE * izg_cnt) - (ori_offset % (IO_SIZE * izg_cnt));
		ori_offset = 0;

		delete[] tmp_buf;
	    }

	    if(req_size > 0 and remain_bytes != true){
      	        r = _read(segment, buf + buf_offset, start_offset, req_size);
	        buf_offset += req_size * BLOCK_SIZE;
	    }
        }
    }

    //std::cout << "buf[0] : " << buf[0] << std::endl;
    if(read_size){
        std::cout << "Read size error" << std::endl;
        exit(0);
    }

    write_lock.clear(std::memory_order_release);

    return 0;
}

int Userdata::Read(char* buf, unsigned int read_size)
{
    class Segment* segment;
    unsigned int ori_read_size = read_size;
    char* tmp_buf = buf;

    read_size /= BLOCK_SIZE;
    if(ori_read_size % BLOCK_SIZE){
        read_size++;
    }

    unsigned int segment_size = 0;
    ssize_t r = 0;

    while (write_lock.test_and_set(std::memory_order_acquire));

    for (auto iter = file_segment.begin(); iter != file_segment.end(); iter++){
        if(read_size <= 0) {
            break;
        }
        segment = *iter;
        segment_size = segment->get_size();
        r = _read(segment, tmp_buf, segment_size);
        //std::cout << tmp_buf[0] << std::endl;
        //std::cout << tmp_buf[(segment_size * BLOCK_SIZE) - 1] << std::endl;
        read_size -= segment_size;
        tmp_buf += segment_size * BLOCK_SIZE;
    }

    /*
    if(read_size != 0){
        std::cout << "Read error" << std::endl;
        exit(0);
    }
    */

    write_lock.clear(std::memory_order_release);

    return 0;
}
int Userdata::Append(const char* buf, unsigned int write_size)
{
    unsigned int ori_write_size = write_size;
    unsigned int offset = 0;

    write_size /= BLOCK_SIZE;
    if(ori_write_size % BLOCK_SIZE){
        write_size++;
    }

    while (write_lock.test_and_set(std::memory_order_acquire));
    remain_write_req += 1;

    if(file_segment.empty()) {
        offset = 0;
    }
    else
    {
        auto last_segment = file_segment.back();
        offset = last_segment->get_next_offset();
    }

    auto tmp_seg = new class Segment(next_segment_id, offset, write_size);
    next_segment_id++;
    
    ssize_t write_result = _write(tmp_seg, buf, write_size);
    if(write_result < 0)
    {
        write_lock.clear(std::memory_order_release);
        std::cout << "Append error" << std::endl;
        exit(0);
    }
    
    insert_segment(file_segment, tmp_seg);
    size += write_size;

    remain_write_req -= 1;
    write_lock.clear(std::memory_order_release);

    return 0;
}

int Userdata::invalid_segment(class Segment* segment){
    class Interval* tmp_interval;
    auto interval_list = segment->get_interval_list();
    for(auto it = interval_list->begin(); it != interval_list->end(); it++){
        tmp_interval = *it;
        tmp_interval->invalid();
    }
    interval_list->clear();
    delete interval_list;

    return 0;
}

int Userdata::Delete(){
    class Segment* segment;
    unsigned int segment_size = 0;
    ssize_t r = 0;

    while (write_lock.test_and_set(std::memory_order_acquire));
    remain_write_req += 1;

    for (auto iter = file_segment.begin(); iter != file_segment.end(); iter++){
        segment = *iter;
        invalid_segment(segment);
        delete segment;
    }

    remain_write_req -= 1;
    write_lock.clear(std::memory_order_release);

    return 0;
}
/*
int Userdata::Sync(unsigned int data_size){
    unsigned int written_size = 0;
    class Segment* segment;

    data_size/=BLOCK_SIZE;

    while(data_size > written_size){
        written_size = 0;
        write_lock.lock();
        for (auto iter = file_segment.begin(); iter != file_segment.end(); iter++){
            segment = *iter;
            written_size += segment->get_size();
        }
        write_lock.unlock();
    }

    return 0;
}*/
int Userdata::Sync(){
    unsigned int written_size = 0;
    class Segment* segment;

    while (write_lock.test_and_set(std::memory_order_acquire));
    while(remain_write_req != 0);
    write_lock.clear(std::memory_order_release);

    return 0;
}
Userdata::~Userdata()
{
    //Delete Radixtree Recursive
}
