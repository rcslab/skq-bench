
#ifndef __CELESTIS_CHANNEL_H__
#define __CELESTIS_CHANNEL_H__

#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace Celestis
{

template <class _T>
class Channel {
public:
    Channel() : closed(false)
    {
    }
    ~Channel()
    {
    }
    bool is_closed()
    {
	return closed;
    }
    void close()
    {
	std::unique_lock<std::mutex> lock(m);
	closed = true;

	cv.notify_all();
    }
    void put(_T item)
    {
	std::unique_lock<std::mutex> lock(m);

	q.push(item);
	cv.notify_one();
    }
    _T get(bool wait = true)
    {
	std::unique_lock<std::mutex> lock(m);

	cv.wait(lock, [&](){ return !q.empty(); });

	_T val = q.front();
	q.pop();

	return val;
    }
private:
    template<typename Arg1>
    static _T get_any_nosleep(Arg1& arg1)
    {
	return arg1.get(false);
    }
    template<typename Arg1, typename... Args>
    static _T get_any_nosleep(Arg1& arg1, Args&... args)
    {
	try {
	    return arg1.get(false);
	} catch (std::exception &e) {
	}
	return get_any_nosleep(args...);
    }
public:
    template<typename... Args>
    static _T get_any(Args& ... args)
    {
	while (1) {
	    try {
		return get_any_nosleep(args...);
	    } catch (std::exception &e) {
	    }
	    std::this_thread::yield();
	}
    }
private:
    std::mutex m;
    std::condition_variable cv;
    bool closed;
    std::queue<_T> q;
};

}

#endif /* __CELESTIS_CHANNEL_H__ */

