#define CATCH_CONFIG_MAIN
#include "catch/catch.hpp"
#include "ltalloc.h"
#include <thread>
#include <chrono>

const unsigned int ALLOC_COUNT = 1000000;

void threadproc(int* error)
{
	const unsigned int N = 8 * 1024;
	char *pp[N][2] = { 0 };//double-buffer for allocations to avoid branching
	for (unsigned int i = 0, t = 0; i < ALLOC_COUNT; i += 200, t ^= 1)
		for (unsigned int ii = 200, rnd = 0; ii; ii--)
		{
			rnd = 1664525L * rnd + 1013904223L;
			unsigned n = (rnd >> 8) % N;
			char *(&p)[2] = pp[n];
			if (p[t ^ 1])
			{
				*error = true;
				return;
			}
			p[t ^ 1] = (char*)ltmalloc(128);
			ltfree(p[t]);
			p[t] = NULL;
		}
	for (unsigned int i = 0; i < 2 * N; i++)
		ltfree(((char**)pp)[i]);
}

TEST_CASE("Original test code from the wiki", "[ltalloc]")
{
	int nb_threads = std::min(std::thread::hardware_concurrency(), 12u);
	INFO("nb_threads = " << nb_threads);
	std::cout << "Using " << nb_threads << " threads\n";
	std::cout.flush();

	float maxmops = 0;
	for (int n = 0; n < 4; ++n)
	{
		std::vector<int> thread_errors;
		thread_errors.resize(nb_threads);
		std::vector<std::thread> threads;
		using namespace std::chrono;
		auto start = high_resolution_clock::now();
		for (int n = 0; n < nb_threads; n++)
			threads.push_back(std::thread(&threadproc, &thread_errors[n]));
		for (size_t i = 0; i < threads.size(); i++)
			threads[i].join();
		for (int n = 0; n < nb_threads; n++)
			REQUIRE(!thread_errors[n]);
		float mops = ALLOC_COUNT / 1e6f / duration_cast<duration<float>>(high_resolution_clock::now() - start).count();
		std::cout.precision(1);
		std::cout << "Run " << n << ": ";
		std::cout << std::fixed << mops << " millions operations per second\n";
		std::cout.flush();
		maxmops = std::max(maxmops, mops);
	}

	std::cout << "Max: " << std::fixed << maxmops << " millions operations per second\n";

#ifdef LTALLOC_OVERFLOW_DETECTION
	std::cout << "following assert should trigger!\n";
	int *overflow = new int (1);
	overflow[1] = 1;
	delete overflow;
#endif
}
