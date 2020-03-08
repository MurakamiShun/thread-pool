#include "thread_pool.hpp"

#include <iostream>
#include <chrono>

int main() {
	using namespace std;

	constexpr int core = 8;

	// thread group
	thread_group group(core);
	auto t = chrono::system_clock::now();
	aligned_array<int> sum(core);
	for (int i = 0; i < 100; i++) {
		for (int th = 0; th < core; th++) {
			group.post(th, [&sum, th] {
				int tmp = 0;
				for (int j = 0; j < 1000000; j++)
					sum[th] += 1;
				sum[th] += tmp;
			});
		}
	}
	group.wait();
	auto multi = (chrono::system_clock::now() - t).count();
	cout << core << " thread group:\t" << multi << " ns" << endl;

	thread_pool p;
	t = chrono::system_clock::now();
	int tmp = 0;
	for (int i = 0; i < 100; i++) {
		p.post([&] {
			for (int j = 0; j < 1000000 * core; j++)
				tmp += 1;
		});
	}
	p.wait();
	auto single = (chrono::system_clock::now() - t).count();
	cout << "single thread:\t" << single << " ns" << endl;

	cout << "ratio:\t" << static_cast<double>(single) / static_cast<double>(multi) << std::endl;
	
	return 0;
}