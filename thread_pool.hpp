#pragma once
#include <functional>
#include <thread>
#include <condition_variable>
#include <list>

#ifdef _WIN32
#include <malloc.h>
#endif
#ifdef __linux__
#include <stdlib.h>
#endif



class thread_pool {
private:
	std::thread thread;
	std::condition_variable cv_empty;
	std::condition_variable cv_finish_func;
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
					cv_empty.wait(lock, [&] {return func_list.size(); });
				}
				std::function<void()> func;
				{
					std::lock_guard<std::mutex> lock(mtx);
					func = std::move(func_list.front());
					func_list.pop_front();
				}
				func();
				cv_finish_func.notify_all();
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
	void run(const std::function<void()>& arg) {
		std::lock_guard<std::mutex> lock(mtx);
		func_list.push_back(arg);
		cv_empty.notify_one();
	}
	void run(const std::function<void()>&& arg) {
		std::lock_guard<std::mutex> lock(mtx);
		func_list.push_back(std::move(arg));
		cv_empty.notify_one();
	}
	void wait() {
		std::unique_lock<std::mutex> lock(mtx);
		cv_finish_func.wait(lock, [this] {return !func_list.size(); });
	}
	void join() {
		joined = true;
		if (!func_list.size())
			run([] {});
		thread.join();
	}
};



template<class T, unsigned int align_N=64>
class align_array{
private:
	int d_align;
	int d_size;
	T** data;
	struct iterator {
		int counter;
		const int size;
		T*const * const data;
		iterator(const int arg_size, T* const* const arg_data) noexcept :
			counter(0),
			size(arg_size),
			data(arg_data)
		{
		}
		bool operator!=(iterator& iter) const noexcept {
			return counter != iter.size;
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
	align_array(const int size, const int align=align_N){
		init(size, align);
	}
	align_array(const align_array& copy) {
		(*this) = copy;
	}
	align_array(align_array&& move) noexcept {
		d_size = move.d_size;
		data = move.data;
		move.data = nullptr;
	}
	void resize(const int size) {
		destroy();
		init(size, d_align);
	}
	align_array<T>& operator=(const align_array& copy) {
		init(copy.d_size, copy.d_align);
		for (int i = 0; i < d_size; i++) {
			*data[i] = *copy.data[i];
		}
		return *this;
	}
	align_array<T>& operator=(align_array&& move) noexcept {
		d_size = move.d_size;
		data = move.data;
		move.data = nullptr;
		return *this;
	}
	~align_array() {
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