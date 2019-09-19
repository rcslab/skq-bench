
#ifndef __CELESTIS_THREADPOOL_H__
#define __CELESTIS_THREADPOOL_H__

#include <queue>
#include <vector>
#include <thread>
#include <future>
#include <functional>

namespace Celestis
{

class ThreadPool {
    void worker() {
	while (true) {
	    std::function<void()> task;
	    {
		std::unique_lock<std::mutex> ul(lock);

		while (!interrupted && tasks.empty()) {
		    cv.wait(ul);
		}

		if (interrupted)
		    return;

		task = std::move(tasks.front());
		tasks.pop();
	    }

	    task();
	}
    }
public:
    /*
     * Construct a thread pool of the specified size, the default size is the 
     * number of error hardware threads as detected by calling
     * thread::hardware_concurrency.
     */
    ThreadPool(int size = -1)
        : waiting(false), interrupted(false)
    {
        int i;

        if (size == -1)
            size = std::thread::hardware_concurrency();

        for (i = 0; i < size; i++) {
            threads.emplace_back(&ThreadPool::worker, this);
        }
    }

    ~ThreadPool()
    {
	    drain_and_wait();
    }

    template<class F, class ... Args>
    std::future<typename std::result_of<F(Args...)>::type>
    enqueue(F &&f, Args &&... args)
    {
        using rettype = typename std::result_of<F(Args...)>::type;
        std::future<rettype> ret;
        {

            std::unique_lock<std::mutex> ul(lock);
            if (waiting)
            throw std::exception();

            auto b = std::bind(f, args...);
            auto t = std::make_shared<std::packaged_task<rettype()> >(b);
            ret = t->get_future();

            tasks.emplace([t](){ (*t)(); });
        }

        cv.notify_one();

        return ret;
    }

    void 
    drain_and_wait()
    {
	while (true) {
            {
		std::unique_lock<std::mutex> ul(lock);
		waiting = true;
		if (tasks.size() == 0) {
		    interrupted = true;
		    break;
		}
            }
            std::this_thread::yield();
        }
        stop_and_wait();
    }

    void 
    stop_and_wait()
    {
        waiting = true;
        interrupted = true;

        cv.notify_all();

        for (auto &t : threads) {
            t.join();
        }
        threads.clear();
    }

    int 
    concurrency()
    {

        return threads.size();
    }

private:
    // Locking
    std::mutex lock;
    std::condition_variable cv;
    bool waiting;
    bool interrupted;
    // Workers
    std::vector<std::thread> threads;
    // Task Queue
    std::queue<std::function<void()> > tasks;
};

}

#endif /* __CELESTIS_THREADPOOL_H__ */

