#pragma once
#include <functional>
#include <thread>
#include <condition_variable>
#include <queue>
#include <new>
#include <atomic>
#include <optional>
#include <future>

#ifdef _WIN32
#include <malloc.h>
#endif
#ifdef __linux__
#include <stdlib.h>
#endif

class thread_pool {
private:
	using Proc = std::function<void()>;
	std::thread thread;
	std::condition_variable enqueque_cv;
	std::mutex mtx;
	std::queue<Proc> tasks;
	std::atomic_bool stop = false;
	std::condition_variable task_empty_cv;
public:
	std::optional<std::function<std::optional<Proc>()>> task_fetcher;

	thread_pool() {
		thread = std::thread([this] {
			while (true) {
				if (task_fetcher) {
					auto func = (*task_fetcher)();
					if (!stop && !func) {
						task_empty_cv.notify_all();
						std::unique_lock lock(mtx);
						enqueque_cv.wait(lock);
					}
					if (stop) return;
					if (!func) continue;
					(*func)();
				}
				else {
					Proc func;
					{
						std::unique_lock lock(mtx);
						if (!stop && tasks.empty()) {
							task_empty_cv.notify_all();
							enqueque_cv.wait(lock);
						}
						if (stop) return;
						if (tasks.empty()) continue;
						func = std::move(tasks.front());
						tasks.pop();
					}
					func();
				}
			}
		});
	}
	~thread_pool() {
		stop = true;
		enqueque_cv.notify_all();
		thread.join();
	}
	thread_pool(const thread_pool&) = delete;

	template<typename F, typename... Args>
	auto post(F&& f, Args&&... args) {
		if constexpr (std::is_same_v<void, std::invoke_result_t<F, Args...>>) {
			std::lock_guard lock(mtx);
			tasks.push([f,args...] { f(args...); });
			enqueque_cv.notify_all();
			return;
		}
		else {
			auto task = std::make_shared<std::packaged_task<std::invoke_result_t<F, Args...>(Args...)>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
			auto ret = task->get_future();

			std::lock_guard lock(mtx);
			tasks.push([task] { (*task)(); });
			enqueque_cv.notify_all();

			return ret;
		}
	}

	void run_fetcher() {
		enqueque_cv.notify_all();
	}

	void wait() {
		std::unique_lock lock(mtx);
		task_empty_cv.wait(lock, [this] { return tasks.empty(); });
	}
	size_t task_count() {
		std::lock_guard lock(mtx);
		return tasks.size();
	}
};

class thread_group {
private:
	using Proc = std::function<void()>;
	std::mutex mtx;
	std::queue<Proc> tasks;
	std::condition_variable task_cv;

public:
	std::unique_ptr<thread_pool[]> threads;
	const size_t thread_count;

	thread_group(const size_t thread_num = std::thread::hardware_concurrency()) : thread_count(thread_num){
		threads = std::make_unique<thread_pool[]>(thread_num);
		for (size_t i = 0; i < thread_num; ++i) {
			threads[i].task_fetcher = [this]()->std::optional<Proc> {
				Proc func;
				{
					std::lock_guard lock(mtx);
					if (tasks.empty()) return std::nullopt;
					func = std::move(tasks.front());
					tasks.pop();
				}
				task_cv.notify_all();
				return std::move(func);
			};
		}
	}

	template<typename F, typename... Args>
	auto post(F&& f, Args&&... args) {
		if constexpr (std::is_same_v<void, std::invoke_result_t<F, Args...>>) {
			std::lock_guard lock(mtx);
			tasks.push([f, args...]{ f(args...); });
			run();
			return;
		}
		else {
			auto task = std::make_shared<std::packaged_task<std::invoke_result_t<F, Args...>(Args...)>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
			auto ret = task->get_future();

			std::lock_guard lock(mtx);
			tasks.push([task] { (*task)(); });
			run();

			return ret;
		}
	}
	void run() {
		for (size_t i = 0; i < thread_count; ++i)
			threads[i].run_fetcher();
	}

	void wait_all() {
		std::unique_lock lock(mtx);
		task_cv.wait(lock, [this] { return tasks.empty(); });

		for (size_t i = 0; i < thread_count; ++i)
			threads[i].wait();
	}
	size_t task_count() {
		std::lock_guard lock(mtx);
		return tasks.size();
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