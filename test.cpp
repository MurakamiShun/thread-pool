#include "thread_pool.hpp"

#include <iostream>
#include <chrono>

int main() {
	using namespace std;

	constexpr int core = 2;
	// single thread
	{
		auto t = chrono::system_clock::now();
		volatile int c = 0;
		for (int i = 0; i < 100; i++) {
			for (int j = 0; j < 1000000 * core; j++)
				c += 1;
		}
		cout << core << " single:\t" << chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - t).count() << " ms" << endl;
	}

	// multi thread
	{
		thread_pool pool[core];
		align_array<int, 64> count(core);
		for (auto& i : count) {
			i = 0;
		}
		// Normal array is not good for cache line size
		//int count[core] = { 0 };
		auto t = chrono::system_clock::now();
		for (int i = 0; i < 100; i++) {
			for (int th = 0; th < core; th++) {
				int& c = count[th];
				pool[th].run([&c] {
					for (int j = 0; j < 1000000; j++)
						c += 1;
				});
			}
		}
		for (int th = 0; th < core; th++)
			pool[th].join();
		cout << core << " threads:\t" << chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now() - t).count() << " ms" << endl;
	}

#ifdef _WIN32
	system("pause");
#endif
	return 0;
}