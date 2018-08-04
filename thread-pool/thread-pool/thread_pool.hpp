#pragma once
#include <functional>
#include <thread>
#include <condition_variable>
#include <list>
#include <malloc.h>
#include <new>

#ifndef _THREADPOOL_CACHE_LINE_SIZE
#define _THREADPOOL_CACHE_LINE_SIZE 64
#endif


class thread_pool {
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
			data[i] = new(_mm_malloc(sizeof(T), align)) T;
		}
	}
	void destroy() {
		if (data != nullptr) {
			for (int i = 0; i < d_size; i++) {
				data[i]->~T();
				_mm_free(data[i]);
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