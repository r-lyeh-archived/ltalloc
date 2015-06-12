#include <vector>
#include <thread>
#include <chrono>
#include <iostream>
#include <assert.h>

const unsigned int ALLOC_COUNT = 100000000;

void threadproc()
{
	const unsigned int N = 8*1024;
	char *pp[N][2] = {0};//double-buffer for allocations to avoid branching
	for (unsigned int i=0, t=0; i<ALLOC_COUNT; i+=200, t^=1)
		for (unsigned int ii=200, rnd=0; ii; ii--)
		{
			rnd = 1664525L * rnd + 1013904223L;
			unsigned n = (rnd >> 8) % N;
			char *(&p)[2] = pp[n];
			assert(!p[t^1]);
			p[t^1] = new char [128];
			delete [] p[t];
			p[t] = NULL;
		}
	for (unsigned int i=0; i<2*N; i++)
		delete [] ((char**)pp)[i];
}

int main(int argc, char *argv[])
{
	float maxmops = 0;
	for (int n=3; n; n--)
	{
		std::vector<std::thread> threads;
		using namespace std::chrono;
		auto start = high_resolution_clock::now();
		for (int n=argc > 1 ? atoi(argv[1]) : 1; n; n--)
			threads.push_back(std::thread(&threadproc));
		for (size_t i=0; i<threads.size(); i++)
			threads[i].join();
		float mops = ALLOC_COUNT/1e6f/duration_cast<duration<float>>(high_resolution_clock::now() - start).count();
		std::cout.precision(1);
		std::cout << std::fixed << mops << '\n';
		std::cout.flush();
		maxmops = std::max(maxmops, mops);
	}
	return int(maxmops*10 + .5f);
}
