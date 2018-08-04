#include "thread_pool.hpp"

#include <iostream>
#include <chrono>

int main() {
	using namespace std;

	constexpr int core = 8;

	thread_pool pool[core];
	aligned<int> count(core, 64);
	for (auto& i : count) {
		i = 0;
	}
	// Normal array is not good for cache line size
	//int count[core] = { 0 };
	auto t = chrono::system_clock::now();
	for (int i = 0; i < 100; i++) {
		for (int th = 0; th < core; th++) {
			int& ccc = count[th];
			pool[th].run([&] {
				for (int j = 0; j < 1000000; j++)
					ccc += 1;
			});
		}
	}
	for(int th=0; th < core; th++)
		pool[th].join();
	
	cout << core << " threads:\t" << (chrono::system_clock::now() - t).count() << " ns" << endl;
	

	thread_pool p;
	t = chrono::system_clock::now();
	int c = 0;
	for (int i = 0; i < 100; i++) {
		p.run([&] {
			for (int j = 0; j < 1000000 * core; j++)
				c += 1;
		});
	}
	p.join();
	cout << "single thread:\t" << (chrono::system_clock::now() - t).count() << " ns" << endl;

	
	t = chrono::system_clock::now();
	volatile int c_ = 0;
	for (int i = 0; i < 100; i++) {
		for (int j = 0; j < 1000000 * core; j++)
				c_ += 1;
	}
	cout << "simple for:\t" << (chrono::system_clock::now() - t).count() << " ns" << endl;
	
#ifdef _WIN32
	system("pause");
#endif
	return 0;
}