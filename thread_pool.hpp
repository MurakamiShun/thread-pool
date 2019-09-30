#pragma once
#include <functional>
#include <thread>
#include <condition_variable>
#include <list>
#include <new>
#include <atomic>

#ifdef _WIN32
#include <malloc.h>
#endif
#ifdef __linux__
#include <stdlib.h>
#endif

#ifndef _THREADPOOL_CACHE_LINE_SIZE
#define _THREADPOOL_CACHE_LINE_SIZE 64
#endif


class thread_pool {
private:
	std::thread thread;
	std::condition_variable cv;
	std::mutex mtx;
	std::list<std::function<void()>> func_list;
	std::atomic_bool joined = false;
	std::atomic_bool is_running = false;
public:
	thread_pool() {
		thread = std::thread([this] {
			while (!joined || func_list.size()) {
				std::function<void()> func;
				{
					std::unique_lock<std::mutex> lock(mtx);
					if (func_list.size() == 0) {
						cv.wait(lock, [&] {return func_list.size() && is_running; });
						continue;
					}
					func = std::move(func_list.front());
					func_list.pop_front();
				}
				func();
				cv.notify_all();
			}
		});
	}
	~thread_pool() {
		join();
	}
	thread_pool(const thread_pool&) = delete;
	void post(std::function<void()> func) {
		std::lock_guard<std::mutex> lock(mtx);
		func_list.push_back(std::move(func));
		cv.notify_all();
	}
	void run() {
		is_running = true;
	}
	void wait() {
		std::unique_lock<std::mutex> lock(mtx);
		cv.wait(lock, [this] {return !func_list.size(); });
		is_running = false;
	}
	void join() {
		if (!joined) {
			joined = true;
			is_running = true;
			if (!func_list.size())
				post([] {});
			thread.join();
			is_running = false;
		}
	}
	size_t task_count() {
		std::lock_guard<std::mutex> lock(mtx);
		return func_list.size();
	}
};

class thread_group {
private:
	std::condition_variable cv;
	std::mutex mtx;
	std::list<std::function<void()>> func_list;
	const size_t thread_num;
	std::unique_ptr<std::thread[]> threads;
	std::atomic_bool joined = false;
	std::atomic_bool is_running = false;
public:
	thread_group(const size_t thread_num):
	thread_num(thread_num)
	{
		threads = std::make_unique<std::thread[]>(thread_num);
		for (size_t i = 0; i < thread_num; i++) {
			threads[i] = std::thread([this] {
				while (!joined || func_list.size() > this->thread_num) {
					std::function<void()> func;
					{
						std::unique_lock<std::mutex> lock(mtx);
						if (func_list.size() == 0) {
							cv.wait(lock, [&] {return func_list.size() && is_running; });
						}
						func = std::move(func_list.front());
						func_list.pop_front();
					}
					func();
					cv.notify_all();
				}
			});
		}
	}
	~thread_group() {
		join();
	}
	void post(std::function<void()> func) {
		std::lock_guard<std::mutex> lock(mtx);
		func_list.push_back(std::move(func));
		cv.notify_all();
	}
	void run() {
		is_running = true;
	}
	void wait() {
		std::unique_lock<std::mutex> lock(mtx);
		cv.wait(lock, [this] {return !func_list.size(); });
		is_running = false;
	}
	void join() {
		if (!joined) {
			joined = true;
			is_running = true;
			if (func_list.size() < thread_num) {
				for (size_t i = 0; i < thread_num; i++) {
					post([] {});
				}
			}
			for (size_t i = 0; i < thread_num; i++) {
				threads[i].join();
			}
			is_running = false;
		}
	}
	size_t task_count() {
		std::lock_guard<std::mutex> lock(mtx);
		return func_list.size();
	}
};



template<class T>
class aligned{
private:
	int d_align;
	int d_size;
	T** data;
	struct iterator {
		int counter;
		const int end;
		T*const * const data;
		iterator(const int arg_end, T* const* const arg_data) noexcept :
			counter(0),
			end(arg_end),
			data(arg_data)
		{
		}
		bool operator!=(iterator& iter) const noexcept {
			return counter != iter.end;
		}
		T& operator*() {
			return *data[counter];
		}
		const T& operator*() const{
			return *data[counter];
		}
		iterator& operator++() noexcept {
			counter++;
			return *this;
		}
	};
	void init(const int size, const int align) {
		d_size = size;
		d_align = align;
		data = new T*[size];
		for (int i = 0; i < d_size; i++) {
#ifdef _WIN32
			data[i] = new(_aligned_malloc(sizeof(T), align)) T;
#endif
#ifdef __linux__
			data[i] = new(aligned_alloc(sizeof(T), align)) T;
#endif
		}
	}
	void destroy() {
		if (data != nullptr) {
			for (int i = 0; i < d_size; i++) {
				data[i]->~T();
#ifdef _WIN32
				_aligned_free(data[i]);
#endif
#ifdef __linux__
				free(data[i]);
#endif
			}
			delete[] data;
			data = nullptr;
		}
	}
public:
	aligned(const int size, const int align=_THREADPOOL_CACHE_LINE_SIZE){
		init(size, align);
	}
	void resize(const int size) {
		this->destroy();
		this->init(size, d_align);
	}
	aligned(aligned&& move) noexcept{
		d_size = move.d_size;
		data = move.data;
		move.data = nullptr;
	}
	aligned<T>& operator=(aligned&& move) noexcept {
		d_size = move.d_size;
		data = move.data;
		move.data = nullptr;
		return *this;
	}
	~aligned() {
		destroy();
	}
	T& operator[](const int index) {
		return *(data[index]);
	}
	const T& operator[](const int index) const {
		return *(data[index]);
	}
	const int size() const noexcept {
		return d_size;
	}
	const int align() const noexcept {
		return d_align;
	}
	iterator begin() const noexcept {
		return iterator(d_size, data);
	}
	iterator end() const noexcept {
		return iterator(d_size, data);
	}
};