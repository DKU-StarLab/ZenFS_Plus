#include <iostream>
#include <string.h>
#include <limits>
#include <string>
//#include <list>
//#include <vector>
//#include <algorithm>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <queue>
#include <set>
#include <thread>
#include <type_traits>

#include "leveldb/env.h"
#include "leveldb/slice.h"
#include "leveldb/status.h"
#include "port/port.h"
#include "port/thread_annotations.h"
#include "util/posix_logger.h"
#include "util/env_posix_test_helper.h"
#include "util/mutexlock.h"
#include "zoad/zoad.h"

#define MAX_FLAG 2 //0 : Flush/log/Meta, 1 : Compaction 
#define BUF_SIZE 1

namespace leveldb {
    unsigned int buf_size[MAX_FLAG];

    namespace {

        using namespace std;

        static Status PosixError(const std::string& context, int err_number) {
            if (err_number == ENOENT) {
                return Status::NotFound(context, strerror(err_number));
            } else {
                return Status::IOError(context, strerror(err_number));
            }
        }

        class ZoadFile{
        private:
            Status WriteUnbuffered(const char* data, size_t wsize)
            {
		size_t copy_size = 0;
		while(wsize > 0){
		    copy_size = std::min(wsize, buf_size[flag] - buf_pos_);
                    memcpy(buf_ + buf_pos_, data, copy_size);
                    data += copy_size;
		    wsize -= copy_size;
                    buf_pos_ += copy_size;   
		    
		    if(buf_pos_ >= buf_size[flag]){
			Flush_file_buffer();
		    }
		}

                return Status::OK();
            }

	    Status Flush_file_buffer(){
		size_t wsize = buf_pos_;
		if(wsize >= buf_size[flag]){
		   //std::cout << flush_cnt << " " << name << " " << buf_pos_<< " Flush_fill_buffer() : " << buf_[12410] << std::endl;
		/*for(unsigned int k = 0; k < IO_SIZE; k++){
			if(buf_[k] == 0x00 or buf_[k] == 0x01){
				std::cout << flush_cnt << " W_Index : " << k << std::endl;
				break;
			}
		}*/
		    //flush_cnt++;

                    if(wsize % IO_SIZE)
	            {	
                        buf_pos_ = wsize % IO_SIZE;
                        wsize = wsize - (wsize % IO_SIZE);
			::write(fd, buf_, wsize);
                        memcpy(buf_, buf_ + wsize, buf_pos_);
                    }
		    else
		    {
			buf_pos_ = 0;
			::write(fd, buf_, wsize);
		    }
		}

                return Status::OK();
	    }

            Status SyncDirIfManifest() {
                Status status;
                if (!is_manifest_)
                {
                    return status;
                }

		if(file_size >= buf_size[flag])
			::fdatasync(fd);

                return status;
            }

            static bool IsManifest(const std::string& filename) {
                return Basename(filename).starts_with("MANIFEST");
            }

            static Slice Basename(const std::string& filename)
            {
                std::string::size_type separator_pos = filename.rfind('/');
                if (separator_pos == std::string::npos) {
                    return Slice(filename);
                }
                assert(filename.find('/', separator_pos + 1) == std::string::npos);

                return Slice(filename.data() + separator_pos + 1,
                             filename.length() - separator_pos - 1);
            }

            // No copying allowed.
            ZoadFile(const ZoadFile&);
            void operator=(const ZoadFile&);

            port::Mutex refs_mutex_;
            int refs_ GUARDED_BY(refs_mutex_);

            // The following fields are not protected by any mutex. They are only mutable
            // while the file is being written, and concurrent access is not allowed
            // to writable files.
            string name;
            int fd;
	    unsigned int flag;
            uint64_t level;
            uint64_t file_size;
            char *buf_;
            size_t buf_pos_;
            bool is_manifest_;  // True if the file's name starts with MANIFEST.
	    char *read_buf_;
            size_t read_buf_offset_;
            size_t read_size;
            size_t buf_read_size;
	    bool use_read_buf;
        public:
            ZoadFile(unsigned int flag_, uint64_t file_level, string filename, int fd_){
                name = filename;
		fd = fd_;
		flag = flag_;
                level = file_level;
                file_size = 0;
                buf_pos_ = 0;
                is_manifest_ = IsManifest(filename);
		//unsigned int temp = level;
		buf_ = new char[buf_size[flag]];
		read_buf_ = new char[IO_SIZE];
		read_buf_offset_ = 0;
		read_size = 0;
		buf_read_size = 0;
		use_read_buf = false;
            }
            // Increase the reference count.
            void Ref()
            {
                MutexLock lock(&refs_mutex_);
                ++refs_;
            }

            // Decrease the reference count. Delete if this is the last reference.
            void Unref()
            {
                bool do_delete = false;
                {
                    MutexLock lock(&refs_mutex_);
                    --refs_;
                    //HOON
                    //assert(refs_ >= 0);
                    if (refs_ <= 0)
                    {
                        do_delete = true;
                    }
                }

                if (do_delete)
                {
		    //delete buf_;
                    //HOON
                    //Delete Zone's Interval
                    //Delete Interval list
                    //delete this;
                }
            }

            uint64_t Size() { return file_size; }

	    int get_fd() {return fd;}

	    string get_name() {return name;}

	    void Rename(string target) {
		    //std::cout << "name : " <<  name << std::endl;
		    name = target;
		    //std::cout << "buf_pos_ : " <<  buf_pos_ << std::endl;
	    }

            Status Read(uint64_t offset, size_t n, Slice* result, char* scratch)
            {
                if (offset > file_size)
                {
                    return Status::IOError("Offset greater than file size.");
                }
                
		if (n > file_size - offset)
                    n = file_size - offset;

                if (n == 0) {
                    *result = Slice();
                    return Status::OK();
                }
		
  		//std::cout << "ZFR0" << std::endl;
		if(file_size == buf_pos_){
  		//std::cout << "ZFR0.1" << std::endl;
		    memcpy(scratch, buf_ + offset, n);
  		//std::cout << "ZFR0.2" << std::endl;
		}
		else if(offset >= file_size - buf_pos_){
  		//std::cout << "ZFR0.3" << std::endl;
		    if(offset + n > file_size){
  		        std::cout << "ZoadFile Read : Size Error" << std::endl;
		        exit(0);
  		    }
		    memcpy(scratch, buf_ + offset - (file_size - buf_pos_), n);
		}
		else if(offset + n <= file_size - buf_pos_){
  		//std::cout << "ZFR0.5" << std::endl;
		    if(use_read_buf){
		    if(offset >= read_buf_offset_ and offset + n <= read_buf_offset_ + IO_SIZE){
		        memcpy(scratch, read_buf_ + offset - read_buf_offset_, n);
		    }
		    else if(offset >= read_buf_offset_ + IO_SIZE or offset < read_buf_offset_){
			read_size = n - n%IO_SIZE;
			if(n%IO_SIZE)
			    read_size += IO_SIZE;
			
			pread(fd, scratch, read_size, static_cast<off_t>(offset));

			memcpy(read_buf_, scratch + read_size - IO_SIZE, IO_SIZE);
			read_buf_offset_= offset + read_size - IO_SIZE;
		    }
		    else if(offset >= read_buf_offset_ and  offset < read_buf_offset_ + IO_SIZE and offset + n > read_buf_offset_ + IO_SIZE){
			read_size = n - n%IO_SIZE;
			if(n%IO_SIZE)
			    read_size += IO_SIZE;
 			
			buf_read_size = IO_SIZE - offset%IO_SIZE;
			memcpy(scratch, read_buf_ + offset - read_buf_offset_, buf_read_size);

			pread(fd, scratch + buf_read_size, read_size, static_cast<off_t>(offset + buf_read_size));

			memcpy(read_buf_, scratch + buf_read_size + read_size - IO_SIZE, IO_SIZE);
			read_buf_offset_= offset + buf_read_size + read_size - IO_SIZE;
		    }
		    else{
			std::cout << "WTF" << std::endl;
			exit(0);
		    }
		    }
		    else{
			read_size = n - n%IO_SIZE;
			if(n%IO_SIZE)
			    read_size += IO_SIZE;

			pread(fd, scratch, read_size, static_cast<off_t>(offset));

			memcpy(read_buf_, scratch + read_size - IO_SIZE, IO_SIZE);
			read_buf_offset_= offset + read_size - IO_SIZE;
			use_read_buf = true;
		    }
  		//std::cout << "ZFR0.6" << std::endl;
                }
                else{
  		//std::cout << "ZFR0.7" << std::endl;
		    pread(fd, scratch, file_size - buf_pos_ - offset, static_cast<off_t>(offset));
		    memcpy(scratch + file_size - buf_pos_ - offset, buf_, offset + n - (file_size - buf_pos_));
  		//std::cout << "ZFR0.8" << std::endl;
                }
  		//std::cout << "ZFR1" << std::endl;
                *result = Slice(scratch, n);
  		//std::cout << "ZFR2" << std::endl;
                return Status::OK();
            }

            Status Append(const Slice& data)
            {
		//std::cout << "WA0" << std::endl;
                size_t write_size = data.size();
                const char* write_data = data.data();
                file_size += write_size;
                // Fit as much as possible into buffer.
                size_t copy_size = std::min(write_size, buf_size[flag] - buf_pos_);

  		//std::cout << "WA1" << std::endl;
                memcpy(buf_ + buf_pos_, write_data, copy_size);
  		//std::cout << "WA1.5" << std::endl;
                write_data += copy_size;
                write_size -= copy_size;
                buf_pos_ += copy_size;
  		//std::cout << "WA2" << std::endl;
                if (write_size == 0){
  			//std::cout << "WA2.5" << std::endl;
                    return Status::OK();
		}
  		//std::cout << "WA3" << std::endl;
                // Can't fit in buffer, so need to do at least one write.
                Status status = Flush_file_buffer();
                if (!status.ok())
                    return status;
  		//std::cout << "WA4" << std::endl;
                // Small writes go to buffer, large writes are written directly.
                /*
		if (write_size < buf_size[flag]) {
                    memcpy(buf_, write_data, write_size);
                    buf_pos_ = write_size;
                    return Status::OK();
                }
		*/
  		//std::cout << "WA5" << std::endl;

                return WriteUnbuffered(write_data, write_size);
            }

            Status Sync()
            {
		Status status;
                
		//status = SyncDirIfManifest();
                //if (!status.ok()) {
                //    return status;
                //}

                status = Flush_file_buffer();
		if(file_size >= buf_size[flag])
		    ::fdatasync(fd);

                return status;
            }

	    Status Delete(){
		file_size = 0;
		delete[] buf_;
		buf_pos_ = 0;
                return Status::OK();
            }

            ~ZoadFile()
            {
                //Hoon
            }

            Status FlushBuffer()
            {
                //Status status = WriteUnbuffered(buf_, buf_pos_);
		Status status = Flush_file_buffer();

                return status;
            }

        };

        class SequentialFileImpl : public SequentialFile
        {
        public:
            explicit SequentialFileImpl(ZoadFile* file) : file_(file), pos_(0)
            {
                file_->Ref();
            }

            ~SequentialFileImpl()
            {
                file_->Unref();
            }

            virtual Status Read(size_t n, Slice* result, char* scratch)
            {
  		//std::cout << "ZSAR0" << std::endl;
  		//std::cout << pos_ << std::endl;
  		//std::cout << n << std::endl;
                Status s = file_->Read(pos_, n, result, scratch);
                if (s.ok()) {
	  		//std::cout << "n: " <<  n << std::endl;
			//std::cout << "size: " << result->size() << std::endl;
                    pos_ += result->size();
                }
  		//std::cout << "ZSAR1" << std::endl;
                return s;
            }

            virtual Status Skip(uint64_t n)
            {
	  	std::cout <<"Skip n: " <<  n << std::endl;

                if (pos_ > file_->Size()) {
                    return Status::IOError("pos_ > file_->Size()");
                }
                const uint64_t available = file_->Size() - pos_;
                if (n > available) {
                    n = available;
                }
                pos_ += n;
                return Status::OK();
            }

        private:
            ZoadFile* file_;
            uint64_t pos_;
        };

        class RandomAccessFileImpl : public RandomAccessFile
        {
        public:
            explicit RandomAccessFileImpl(ZoadFile* file) : file_(file)
            {
                file_->Ref();
            }

            ~RandomAccessFileImpl()
            {
                file_->Unref();
            }

            virtual Status Read(uint64_t offset, size_t n, Slice* result, char* scratch) const {
                return file_->Read(offset, n, result, scratch);
            }

        private:
            ZoadFile* file_;
        };

        class WritableFileImpl : public WritableFile
        {
        public:
            WritableFileImpl(ZoadFile* file) : file_(file)
            {
                file_->Ref();
            }

            ~WritableFileImpl()
            {
                file_->Unref();
            }

            virtual Status Append(const Slice& data) {
  		//std::cout << "WA" << std::endl;
                return file_->Append(data);
            }
            virtual Status Close()
            {
		Status status;
		status = file_->FlushBuffer();
    		const int close_result = ::close(file_->get_fd());
    		if (close_result < 0 && status.ok()) {
      		    status = PosixError(file_ -> get_name(), errno);
    		}
		

                return status;
            }
            virtual Status Flush() override {
                return file_->FlushBuffer();
            }

            virtual Status Sync()
            {
                return file_->Sync();
            }

            static std::string Dirname(const std::string& filename)
            {
                std::string::size_type separator_pos = filename.rfind('/');
                if (separator_pos == std::string::npos) {
                    return std::string(".");
                }
                assert(filename.find('/', separator_pos + 1) == std::string::npos);

                return filename.substr(0, separator_pos);
            }

        private:
            ZoadFile* file_;
        };

        class NoOpLogger : public Logger
        {
        public:
            virtual void Logv(const char* format, va_list ap) { }
        };

        class ZoadFileLock : public FileLock
        {
        public:
            std::string name_;
        };

        class ZoadLockTable
        {
        private:
            port::Mutex mu_;
            std::set<std::string> locked_files_ GUARDED_BY(mu_);
        public:
            bool Insert(const std::string& fname) LOCKS_EXCLUDED(mu_) {
                    mu_.Lock();
                    bool succeeded = locked_files_.insert(fname).second;
                    mu_.Unlock();
                    return succeeded;
            }
            void Remove(const std::string& fname) LOCKS_EXCLUDED(mu_) {
                    mu_.Lock();
                    locked_files_.erase(fname);
                    mu_.Unlock();
            }
        };

        class ZoadEnv : public Env //Level Issue
        {
        public:
            ZoadEnv();
            virtual ~ZoadEnv()
            {
            }

            // Partial implementation of the Env interface.
            virtual Status NewSequentialFile(const std::string& fname, SequentialFile** result)
            {
    		//std::cout << "SA" << std::endl;
		int fd = open(fname.c_str(), O_RDONLY);
                
		MutexLock lock(&mutex_);
                if (file_map_.find(fname) == file_map_.end()) {
                    *result = nullptr;
                    return Status::IOError(fname, "File not found");
                }
                *result = new SequentialFileImpl(file_map_[fname]);
    		//std::cout << "SA2" << std::endl;
                return Status::OK();
            }

            virtual Status NewRandomAccessFile(const std::string& fname, RandomAccessFile** result)
            {
		int fd = open(fname.c_str(), O_RDONLY);
                MutexLock lock(&mutex_);
    		//std::cout << "RA" << std::endl;
                if (file_map_.find(fname) == file_map_.end()) {
                    *result = nullptr;
                    return Status::IOError(fname, "File not found");
                }

                *result = new RandomAccessFileImpl(file_map_[fname]);
 
    		//std::cout << "RA2" << std::endl;
                return Status::OK();
            }

            virtual Status NewWritableFile(const std::string& fname, unsigned int level, unsigned int flag, WritableFile** result)
            {
    		//std::cout << "ZW" << std::endl;
		//int fd = open(fname.c_str(), O_TRUNC | O_WRONLY | O_CREAT | O_SYNC, 0644);
		int fd = open(fname.c_str(), O_TRUNC | O_WRONLY | O_CREAT, 0644);
                MutexLock lock(&mutex_);
                if (file_map_.find(fname) != file_map_.end()) {
                    DeleteFileInternal(fname);
                }

		ZoadFile* file = new ZoadFile(0, level, fname, fd);
                file_map_[fname] = file;

                *result = new WritableFileImpl(file);
    		//std::cout << "ZW2" << std::endl;
                return Status::OK();
            }

            virtual Status NewAppendableFile(const std::string& fname, unsigned int level, unsigned int flag, WritableFile** result)
            {
    		//std::cout << "ZA" << std::endl;
		//int fd = open(fname.c_str(), O_APPEND | O_WRONLY | O_CREAT| O_SYNC, 0644);
		int fd = open(fname.c_str(), O_APPEND | O_WRONLY | O_CREAT, 0644);
		MutexLock lock(&mutex_);
                ZoadFile** sptr = &file_map_[fname];
                ZoadFile* file = *sptr;
                if (file == nullptr) {
                    file = new ZoadFile(0, level, fname, fd);
                    file_map_[fname] = file;
                }

                *result = new WritableFileImpl(file);
    		//std::cout << "ZA2" << std::endl;
                return Status::OK();
            }

            virtual bool FileExists(const std::string& fname)
            {
		return access(fname.c_str(), F_OK) == 0;
            }

            virtual Status GetChildren(const std::string& dir, std::vector<std::string>* result)
            {
    		result->clear();
		DIR* d = opendir(dir.c_str());
		if (d == nullptr) {
		      return PosixError(dir, errno);
    		}
		struct dirent* entry;
	    	while ((entry = readdir(d)) != nullptr) {
	        result->push_back(entry->d_name);
    		}
		closedir(d);

                return Status::OK();
            }

            void DeleteFileInternal(const std::string& fname) EXCLUSIVE_LOCKS_REQUIRED(mutex_){
        	if (file_map_.find(fname) == file_map_.end()) {
                        return;
                }
                    file_map_[fname]->Delete();
                    file_map_[fname]->Unref();
                    file_map_.erase(fname);

		unlink(fname.c_str());
            }
	    virtual Status DeleteFile(const std::string& fname) {
    	    	Status result;
 		MutexLock lock(&mutex_);
                if (file_map_.find(fname) == file_map_.end())
                {
                    return Status::IOError(fname, "File not found");
                }

                DeleteFileInternal(fname);

		if (unlink(fname.c_str()) != 0) {
		      result = PosixError(fname, errno);
    	    	}
		return result;
	    }

	    virtual Status CreateDir(const std::string& name) {
		Status result;
	        if (mkdir(name.c_str(), 0755) != 0) {
	        result = PosixError(name, errno);
    		}
	        return result;
  	    }

	    virtual Status DeleteDir(const std::string& name) {
		Status result;
    		if (rmdir(name.c_str()) != 0) {
      		result = PosixError(name, errno);
    		}
    		return result;
	    }

  	    virtual Status GetFileSize(const std::string& fname, uint64_t* size) {
                MutexLock lock(&mutex_);
                if (file_map_.find(fname) == file_map_.end())
                {
                    return Status::IOError(fname, "File not found");
                }

                *size = file_map_[fname]->Size();
                return Status::OK();
  	    }

	    virtual Status RenameFile(const std::string& src, const std::string& target) {
    		Status result;
                MutexLock lock(&mutex_);
                if (file_map_.find(src) == file_map_.end()) {
                    return Status::IOError(src, "File not found");
                }

                file_map_[src]->Rename(target);
                file_map_[target] = file_map_[src];
                file_map_.erase(src);

    		if (rename(src.c_str(), target.c_str()) != 0) {
			std::cout << src << std::endl;
			std::cout << target << std::endl;
		    std::cout << "KoKo" << std::endl;
      		    result = PosixError(src, errno);
    		}
		return result;
	    }

            virtual Status LockFile(const std::string& fname, FileLock** lock)
            {
                *lock = nullptr;
                Status result;
                /*
                   int fd = open(fname.c_str(), O_RDWR | O_CREAT, 0644);
                   if (fd < 0) {
                   result = PosixError(fname, errno);
                   } else
                   */
                if (!locks_.Insert(fname))
                {
                    result = Status::IOError("lock " + fname, "already held by process");
                }
                    /*
                       else if (LockOrUnlock(true) == -1)
                       {
                       result = PosixError("lock " + fname, errno);
                       locks_.Remove(fname);
                       }
                       */
                else
                {
                    ZoadFileLock* my_lock = new ZoadFileLock;
                    my_lock->name_ = fname;
                    *lock = my_lock;
                }

                return result;
            }

            virtual Status UnlockFile(FileLock* lock)
            {
                ZoadFileLock* my_lock = reinterpret_cast<ZoadFileLock*>(lock);
                Status result;
                /*
                   if (LockOrUnlock(false) == -1) {
                   result = PosixError("unlock", errno);
                   }
                   */
                locks_.Remove(my_lock->name_);

                delete my_lock;
                return result;
            }

            virtual void Schedule(void (*background_work_function)(void* background_work_arg), void* background_work_arg)
            {
                background_work_mutex_.Lock();

                // Start the background thread, if we haven't done so already.
                if (!started_background_thread_)
                {
                    started_background_thread_ = true;
                    std::thread background_thread(ZoadEnv::BackgroundThreadEntryPoint, this);
                    background_thread.detach();
                }

                // If the queue is empty, the background thread may be waiting for work.
                if (background_work_queue_.empty())
                {
                    background_work_cv_.Signal();
                }

                background_work_queue_.emplace(background_work_function, background_work_arg);
                background_work_mutex_.Unlock();
            }

            virtual void StartThread(void (*thread_main)(void* thread_main_arg), void* thread_main_arg)
            {
                std::thread new_thread(thread_main, thread_main_arg);
                new_thread.detach();
            }

            virtual Status GetTestDirectory(std::string* path)
            {
                *path = "/test";
                return Status::OK();
            }

            virtual Status NewLogger(const std::string& fname, Logger** result)
            {
                *result = new NoOpLogger;
                return Status::OK();
            }

            virtual uint64_t NowMicros()
            {
                struct timeval tv;
                gettimeofday(&tv, nullptr);
                return static_cast<uint64_t>(tv.tv_sec) * 1000000 + tv.tv_usec;
            }

            virtual void SleepForMicroseconds(int micros)
            {
                usleep(micros);
            }

        private:
            void BackgroundThreadMain()
            {
                while (true)
                {
                    background_work_mutex_.Lock();

                    // Wait until there is work to be done.
                    while (background_work_queue_.empty())
                    {
                        background_work_cv_.Wait();
                    }

                    assert(!background_work_queue_.empty());
                    auto background_work_function = background_work_queue_.front().function;
                    void* background_work_arg = background_work_queue_.front().arg;
                    background_work_queue_.pop();

                    background_work_mutex_.Unlock();
                    background_work_function(background_work_arg);
                }
            }

            static void BackgroundThreadEntryPoint(ZoadEnv* env)
            {
                env->BackgroundThreadMain();
            }

            // Stores the work item data in a Schedule() call.
            //
            // Instances are constructed on the thread calling Schedule() and used on the
            // background thread.
            //
            // This structure is thread-safe beacuse it is immutable.
            struct BackgroundWorkItem
            {
                explicit BackgroundWorkItem(void (*function)(void* arg), void* arg)
                        : function(function), arg(arg) {}

                void (* const function)(void*);
                void* const arg;
            };
	
	    typedef std::map<std::string, ZoadFile*> ZoadFileMap;
            //Need to seperate Lock
            //HOON
            port::Mutex mutex_;
	    ZoadFileMap file_map_ GUARDED_BY(mutex_);

            port::Mutex background_work_mutex_;
            port::CondVar background_work_cv_ GUARDED_BY(background_work_mutex_);
            bool started_background_thread_ GUARDED_BY(background_work_mutex_);

            std::queue<BackgroundWorkItem> background_work_queue_
            GUARDED_BY(background_work_mutex_);

            ZoadLockTable locks_;
        };

        ZoadEnv::ZoadEnv()
                : background_work_cv_(&background_work_mutex_),
                  started_background_thread_(false){}

        // Wraps an Env instance whose destructor is never created.
        //
        // Intended usage:
        //   using PlatformSingletonEnv = SingletonEnv<PlatformEnv>;
        //   void ConfigurePosixEnv(int param) {
        //     PlatformSingletonEnv::AssertEnvNotInitialized();
        //     // set global configuration flags.
        //   }
        //   Env* Env::Default() {
        //     static PlatformSingletonEnv default_env;
        //     return default_env.env();
        //   }
        template<typename EnvType>
        class SingletonEnv
        {
        public:
            SingletonEnv() {
#if !defined(NDEBUG)
                env_initialized_.store(true, std::memory_order::memory_order_relaxed);
#endif  // !defined(NDEBUG)
                static_assert(sizeof(env_storage_) >= sizeof(EnvType),
                              "env_storage_ will not fit the Env");
                static_assert(alignof(decltype(env_storage_)) >= alignof(EnvType),
                              "env_storage_ does not meet the Env's alignment needs");
                new (&env_storage_) EnvType();
            }
            ~SingletonEnv() = default;

            SingletonEnv(const SingletonEnv&) = delete;
            SingletonEnv& operator=(const SingletonEnv&) = delete;

            Env* env() { return reinterpret_cast<Env*>(&env_storage_); }

            static void AssertEnvNotInitialized() {
#if !defined(NDEBUG)
                assert(!env_initialized_.load(std::memory_order::memory_order_relaxed));
#endif  // !defined(NDEBUG)
            }

        private:
            typename std::aligned_storage<sizeof(EnvType), alignof(EnvType)>::type
                    env_storage_;
#if !defined(NDEBUG)
            static std::atomic<bool> env_initialized_;
#endif  // !defined(NDEBUG)
        };

#if !defined(NDEBUG)
        template<typename EnvType>
        std::atomic<bool> SingletonEnv<EnvType>::env_initialized_;
#endif  // !defined(NDEBUG)

        using ZoadDefaultEnv = SingletonEnv<ZoadEnv>;

    }  // namespace

    Env* Env::ZoadDefault() {
        static ZoadDefaultEnv env_container;

        for(int i = 0; i < MAX_FLAG; i++)
        {
            buf_size[i] = IO_SIZE * BUF_SIZE;
        }

        return env_container.env();
    }

}  // namespace leveldb
