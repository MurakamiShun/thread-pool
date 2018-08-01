#pragma once
#include <functional>
#include <thread>
#include <condition_variable>
#include <list>


class alignas(64) thread_pool {
private:
	std::thread thread;
	std::condition_variable cv;
	std::mutex mtx;
	std::list<std::function<void()>> func_list;
	bool joined;
public:
	thread_pool():
		joined(false)
	{
		thread = std::thread([this] {
			while (!joined || func_list.size()) {
				if (func_list.size() == 0){
					std::unique_lock<std::mutex> lock(mtx);
					cv.wait(lock, [&] {return func_list.size(); });
				}
				func_list.front()();
				func_list.pop_front();
				cv.notify_all();
			}
		});
	}
	~thread_pool() {
		if (!joined) {
			joined = true;
			if (!func_list.size())
				run([] {});
			thread.join();
		}
		
	}
	thread_pool(const thread_pool&) = delete;
	void run(std::function<void()>& arg) {
		std::unique_lock<std::mutex> lock(mtx);
		func_list.push_back(arg);
		cv.notify_all();
	}
	void run(const std::function<void()> arg) {
		std::unique_lock<std::mutex> lock(mtx);
		func_list.push_back(arg);
		cv.notify_all();
	}
	void wait() {
		std::unique_lock<std::mutex> lock(mtx);
		cv.wait(lock, [this] {return !func_list.size(); });
	}
	void join() {
		joined = true;
		if (!func_list.size())
			run([] {});
		thread.join();
	}
};