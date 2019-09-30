#include "thread_pool.hpp"

#include <iostream>
#include <chrono>

int main() {
	using namespace std;

	constexpr int core = 2;

	thread_pool pool[core];
	align_array<int> count(core, 64);
	for (auto& i : count) {
		i = 0;
	}
	
	// thread group
	thread_group group(core);
	auto t = chrono::system_clock::now();
	group.run();
	for (int i = 0; i < 100; i++) {
		for (int th = 0; th < core; th++) {
			int& ccc = count[th];
			group.post([&] {
				for (int j = 0; j < 1000000; j++)
					ccc += 1;
			});
		}
	}
	group.join();
	auto multi = (chrono::system_clock::now() - t).count();
	cout << core << " thread group:\t" << multi << " ns" << endl;

	thread_pool p;
	t = chrono::system_clock::now();
	int c = 0;
	for (int i = 0; i < 100; i++) {
		p.post([&] {
			for (int j = 0; j < 1000000 * core; j++)
				c += 1;
		});
		p.run();
	}
	p.join();
	auto single = (chrono::system_clock::now() - t).count();
	cout << "single thread:\t" << single << " ns" << endl;

	cout << "ratio:\t" << static_cast<double>(single) / static_cast<double>(multi) << std::endl;
	

#ifdef _WIN32
	system("pause");
#endif
	return 0;
}