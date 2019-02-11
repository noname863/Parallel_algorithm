#ifndef ALG_H
#define ALG_H
#include <cstdlib>
#include <cstdint>
#include <thread>
#include <algorithm>
#include <iterator>
#include <type_traits>
#include <functional>
#include <atomic>
#include <future>
#include <numeric>
#include "IteratorWrapper.hpp"
#include "algThreads.hpp"
#include "all_is_same.hpp"
namespace alg
{
template <class It, class T>
using _ifRAIt = typename std::enable_if<std::is_same<typename std::iterator_traits<It>::iterator_category,
                                                     std::random_access_iterator_tag>::value, T>::type;

template <class It, class T>
using _ifnotRAIt = typename std::enable_if<!std::is_same<typename std::iterator_traits<It>::iterator_category,
                                                         std::random_access_iterator_tag>::value, T>::type;

template <class T, class... It>
using _ifAllRAIt = typename std::enable_if<all_is_same<std::random_access_iterator_tag,
                                                       typename std::iterator_traits<It>::iterator_category...>::value, T>::type;

template <class T, class... It>
using _ifAnyNotRAIt = typename std::enable_if<!all_is_same<std::random_access_iterator_tag,
                                                       typename std::iterator_traits<It>::iterator_category...>::value, T>::type;

template <typename _T1, typename _T2>
struct _pair
{
    _T1 first;
    _T2 second;
};

template <typename _T1, typename _T2>
alg::_pair<_T1, _T2> _make_pair(const _T1 & first, const _T2 & second)
{
    alg::_pair<_T1, _T2> res;
    res.first = first;
    res.second = second;
    return res;
}

template <typename _T1, typename _T2>
std::pair<_T1, _T2> _to_std_pair(const alg::_pair<_T1, _T2> & item)
{
    return std::make_pair(item.first, item.second);
}

size_t * _splitSize(size_t n)
{
    size_t div = n / alg::_num_of_threads;
    size_t mod = n % alg::_num_of_threads;
    size_t * res = new size_t[alg::_num_of_threads];
    for (uint8_t i = 0; i < alg::_num_of_threads; ++i)
    {
        res[i] = div;
        if (mod)
        {
            ++res[i];
            --mod;
        }
    }
    return res;
}

template <class RAIt>
RAIt * _split(RAIt first, RAIt last)
{
    RAIt * res = new RAIt[alg::_num_of_threads + 1];
    res[0] = first;
    size_t * splitedSize = alg::_splitSize(last - first);
    std::transform(res, res + alg::_num_of_threads, splitedSize, res + 1, std::plus<>());
    delete[] splitedSize;
    return res;
}


template <class It, class Func>
void _inThreadFindIfRAIt(It first, const It & last, const Func & f, std::atomic<It> & result, const It & trueLast)
{
    for (; (first != last) && (static_cast<It>(result) == trueLast); ++first)
        if (f(*first))
        {
            result = first;
            return;
        }
}

template <class It, class Func>
void _inThreadFindIfNotRAit(It first, const It & last, const Func & f, std::atomic<It> & result)
{
    while (true)
    {
        for (uint8_t i = 0; (i < alg::_num_of_threads) && (first != last); ++i, ++first);
        if (static_cast<It>(result) != last)
            return;
        if (f(*first))
        {
            result = first;
            return;
        }
    }
}

template <class It, class Func>
alg::_ifRAIt<It, It> find_any_if(It first, const It & last, const Func & f)
{
    It * splited = alg::_split(first, last);
    std::atomic<It> result = last;
    for (uint8_t i = 0; i < alg::_num_of_threads; ++i)
    {
        alg::_threads[i] = std::thread(alg::_inThreadFindIfRAIt<It, Func>,
                                  splited[i],
                                  std::cref(splited[i + 1]),
                                  std::cref(f),
                                  std::ref(result),
                                  std::cref(last));
    }
    alg::_joinAllThreads();
    delete[] splited;
    return static_cast<It>(result);
}

template <class It, class Func>
alg::_ifnotRAIt<It, It> find_any_if(It first, const It & last, const Func & f)
{
    std::atomic<It> result = last;
    for (uint8_t i = 0; i < alg::_num_of_threads; ++i, ++first)
    {
        alg::_threads[i] = std::thread(alg::_inThreadFindIfNotRAit<It, Func>,
                                  first,
                                  std::cref(last),
                                  std::cref(f),
                                  std::ref(result));
    }
    alg::_joinAllThreads();
    return static_cast<It>(result);
}

template <class It, class T>
It find_any(const It & first, const It & last, const T & item)
{
    return alg::find_any_if(first, last, std::bind(std::equal_to<>(), std::placeholders::_1, item));
}

template <class It, class Func>
It find_any_if_not(const It & first, const It & last, const Func & f)
{
    return alg::find_any_if(first, last, std::not_fn(f));
}

template <class It, class Func>
bool all_of(const It & first, const It & last, const Func & f)
{
    It check = alg::find_any_if(first, last, std::not_fn(f));
    return check == last;
}

template <class It, class Func>
bool any_of(const It & first, const It & last, const Func & f)
{
    It check = alg::find_any_if(first, last, f);
    return check != last;
}

template <class It, class Func>
bool none_of(const It & first, const It & last, const Func & f)
{
    It check = alg::find_any_if(first, last, f);
    return check == last;
}

template <class It, class Func>
alg::_ifRAIt<It, Func> for_each(It first, const It & last, const Func & f)
{
    It * splited = alg::_split(first, last);
    for (uint8_t i = 0; i < alg::_num_of_threads; ++i)
    {
        alg::_threads[i] = std::thread(std::for_each<It, Func>, splited[i], splited[i + 1], f);
    }
    alg::_joinAllThreads();
    delete[] splited;
    return std::move(f);
}

template <class It, class Func>
alg::_ifnotRAIt<It, Func> for_each(It first, const It & last, const Func & f)
{
    std::thread * beg = alg::_threads;
    std::thread * end = alg::_threads + alg::_num_of_threads;
    for (;beg != end; ++first, ++beg)
    {
        *beg = std::thread(std::for_each<alg::_IteratorWrapper<It>, Func>,
                           alg::_IteratorWrapper<It>(first, last),
                           alg::_IteratorWrapper<It>(last), f);
    }
    alg::_joinAllThreads();
    return std::move(f);
}

template <class It, class Func>
alg::_ifRAIt<It, uint> count_if(It first, const It & last, const Func & f)
{
    It * splited = alg::_split(first, last);
    std::future<long> * results = new std::future<long>[alg::_num_of_threads];
    for (uint8_t i = 0; i < alg::_num_of_threads; ++i)
    {
        results[i] = std::async(std::count_if<It, Func>, splited[i], splited[i + 1], std::cref(f));
    }
    long sum = std::accumulate(results, results + alg::_num_of_threads, 0, std::bind(std::plus<>(),
                                                                                     std::placeholders::_1,
                                                                                     std::bind(std::mem_fn(&std::future<long>::get),
                                                                                               std::placeholders::_2)));
    delete[] splited;
    delete[] results;
    return sum;
}

template <class It, class Func>
alg::_ifnotRAIt<It, uint> count_if(It first, const It & last, const Func & f)
{
    std::future<long> * results = new std::future<long>[alg::_num_of_threads];
    std::future<long> * beg = results;
    std::future<long> * end = results + alg::_num_of_threads;
    for (; beg != end; ++beg, ++first)
    {
        *beg = std::async(std::count_if<alg::_IteratorWrapper<It>, Func>,
                          alg::_IteratorWrapper<It>(first, last),
                          alg::_IteratorWrapper<It>(last),
                          std::cref(f));
    }
    long sum = std::accumulate(results, results + alg::_num_of_threads, 0, std::bind(std::plus<>(),
                                                                                     std::placeholders::_1,
                                                                                     std::bind(std::mem_fn(&std::future<long>::get),
                                                                                               std::placeholders::_2)));
    delete[] results;
    return sum;
}

template <class It1, class It2>
void _inThreadAnyMismatch(It1 first1, const It1 & last1, It2 first2, const It1 & trueLast, std::atomic<alg::_pair<It1, It2>> & result)
{
    for(; (first1 != last1); ++first1, ++first2)
    {
        if (static_cast<alg::_pair<It1, It2>>(result).first != trueLast)
            return;
        if (*first1 != *first2)
        {
            result = alg::_make_pair(first1, first2);
            return;
        }
    }
}

template <class It1, class It2>
alg::_ifAllRAIt<std::pair<It1, It2>, It1, It2> mismatch_any(It1 first1, const It1 & last1, It2 first2)
{
    It1 * splited = alg::_split(first1, last1);
    std::atomic<alg::_pair<It1, It2>> result = alg::_make_pair(last1, last1);
    for (uint8_t i = 0; i < alg::_num_of_threads; ++i, first2 += (splited[i] - splited[i - 1]))
    {
        alg::_threads[i] = std::thread(alg::_inThreadAnyMismatch<It1, It2>,
                                  splited[i],
                                  splited[i + 1],
                                  first2,
                                  std::cref(last1),
                                  std::ref(result));
    }
    alg::_joinAllThreads();
    delete[] splited;
    return alg::_to_std_pair(static_cast<alg::_pair<It1, It2>>(result));
}

template <class It1, class It2>
alg::_ifAnyNotRAIt<std::pair<It1, It2>, It1, It2> mismatch_any(It1 first1, const It1 & last1, It2 first2)
{
    std::atomic<alg::_pair<It1, It2>> result(alg::_make_pair(last1, first2));
    std::thread * beg = alg::_threads;
    std::thread * end = alg::_threads + alg::_num_of_threads;
    for (;beg != end; ++beg, ++first1, ++first2)
    {
        *beg = std::thread(alg::_inThreadAnyMismatch<It1, It2>, first1, std::cref(last1), first2, std::cref(last1), std::ref(result));
    }
    alg::_joinAllThreads();
    return alg::_to_std_pair(static_cast<alg::_pair<It1, It2>>(result));
}

template <class It1, class It2>
bool equal(It1 first1, const It1 & last1, It2 first2)
{
    std::pair<It1, It2> check = alg::mismatch_any(first1, last1, first2);
    return check.first == last1;
}

template <class InputIt, class OutputIt, class Func>
alg::_ifAllRAIt<void, InputIt, OutputIt> transform(InputIt first1, const InputIt & last1, OutputIt first2, const Func & f)
{
    size_t * splited = alg::_splitSize(last1 - first1);
    for (uint8_t i = 0; i < alg::_num_of_threads; first1 += splited[i], first2 += splited[i], ++i)
    {
        alg::_threads[i] = std::thread(std::transform<InputIt, OutputIt, Func>, first1, first1 + splited[i], first2, f);
    }
    alg::_joinAllThreads();
    delete [] splited;
}

template <class InputIt, class OutputIt, class Func>
alg::_ifAnyNotRAIt<void, InputIt, OutputIt> transform(InputIt first1, const InputIt & last1, OutputIt first2, const Func & f)
{
    for (uint8_t i = 0; i < alg::_num_of_threads; ++first1, ++first2, ++i)
    {
        alg::_threads[i] = std::thread(std::transform<alg::_IteratorWrapper<InputIt>, alg::_IteratorWrapper<OutputIt>, Func>,
                                  alg::_IteratorWrapper<InputIt>(first1, last1),
                                  alg::_IteratorWrapper<InputIt>(last1),
                                  alg::_IteratorWrapper<OutputIt>(first2), f);
    }
    alg::_joinAllThreads();
}

template <class InputIt1, class InputIt2, class OutputIt, class Func>
alg::_ifAllRAIt<void, InputIt1, InputIt2, OutputIt> transform(InputIt1 first1, const InputIt1 & last1, InputIt2 first2, OutputIt first3, const Func & f)
{
    size_t * splited = alg::_splitSize(last1 - first1);
    for (uint8_t i = 0; i < alg::_num_of_threads; first1 += splited[i], first2 += splited[i], first3 += splited[i], ++i)
    {
        alg::_threads[i] = std::thread(std::transform<InputIt1, InputIt2, OutputIt, Func>, first1, first1 + splited[i], first2, first3, f);
    }
    alg::_joinAllThreads();
    delete [] splited;
}

template <class InputIt1, class InputIt2, class OutputIt, class Func>
alg::_ifAnyNotRAIt<void, InputIt1, InputIt2, OutputIt> transform(InputIt1 first1, const InputIt1 & last1, InputIt2 first2, OutputIt first3, const Func & f)
{
    for (uint8_t i = 0; i < alg::_num_of_threads; ++first1, ++first2, ++first3, ++i)
    {
        alg::_threads[i] = std::thread(std::transform<alg::_IteratorWrapper<InputIt1>, alg::_IteratorWrapper<InputIt2>, alg::_IteratorWrapper<OutputIt>, Func>,
                                  alg::_IteratorWrapper<InputIt1>(first1, last1),
                                  alg::_IteratorWrapper<InputIt1>(last1),
                                  alg::_IteratorWrapper<InputIt2>(first2),
                                  alg::_IteratorWrapper<OutputIt>(first3),  f);
    }
    alg::_joinAllThreads();
}

template <class InputIt, class OutputIt, class Func>
void _oneThreadTransformN(InputIt first1, size_t n, OutputIt first2, const Func & f)
{
    for (size_t i = 0; i < n; ++i, ++first1, ++first2)
        *first2 = f(*first1);
}

template <class InputIt1, class InputIt2, class OutputIt, class Func>
void _oneThreadTransformN(InputIt1 first1, size_t n, InputIt2 first2, OutputIt first3, const Func & f)
{
    for (size_t i = 0; i < n; ++i, ++first1, ++first2, ++first3)
        *first3 = f(*first1, *first2);
}

template <class InputIt, class OutputIt, class Func>
alg::_ifAllRAIt<void, InputIt, OutputIt> transform_n(InputIt first1, size_t n, OutputIt first2, const Func & f)
{
    size_t * splited = alg::_splitSize(n);
    for (uint8_t i = 0; i < alg::_num_of_threads; first1 += splited[i], first2 += splited[i], ++i)
    {
        alg::_threads[i] = std::thread(alg::_oneThreadTransformN<InputIt, OutputIt, Func>, first1, splited[i], first2, std::cref(f));
    }
    alg::_joinAllThreads();
    delete [] splited;
}

template <class InputIt, class OutputIt, class Func>
alg::_ifAnyNotRAIt<void, InputIt, OutputIt> transform_n(InputIt first1, size_t n, OutputIt first2, const Func & f)
{
    size_t * splited = alg::_splitSize(n);
    for (uint8_t i = 0; i < alg::_num_of_threads; ++first1, ++first2, ++i)
    {
        alg::_threads[i] = std::thread(alg::_oneThreadTransformN<alg::_IteratorWrapper<InputIt>, alg::_IteratorWrapper<OutputIt>, Func>,
                                  alg::_IteratorWrapper<InputIt>(first1),
                                  splited[i],
                                  alg::_IteratorWrapper<OutputIt>(first2), std::cref(f));
    }
    alg::_joinAllThreads();
    delete [] splited;
}

template <class InputIt1, class InputIt2, class OutputIt, class Func>
alg::_ifAllRAIt<void, InputIt1, InputIt2, OutputIt> transform_n(InputIt1 first1, size_t n, InputIt2 first2, OutputIt first3, const Func & f)
{
    size_t * splited = alg::_splitSize(n);
    for (uint8_t i = 0; i < alg::_num_of_threads; first1 += splited[i], first2 += splited[i], first3 += splited[i], ++i)
    {
        alg::_threads[i] = std::thread(alg::_oneThreadTransformN<InputIt1, InputIt2, OutputIt, Func>, first1, splited[i], first2, first3, std::cref(f));
    }
    alg::_joinAllThreads();
    delete [] splited;
}

template <class InputIt1, class InputIt2, class OutputIt, class Func>
alg::_ifAnyNotRAIt<void, InputIt1, InputIt2, OutputIt> transform_n(InputIt1 first1, size_t n, InputIt2 first2, OutputIt first3, const Func & f)
{
    size_t * splited = alg::_splitSize(n);
    for (uint8_t i = 0; i < alg::_num_of_threads; ++first1, ++first2, ++first3, ++i)
    {
        alg::_threads[i] = std::thread(alg::_oneThreadTransformN<alg::_IteratorWrapper<InputIt1>, alg::_IteratorWrapper<InputIt2>, alg::_IteratorWrapper<OutputIt>, Func>,
                                  alg::_IteratorWrapper<InputIt1>(first1),
                                  splited[i],
                                  alg::_IteratorWrapper<InputIt2>(first2),
                                  alg::_IteratorWrapper<OutputIt>(first3), std::cref(f));
    }
    alg::_joinAllThreads();
    delete [] splited;
}

template <class InputIt, class OutputIt>
void copy(InputIt first1, const InputIt & last1, OutputIt first2)
{
    alg::transform(first1, last1, first2, [](const auto & item) { return item; });
}

template <class InputIt, class OutputIt>
void copy_n(InputIt first1, size_t n, OutputIt first2)
{
    alg::transform_n(first1, n, first2, [](const auto & item) { return item; });
}

template <class InputIt, class OutputIt>
void copy_backward(InputIt first1, const InputIt & last1, OutputIt last2)
{
    alg::transform(std::make_reverse_iterator(last1), std::make_reverse_iterator(first1), std::make_reverse_iterator(last2), [](const auto & item) { return item; });
}

template <class InputIt, class OutputIt>
void move(InputIt first1, const InputIt & last1, OutputIt first2)
{
    alg::transform(first1, last1, first2, [](const auto & item) { return std::move(item); });
}

template <class InputIt, class OutputIt>
void move_backward(InputIt first1, const InputIt & last1, OutputIt last2)
{
    alg::transform(first1, last1, std::make_reverse_iterator(last2), [](const auto & item) { return std::move(item); });
}

template <class It, typename T>
void fill(It first, const It & last, const T & item)
{
    alg::transform(first, last, first, [&item](const auto &) { return item; });
}

template <class It, typename T>
void fill_n(It first, size_t n, const T & item)
{
    alg::transform_n(first, n, first, [&item](const auto &) { return item; });
}

template <class It, class Func>
void generate(It first, const It & last, const Func & f)
{
    alg::transform(first, last, first, [&f](const auto &) { return f(); });
}

template <class It, class Func>
void generate_n(It first, size_t n, const Func & f)
{
    alg::transform_n(first, n, first, [&f](const auto &) { return f(); });
}

template <class InputIt, class OutputIt>
void reverce_copy(InputIt first1, const InputIt & last1, OutputIt first2)
{
    alg::transform(std::make_reverse_iterator(last1), std::make_reverse_iterator(first1), first2, [](const auto & item) { return item;});
}

template <class InputIt, class OutputIt, class Func, typename T>
void replace_copy_if(InputIt first1, const InputIt & last1, OutputIt first2, const Func & f, const T & new_value)
{
    alg::transform(first1, last1, first2, [&f, &new_value](const auto & item) { return f(item) ? new_value : item; });
}

template <class InputIt, class OutputIt, typename T>
void replace_copy(InputIt first1, const InputIt & last1, OutputIt first2, const T & old_value, const T & new_value)
{
    alg::transform(first1, last1, first2, [&old_value, &new_value](const auto & item) { return (item == old_value) ? new_value : item; });
}

template <class It, class Func, typename T>
void replace_if(It first1, const It & last1, const Func & f, const T & new_value)
{
    alg::for_each(first1, last1, [&f, &new_value](auto & item) { if (f(item)) item = new_value; });
}

template <class It, typename T>
void replace(It first1, const It & last1, const T & old_value, const T & new_value)
{
    alg::for_each(first1, last1, [&old_value, &new_value](auto & item) { if (item == old_value) item = new_value; });
}

template <class It1, class It2, class Func>
void _zipForEach(It1 first1, const It1 & last1, It2 first2, const Func & f)
{
    for (;first1 != last1; ++first1, ++first2)
        f(*first1, *first2);
}

template <class It1, class It2, class Func>
alg::_ifAllRAIt<void, It1, It2> zip_for_each(It1 first1, const It1 & last1, It2 first2, const Func & f)
{
    size_t * splited = alg::_splitSize(last1 - first1);
    for (uint8_t i = 0; i < _num_of_threads; first1 += splited[i], first2 += splited[i], ++i)
    {
        alg::_threads[i] = std::thread(alg::_zipForEach<It1, It2, Func>, first1, first1 + splited[i], first2, std::cref(f));
    }
    _joinAllThreads();
    delete[] splited;
}

template <class It1, class It2, class Func>
alg::_ifAnyNotRAIt<void, It1, It2> zip_for_each(It1 first1, const It1 & last1, It2 first2, const Func & f)
{
    std::thread * beg = alg::_threads;
    std::thread * end = alg::_threads + alg::_num_of_threads;
    for (; beg != end; ++first1, ++first2, ++beg)
    {
        *beg = std::thread(alg::_zipForEach<alg::_IteratorWrapper<It1>, alg::_IteratorWrapper<It2>, Func>,
                           alg::_IteratorWrapper<It1>(first1, last1),
                           alg::_IteratorWrapper<It1>(last1),
                           alg::_IteratorWrapper<It2>(first2), std::cref(f));
    }
    _joinAllThreads();
}

}
#endif // ALG_H
