#pragma once
#include <functional>
#include <thread>
#include <condition_variable>
#include <queue>
#include <new>
#include <atomic>

#ifdef _WIN32
#include <malloc.h>
#endif
#ifdef __linux__
#include <stdlib.h>
#endif



class thread_pool {
private:
	std::thread thread;
	std::condition_variable cv;
	std::mutex mtx;
	std::queue<std::function<void()>> tasks;
	std::atomic_bool stop = false;
public:
	thread_pool() {
		thread = std::thread([this] {
			while (true) {
				std::function<void()> func;
				{
					std::unique_lock<std::mutex> lock(mtx);
					if (!stop && tasks.empty()) {
						cv.notify_all();
						cv.wait(lock);
					}
					if (stop) return;
					func = std::move(tasks.front());
					tasks.pop();
				}
				func();
			}
		});
	}
	~thread_pool() {
		stop = true;
		cv.notify_all();
		thread.join();
	}
	thread_pool(const thread_pool&) = delete;
	void post(std::function<void()> func) {
		std::lock_guard<std::mutex> lock(mtx);
		tasks.push(std::move(func));
		cv.notify_all();
	}
	void wait() {
		std::unique_lock<std::mutex> lock(mtx);
		cv.wait(lock, [this] { return !tasks.size(); });
	}
	size_t task_count() {
		std::lock_guard<std::mutex> lock(mtx);
		return tasks.size();
	}
};

class thread_group {
private:
	std::unique_ptr<thread_pool[]> thread_pools;
	size_t thread_count;
public:
	thread_group(const size_t thread_num) {
		thread_pools = std::make_unique<thread_pool[]>(thread_num);
		thread_count = thread_num;
	}
	void post(size_t th, std::function<void()> func) {
		thread_pools[th].post(std::move(func));
	}
	void wait() {
		for (size_t i = 0; i < thread_count; ++i)
			thread_pools[i].wait();
	}
	size_t task_count() {
		size_t sum = 0;
		for (size_t i = 0; i < thread_count; ++i)
			sum += thread_pools[i].task_count();
	}
};


template<typename T>
class aligned_array {
private:
	size_t d_align;
	size_t d_size;
	T** data;
	struct iterator {
		size_t counter;
		const size_t size;
		T* const* const data;
		iterator(const size_t arg_size, T* const* const arg_data) noexcept :
			counter(0),
			size(arg_size),
			data(arg_data)
		{
		}
		bool operator!=(iterator& iter) const noexcept { return counter != iter.size; }
		T& operator*() { return *data[counter]; }
		const T& operator*() const { return *data[counter]; }
		iterator& operator++() noexcept {
			++counter;
			return *this;
		}
	};
	void init(const size_t size, const size_t align) {
		d_size = size;
		d_align = align;
		data = new T * [size];
		for (size_t i = 0; i < d_size; ++i) {
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
			for (int i = 0; i < d_size; ++i) {
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
	aligned_array(const size_t size, const size_t align = 64) { init(size, align); }
	aligned_array(const aligned_array& src) {
		init(src.d_size, src.d_align);
		for (int i = 0; i < d_size; ++i)
			*data[i] = *src.data[i];
	}
	aligned_array(aligned_array&& src) noexcept { operator=(std::move(src)); }
	void resize(const size_t size, const size_t align = 64) {
		destroy();
		init(size, align);
	}
	aligned_array<T>& operator=(aligned_array&& move) noexcept {
		d_size = move.d_size;
		data = move.data;
		move.data = nullptr;
		return *this;
	}
	~aligned_array() { destroy(); }
	T& operator[](const size_t index) { return *(data[index]); }
	const T& operator[](const size_t index) const { return *(data[index]); }
	const size_t size() const noexcept { return d_size; }
	const size_t align() const noexcept { return d_align; }
	iterator begin() const noexcept { return iterator(d_size, data); }
	iterator end() const noexcept { return iterator(d_size, data); }
};