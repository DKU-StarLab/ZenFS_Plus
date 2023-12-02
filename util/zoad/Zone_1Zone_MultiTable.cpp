//
// Created by root on 21. 1. 4..
//
//#include <pthread.h>
#include <iostream>
#include <thread>
#include "Zone.h"
#include <algorithm>
#include <numeric>

bool Zone::full(){
    return zns_full_zone(number);
}
unsigned int Zone::get_zone_number(){
    return number;
}
unsigned int Zone::get_wp(){
    return zns_get_wp(number);
}
unsigned int Zone::get_free_size(){
    auto start_lba = zns_get_start_lba(number);
    auto wp = zns_get_wp(number);
    auto zone_size = zns_get_zone_size();

    //std::cout << start_lba << std::endl;
    //std::cout << wp << std::endl;
    //std::cout << zone_size << std::endl;

    //return (zone_size - (wp - start_lba)) * BLOCK_SIZE;
    return zone_size - (wp - start_lba);
}

int Zone::insert_interval(Interval* interval){
    interval_tree.push_back(interval);
    return 0;
}
/*
void* thread_read(void *arg){
    int i = 0;
    unsigned int num = *(unsigned int*)arg;

    //zns_set_zone(num, MAN_OPEN);
    while(i < ((zns_info -> zonef.zsze * BLOCK_SIZE)/(_192KB))){
        zns_read(test_data, IO_SIZE, num, (i * _192KB)/BLOCK_SIZE);
        i++;
    }
    //zns_set_zone(num, MAN_CLOSE);

    pthread_exit((void *) nullptr);
}
*/

void* thread_write(class zone_request* arg){
    class zone_request* req = arg;
    const char* buf = req->w_buf;
    std::vector<class Interval*>::iterator interval_it = req->interval_it;
    std::vector<unsigned int>::iterator write_size_it = req->write_size_it;
    std::vector<unsigned int>::iterator write_min_size_it = req->sorted_write_size_it;
    unsigned int write_zone_cnt = req->write_zone_cnt;
    unsigned int izg_cnt = req->izg_cnt;
    unsigned int total_write_size = req->total_write_size;
    unsigned int total_written_size = 0;
    class Interval* tmp_interval;
    unsigned int cnt = 0;

    total_write_size *= IO_SIZE;
    if(total_write_size <= 0){
        //pthread_exit((void *) nullptr);
        return NULL;
    }

    for(int i = 0; i < write_zone_cnt; i++) {
        tmp_interval = *interval_it;
	//std::cout << "write zone_number : " << tmp_interval -> zone_number << std::endl;
        zns_set_zone(tmp_interval->zone_number, MAN_OPEN);
        interval_it++;
    }

    interval_it -= write_zone_cnt;
    while (total_written_size < total_write_size)
    {
        for (int i = 0; i < write_zone_cnt; i++) {
            if(*write_size_it > 0) {
                //std::cout << tmp_interval->zone_number << std::endl;
                tmp_interval = *interval_it;
                zns_write(buf, IO_SIZE, tmp_interval->zone_number);
		//std::cout << tmp_interval->zone_number << std::endl;
                *write_size_it -= 1;
                buf += IO_SIZE;
                total_written_size += IO_SIZE;
		
                if(*write_size_it <= 0){
                    tmp_interval->end = zns_get_wp(tmp_interval->zone_number);
                }
            }
            interval_it++;
            write_size_it++;
        }
	cnt++;
	while(cnt == *write_min_size_it){
		izg_cnt--;
		write_min_size_it++;
	}
        buf += IO_SIZE * (izg_cnt - write_zone_cnt);
        interval_it -= write_zone_cnt;
        write_size_it -= write_zone_cnt;
    }

    for(int i = 0; i < req->write_zone_cnt; i++) {
        tmp_interval = *interval_it;
        zns_set_zone(tmp_interval->zone_number, MAN_CLOSE);
        interval_it++;
    }

    //write_cnt++;

    return NULL;
}

void* thread_read(class zone_request* arg){
    class zone_request* req = arg;
    char* buf = req->r_buf;
    std::vector<class Interval*>::iterator interval_it = req->interval_it;
    std::vector<unsigned int>::iterator read_min_size_it = req->write_size_it;
    unsigned int read_zone_cnt = req->write_zone_cnt;
    unsigned int total_read_size = req->total_write_size;
    unsigned int total_read_size2 = 0;
    class Interval* tmp_interval;
    unsigned int start;
    unsigned int end;
    char* tmp_buf = buf;
    unsigned int cnt = 0;
    unsigned int cnt2 = 0;

    total_read_size *= IO_SIZE;
    if(total_read_size <= 0){
        return NULL;
    }

    tmp_interval = *interval_it;
    start = tmp_interval->start;
    end = tmp_interval->end;

    while (total_read_size2 < total_read_size)
    {
	    while(cnt == *read_min_size_it){
		    read_zone_cnt--;
		    read_min_size_it++;
		    cnt2++;
		    if(cnt2 >= req->write_zone_cnt)
			    break;
	    }

	    if(start < end) {
                    zns_read(buf, IO_SIZE, tmp_interval->zone_number, start);
                    buf += IO_SIZE * read_zone_cnt;
                    total_read_size2 += IO_SIZE;
                    start += IO_BLOCKS;
		    cnt++;
            }
    }

    //std::cout << "thread_read total_read_size: " << total_read_size2 << std::endl;
    //std::cout << cnt << std::endl;

    return NULL;
}

void* thread_read_random(class zone_request* arg){
    class zone_request* req = arg;
    char* buf = req->r_buf;
    std::vector<class Interval*>::iterator interval_it = req->interval_it;
    std::vector<unsigned int>::iterator read_min_size_it = req->sorted_write_size_it;
    unsigned int read_zone_cnt = req->write_zone_cnt;
    unsigned int total_read_size = req->total_write_size;
    unsigned int total_read_size2 = 0;
    unsigned int actual_read_size = req->actual_io_size;
    unsigned int offset = req->read_offset;
    class Interval* tmp_interval;
    unsigned int start;
    unsigned int end;
    char* tmp_buf = buf;
    unsigned int cnt = 0;
    unsigned int cnt2 = 0;
    unsigned int actual_read_cnt = 0;

    total_read_size *= IO_SIZE;
    if(total_read_size <= 0){
        return NULL;
    }

    tmp_interval = *interval_it;
    start = tmp_interval->start;
    end = tmp_interval->end;
    
    while (total_read_size2 < total_read_size and actual_read_cnt < actual_read_size)
    {
	while(cnt == *read_min_size_it){
	    read_zone_cnt--;
	    read_min_size_it++;
	    cnt2++;
	    if(cnt2 >= req->write_zone_cnt)
		    break;
	}
	
        if(start < end) {
            if(cnt >= offset){
                zns_read(buf, IO_SIZE, tmp_interval->zone_number, start);
		buf += IO_SIZE * read_zone_cnt;
		actual_read_cnt++;
            }
            total_read_size2 += IO_SIZE;
            start += IO_BLOCKS;
            cnt++;
        }
    }

    return NULL;
}

int izg_write(const char* buf, unsigned int izg_cnt, unsigned int total_target_zone, std::vector<class Interval*>* interval_list, std::vector<unsigned int>* write_size_vector, unsigned int write_blocks) {
    //struct timeval sstart = {};
    //struct timeval eend = {};
    //float ttime = 0;
    unsigned int thread_num = (izg_cnt / THREAD_PER_IZG);
    if(izg_cnt % THREAD_PER_IZG){
        thread_num++;
    }
    unsigned int izg_write = 0;
    std::thread thread[thread_num];
    int status = 0;
    unsigned int last_izg_each_zone_write_cnt;
    std::vector<unsigned int>::iterator write_size_it = write_size_vector->begin();
    std::vector<unsigned int> sorted_write_size_vec(write_size_vector->size());
    std::vector<unsigned int>::iterator sorted_write_size_it;
    std::vector<class Interval*>::iterator interval_it;
    unsigned int interval_list_size = 0;
    unsigned int write_zone_cnt = 0;
    unsigned int thread_write_size = 0;
    unsigned int total_thread_write_size = 0;
    class zone_request* tmp_req[thread_num];
    const char* tmp_buf;

    for(int i = 0; i < thread_num; i++){
        tmp_req[i]= new class zone_request();
    }

    izg_write = total_target_zone/izg_cnt;
    if(total_target_zone % izg_cnt){
        izg_write++;
    }

    for (int i = 0; i < izg_write; i++) {
        if(i < izg_write - 1) {
            for (int j = 0; j < izg_cnt; j++) {
                write_blocks -= *write_size_it;
                *write_size_it = *write_size_it/IO_BLOCKS;
                write_size_it++;
            }
        }
        else{
            last_izg_each_zone_write_cnt = write_blocks / (izg_cnt * IO_BLOCKS);
            for(int j = 0; j < izg_cnt; j++){
                *write_size_it = last_izg_each_zone_write_cnt;
                write_size_it++;
            }
            for(int j = 0; j < izg_cnt; j++){
                write_size_it--;
            }

            last_izg_each_zone_write_cnt = (write_blocks % (izg_cnt * IO_BLOCKS))/IO_BLOCKS;
            for(int j = 0; j < last_izg_each_zone_write_cnt; j++){
                *write_size_it += 1;
                write_size_it++;
            }
        }
    }

    interval_it = interval_list->begin();
    interval_list_size = interval_list->size();
    write_size_it = write_size_vector->begin();
    //gettimeofday(&sstart, NULL);
    
    for (int i = 0; i < izg_write; i++) {
	tmp_buf = buf;
	total_thread_write_size = 0;
	
	if(izg_cnt < interval_list_size)
    		partial_sort_copy(write_size_it, write_size_it + interval_list_size, sorted_write_size_vec.begin(), sorted_write_size_vec.end());
	else
		partial_sort_copy(write_size_it, write_size_it + izg_cnt, sorted_write_size_vec.begin(), sorted_write_size_vec.end());
	
	sorted_write_size_it = sorted_write_size_vec.begin();
        
	for (int j = 0; j < thread_num; j++) {
            thread_write_size = 0;
            if(izg_cnt < interval_list_size){
                write_zone_cnt = izg_cnt;
            }
            else{
                write_zone_cnt = interval_list_size;
            }
            if(THREAD_PER_IZG < write_zone_cnt){
                if(THREAD_PER_IZG * (j + 1) > izg_cnt){
                    write_zone_cnt = izg_cnt - THREAD_PER_IZG;
                }
                else{
                    write_zone_cnt = THREAD_PER_IZG;
                }
            }

            for(int k = 0; k < write_zone_cnt; k++){
                thread_write_size += *write_size_it;
                write_size_it++;
            }
	    write_size_it -= write_zone_cnt;
	    total_thread_write_size += thread_write_size;

            tmp_req[j]->set_request(tmp_buf, interval_it, write_size_it, write_zone_cnt, thread_write_size, sorted_write_size_it, sorted_write_size_vec.size());
            thread[j] = std::thread(thread_write, tmp_req[j]);
            //buf += thread_write_size * IO_SIZE;
	    tmp_buf += write_zone_cnt * IO_SIZE;
	    interval_it += write_zone_cnt;
            write_size_it += write_zone_cnt;
            interval_list_size -= write_zone_cnt;
        }

        for (int k = 0; k < thread_num; k++) {
            thread[k].join();
        }
	sorted_write_size_vec.clear();
	buf += total_thread_write_size * IO_SIZE;
    }
    //gettimeofday(&eend, NULL);
    //ttime += (eend.tv_sec - sstart.tv_sec) + ((eend.tv_usec - sstart.tv_usec) * 0.000001);
    //std::cout << ttime << std::endl;
    for(int i = 0; i < thread_num; i++){
        delete tmp_req[i];
    }

    return 0;
}

int izg_read(char* buf, unsigned int izg_cnt, unsigned int total_target_zone, std::vector<class Interval*>* interval_list, unsigned int read_size){
    //struct timeval sstart = {};
    //struct timeval eend = {};
    //float ttime = 0;
    /*
    unsigned int thread_num = (izg_cnt / THREAD_PER_IZG);
    if(izg_cnt % THREAD_PER_IZG){
        thread_num++;
    }
    */
    unsigned int thread_num = 0;
    if(total_target_zone >= izg_cnt)
	    thread_num = izg_cnt;
    else
	    thread_num = total_target_zone;
    unsigned int izg_read = 0;
    std::thread thread[thread_num];
    int status = 0;
    std::vector<class Interval*>::iterator interval_it;
    std::vector<unsigned int> thread_read_size_vec;
    std::vector<unsigned int> thread_read_min_size_vec;
    std::vector<unsigned int>::iterator thread_read_min_size_it;
    unsigned int interval_list_size = 0;
    unsigned int read_zone_cnt = 0;
    unsigned int total_thread_read_size = 0;
    unsigned int thread_read_size = 0;
    class zone_request* tmp_req[thread_num];
    class Interval* tmp_interval;
    char* tmp_buf = buf;

    for(int i = 0; i < thread_num; i++){
        tmp_req[i]= new class zone_request();
    }

    izg_read = total_target_zone/izg_cnt;
    if(total_target_zone % izg_cnt or izg_read == 0){
        izg_read++;
    }

    interval_it = interval_list->begin();
    interval_list_size = interval_list->size();
    
    //gettimeofday(&sstart, NULL);
    for (int i = 0; i < izg_read; i++) {
	total_thread_read_size = 0;
	
        if(izg_cnt < interval_list_size){
            read_zone_cnt = izg_cnt;
        }
        else{
            read_zone_cnt = interval_list_size;
        }
	
        for(int k = 0; k < read_zone_cnt; k++, interval_it++){
            tmp_interval = *interval_it;
            thread_read_size = tmp_interval->get_size();
	    total_thread_read_size += thread_read_size;
	    thread_read_size /= IO_BLOCKS;
	    //std::cout << thread_read_size << std::endl;
	    thread_read_size_vec.push_back(thread_read_size);
	    thread_read_min_size_vec.push_back(thread_read_size);
	}
	sort(thread_read_min_size_vec.begin(), thread_read_min_size_vec.end());
	thread_read_min_size_it = thread_read_min_size_vec.begin();
	interval_it -= read_zone_cnt;
        
	for(int k = 0; k < read_zone_cnt; k++, interval_it++){
            tmp_req[k]->set_request(tmp_buf + (k * IO_SIZE), interval_it, read_zone_cnt, thread_read_size_vec[k], thread_read_min_size_it);
            thread[k] = std::thread(thread_read, tmp_req[k]);
	}

        tmp_buf += total_thread_read_size * BLOCK_SIZE;
        interval_list_size -= read_zone_cnt;
        
	for (int k = 0; k < read_zone_cnt; k++) {
            thread[k].join();
        }
	thread_read_min_size_vec.clear();
	thread_read_size_vec.clear();
    }
    //gettimeofday(&eend, NULL);
    //ttime += (eend.tv_sec - sstart.tv_sec) + ((eend.tv_usec - sstart.tv_usec) * 0.000001);
    //std::cout << "izg_read : " << ttime << std::endl;
    //std::cout << "izg_read read size: " << read_size * 512<< std::endl;
    
    for(int i = 0; i < thread_num; i++){
        delete tmp_req[i];
    }

    return 0;
}

int izg_read(char* buf, unsigned int izg_cnt, unsigned int total_target_zone, std::vector<class Interval*>* interval_list, unsigned int offset, unsigned int read_size){
    //struct timeval sstart = {};
    //struct timeval eend = {};
    //float ttime = 0;
    /*
    unsigned int thread_num = (izg_cnt / THREAD_PER_IZG);
    if(izg_cnt % THREAD_PER_IZG){
        thread_num++;
    }
    */
    unsigned int thread_num = 0;
    if(total_target_zone >= izg_cnt)
	    thread_num = izg_cnt;
    else
	    thread_num = total_target_zone;
    unsigned int izg_read = 0;
    std::thread thread[thread_num];
    int status = 0;
    std::vector<class Interval*>::iterator interval_it;
    std::vector<unsigned int> thread_read_size_vec;
    unsigned int actual_thread_read_size[izg_cnt];
    std::vector<unsigned int> thread_read_min_size_vec;
    unsigned int thread_offset[izg_cnt] = {0,};
    std::vector<unsigned int>::iterator thread_read_min_size_it;
    unsigned int less_skip_zone_cnt[izg_cnt] = {0,};
    unsigned int offset_min_diff[izg_cnt] = {0,};
    unsigned int min_diff = 99999;
    unsigned int interval_list_size = 0;
    unsigned int read_zone_cnt = 0;
    unsigned int thread_read_size = 0;
    unsigned int total_thread_read_size = 0;
    unsigned int actual_total_thread_read_size = 0;
    unsigned int total_read_size = 0;
    class zone_request* tmp_req[thread_num];
    class Interval* tmp_interval;
    char* tmp_buf = buf;
    unsigned int moved = 0;
   
    for(int i = 0; i < thread_num; i++){
        tmp_req[i]= new class zone_request();
    }

    izg_read = total_target_zone/izg_cnt;
    if(total_target_zone % izg_cnt or izg_read == 0){
        izg_read++;
    }

    interval_it = interval_list->begin();
    interval_list_size = interval_list->size();

    //std::cout << "read_size : " << read_size * 512<< std::endl;
    //std::cout << "read_total_target_zone : " << total_target_zone << std::endl;
    //gettimeofday(&sstart, NULL);
    for (int i = 0; i < izg_read; i++) {
        total_thread_read_size = 0;
        actual_total_thread_read_size = 0;

	if(izg_cnt < interval_list_size){
            read_zone_cnt = izg_cnt;
        }
        else{
            read_zone_cnt = interval_list_size;
        }

        for(int k = 0; k < read_zone_cnt; k++, interval_it++){
            tmp_interval = *interval_it;
	    thread_read_size = tmp_interval->get_size();
	    total_thread_read_size += thread_read_size;
	    actual_total_thread_read_size += thread_read_size; 
	    total_read_size += thread_read_size;
	    //std::cout << total_read_size * 512<< std::endl;
	    thread_read_size /= IO_BLOCKS;
	    thread_read_size_vec.push_back(thread_read_size);
        }

   	interval_it -= read_zone_cnt;

	if(offset < total_read_size and offset > 0){
	    unsigned int tmp_offset = offset/IO_BLOCKS;
	    unsigned int offset_per_zone = 0;
	    unsigned int remain_offset = 0;
	    unsigned int full_cnt = 0;
	    unsigned int min_size = *std::min_element(thread_read_size_vec.begin(), thread_read_size_vec.end());
	    
	    if(min_size * read_zone_cnt < tmp_offset){
	   	 for(int k = 0; k < read_zone_cnt; k++){
			thread_offset[k] += min_size;
			//thread_read_size_vec[k] -= min_size;
			if(thread_read_size_vec[k] - min_size == 0){
				full_cnt++;
			}
		 }
		 tmp_offset -= min_size * read_zone_cnt;
	    }

		
	    offset_per_zone = tmp_offset/(read_zone_cnt - full_cnt);
	    remain_offset = tmp_offset%(read_zone_cnt - full_cnt);
	    for(int k = 0; k < read_zone_cnt; k++){
		//if(thread_read_size_vec[k] - min_size != 0){				// if off --> 1Zone-Multi_Table
		   thread_offset[k] += offset_per_zone;
		   //thread_read_size_vec[k] -= offset_per_zone;
		   if(remain_offset > 0){
		   	thread_offset[k] += 1;
			//thread_read_size_vec[k] -= 1;
			remain_offset--;
		   }
		//}

	        actual_thread_read_size[k] = thread_read_size_vec[k] - thread_offset[k];
		actual_total_thread_read_size -= (thread_offset[k] * IO_BLOCKS);
	    	//thread_read_min_size_vec.push_back(thread_read_size_vec[k]);
	    }

	    for(int i = 0; i < read_zone_cnt; i++){
		    min_diff = 99999;
		    for(int k = i; k < read_zone_cnt; k++){	
			    if(thread_offset[i] > thread_offset[k]){
			    	less_skip_zone_cnt[i]++;
				if(thread_offset[i] - thread_offset[k] < min_diff)
					min_diff = thread_offset[i] - thread_offset[k];
			    }
		    }
		    offset_min_diff[i] = min_diff;
	    }
	}

	if(actual_total_thread_read_size > read_size){
	    unsigned int read_io = read_size/IO_BLOCKS;
	    if(read_io == 0 or read_size%IO_BLOCKS)
		    read_io++;
	    unsigned int all_zone_read_io = read_io/read_zone_cnt;
	    unsigned int remain_read_io = read_io - all_zone_read_io;
	    actual_total_thread_read_size = 0;

	    for(int k = 0; k < read_zone_cnt; k++){
		actual_thread_read_size[k] = all_zone_read_io;
		if(remain_read_io > 0 and thread_read_size_vec[k] > 0){
		    actual_thread_read_size[k] += 1;
		    remain_read_io--;
		}

		actual_total_thread_read_size += actual_thread_read_size[k] * IO_BLOCKS;
		thread_read_min_size_vec.push_back(thread_read_size_vec[k]);
	    }
	}
	else{
	    for(int k = 0; k < read_zone_cnt; k++){
	       actual_thread_read_size[k] = thread_read_size_vec[k];
 	       thread_read_min_size_vec.push_back(thread_read_size_vec[k]);
	    }
	}

	if(offset < total_read_size) {
	    sort(thread_read_min_size_vec.begin(), thread_read_min_size_vec.end());
	    thread_read_min_size_it = thread_read_min_size_vec.begin();

	    //std::cout << "Thread cnt : " << thread_num << std::endl;
	    for(int k = 0; k < read_zone_cnt; k++, interval_it++){
		tmp_interval = *interval_it;
		//std::cout << "read zone_number : " << tmp_interval -> zone_number << std::endl;
		//std::cout << "zone_offset : " << thread_offset[k] << std::endl;
		//std::cout << "zone_read_size : " << thread_read_size_vec[k] << std::endl;
		//std::cout << "zone_actual_read_size : " << actual_thread_read_size[k] << std::endl;

		if(less_skip_zone_cnt[k] > 0){
    	    	    tmp_req[k]->set_request(tmp_buf + ((offset_min_diff[k] * less_skip_zone_cnt[k] + moved) * IO_SIZE), interval_it, read_zone_cnt, thread_offset[k], thread_read_size_vec[k], thread_read_min_size_it, actual_thread_read_size[k]);
		    moved++;}
		else{
    	    	    tmp_req[k]->set_request(tmp_buf + ((k - moved) * IO_SIZE), interval_it, read_zone_cnt, thread_offset[k], thread_read_size_vec[k], thread_read_min_size_it, actual_thread_read_size[k]);
		}
	    	
		thread[k] = std::thread(thread_read_random, tmp_req[k]);
	    }
	
	    for (int k = 0; k < read_zone_cnt; k++) {
        		thread[k].join();
            }


	    //tmp_buf += (total_thread_read_size-offset) * BLOCK_SIZE;
	    tmp_buf += actual_total_thread_read_size * BLOCK_SIZE;
	    if(offset > 0){
            	offset = 0;
		std::fill_n(thread_offset,izg_cnt,0);
		std::fill_n(offset_min_diff,izg_cnt,99999);
		std::fill_n(less_skip_zone_cnt,izg_cnt,0);
		moved = 0;
	    }

	    read_size -= actual_total_thread_read_size;
        }
	else
	    interval_it += read_zone_cnt;

        interval_list_size -= read_zone_cnt;
	thread_read_min_size_vec.clear();
	thread_read_size_vec.clear();

	if(read_size <= 0)
		break;
    }

    //gettimeofday(&eend, NULL);
    //ttime += (eend.tv_sec - sstart.tv_sec) + ((eend.tv_usec - sstart.tv_usec) * 0.000001);
    //std::cout << ttime << std::endl;
    for(int i = 0; i < thread_num; i++){
        delete tmp_req[i];
    }

    return 0;
}
