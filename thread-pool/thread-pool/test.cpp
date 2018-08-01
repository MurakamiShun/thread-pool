#include "thread_pool.hpp"

#include <iostream>
#include <chrono>
#include <thread>


int main() {
	using namespace std;


	thread_pool pool[8];

	alignas(64) int g1 = 0, g2 = 0, g3 = 0, g4 = 0, g5 = 0, g6 = 0, g7 = 0, g8 = 0;
	int* count[8] = { &g1, &g2, &g3, &g4, &g5, &g6, &g7, &g8 };
	auto t = chrono::system_clock::now();
	for (int th = 0; th < 8; th++) {
		int& ccc = *count[th];
		for (int i = 0; i < 100; i++) {
			pool[th].run([&] {
				for (int j = 0; j < 1000000; j++)
					ccc += 1;
			});
		}
	}
	for(int th=0; th < 8; th++)
		pool[th].join();
	
	cout << "thread pool:" << (chrono::system_clock::now() - t).count() << " ns" << endl;
	cout << *count[0] << " times" << endl;
	

	thread_pool p;
	t = chrono::system_clock::now();
	int c = 0;
	for (int i = 0; i < 100; i++) {
		p.run([&] {
			for (int j = 0; j < 8000000; j++)
				c += 1;
		});
	}
	p.join();
	cout << "thread pool:" << (chrono::system_clock::now() - t).count() << " ns" << endl;
	cout << c << " times" << endl;

	
	t = chrono::system_clock::now();
	volatile int c_ = 0;
	for (int i = 0; i < 100; i++) {
		for (int j = 0; j < 8000000; j++)
				c_ += 1;
	}
	cout << "thread pool:" << (chrono::system_clock::now() - t).count() << " ns" << endl;
	cout << c_ << " times" << endl;
	


	system("pause");
	return 0;
}