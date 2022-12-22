#ifndef UTILS_THREAD_H
#define UTILS_THREAD_H

#include <thread>
#include <queue>
#include <type_traits>
#include <vector>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <future>

/* 线程之间的负载均衡？
 * 因为每一个线程都在从Que中取，所以已经走了负载均衡，唯一需要注意的是
 * 做好线程之间数据互斥
*/
class UtilsThread {
public:
    static UtilsThread& GetInstance()
    {
        static UtilsThread utl ;// = UtilsThread(cnt);
        return utl;
    }

    UtilsThread(int cnt = 3)
      : cnt_(cnt),
        exit_{false}
    {
        threadVec_.reserve(cnt);
        // create server work thread
        for (int i = 0; i < cnt; ++i) {
            threadVec_.emplace_back(std::thread(&UtilsThread::Worker, this));
            //threadVec_.push_back(thd);
        }
    }

    ~UtilsThread()
    {
        for (auto& ele : threadVec_) {
            if (ele.joinable()) {
                ele.join();
            }
        }
    }

    void Worker()
    {
        std::function<void()> workLocal;
        while (exit_ == false) {
           { 
                std::unique_lock<std::mutex> lk(queMtx_);
                cnd_.wait(lk, [this](){return !workQue_.empty();});
                if (workQue_.empty()) {
                    return;
                }
                workLocal = workQue_.front();
                workQue_.pop();
           }
            workLocal();
        }
    }

    template<class F, class... Args>
    auto AddWork(F&& f, Args&&... args)
        -> std::future<typename std::result_of<F(Args...)>::type>
    {
        //if (workQue_.size() > MAX_QUE_SIZE) {
          //  return ;
        //}
        
        using return_type = typename std::result_of<F(Args...)>::type;
    
        auto work = std::make_shared< std::packaged_task<return_type()> >(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        std::future<return_type> res = work->get_future();
        {
            std::unique_lock<std::mutex> lk(queMtx_);
            workQue_.push([work]() { 
                (*work)();
            });
        }

        cnd_.notify_one();
        return res;  // must wait all the thread return
       //return true;
    }
private:
    int cnt_;
    std::queue<std::function<void()>> workQue_;
    std::mutex queMtx_;
    std::condition_variable  cnd_;
    std::vector<std::thread> threadVec_;
    bool exit_;
    const int MAX_QUE_SIZE = 50;
};

#endif