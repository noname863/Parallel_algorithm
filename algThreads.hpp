#ifndef ALGTHREADS_HPP
#define ALGTHREADS_HPP
#include <thread>
#include <cstdint>

static uint8_t _num_of_threads = static_cast<uint8_t>((std::thread::hardware_concurrency()) == 0 ? 1 : std::thread::hardware_concurrency());

static std::thread * _threads = new std::thread[_num_of_threads];

void _joinAllThreads()
{
    std::thread * beg = _threads;
    std::thread * end = _threads + _num_of_threads;
    for (; beg != end; ++beg)
        beg->join();
}


#endif // ALGTHREADS_HPP
