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


template <class RAIt>
RAIt * _split(RAIt first, RAIt last)
{
    auto len = first - last;
    auto div = len / _num_of_threads;
    auto mod = len % _num_of_threads;
    RAIt * res = new RAIt[_num_of_threads + 1];
    res[0] = first;
    size_t * splitedSize = _splitSize(len);
    std::transform(res, res + _num_of_threads, splitedSize, res + 1, std::plus<>());
    delete[] splitedSize;
    return res;
}

size_t * _splitSize(size_t n)
{
    size_t div = n / _num_of_threads;
    size_t mod = n % _num_of_threads;
    size_t * res = new size_t[_num_of_threads];
    for (uint8_t i = 0; i < _num_of_threads; ++i)
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

template <class It, class Func>
void _inThreadFindIfRAIt(It first, const It & last, const Func & f, std::atomic<It> & result, const It & trueLast)
{
    for (; (first != last) && (result == trueLast); ++first)
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
        for (uint8_t i = 0; (i < _num_of_threads) && (first != last); ++i, ++first);
        if (result != last)
            return;
        if (f(*first))
        {
            result = first;
            return;
        }
    }
}

template <class It, class Func>
_ifRAIt<It, It> find_any_if(It first, const It & last, const Func & f)
{
    It * splited = _split(first, last);
    std::atomic<It> result = last;
    for (uint8_t i = 0; i < _num_of_threads; ++i)
    {
        _threads[i] = std::thread(_inThreadFindIfRAIt<It, Func>,
                                  splited[i],
                                  std::cref(splited[i + 1]),
                                  std::cref(f),
                                  std::ref(result),
                                  std::cref(last));
    }
    _joinAllThreads();
    delete[] splited;
    return static_cast<It>(result);
}

template <class It, class Func>
_ifnotRAIt<It, It> find_any_if(It first, const It & last, const Func & f)
{
    std::atomic<It> result = last;
    for (uint8_t i = 0; i < _num_of_threads; ++i, ++first)
    {
        _threads[i] = std::thread(_inThreadFindIfRAIt<It, Func>,
                                  first,
                                  std::cref(last),
                                  std::cref(f),
                                  std::ref(result));
    }
    _joinAllThreads();
    return static_cast<It>(result);
}

template <class It, class T, class Func>
It find_any(const It & first, const It & last, const T & item)
{
    return find_any_if(first, last, std::bind(std::equal_to<>(), std::placeholders::_1, item));
}

template <class It, class Func>
It find_any_if_not(const It & first, const It & last, const Func & f)
{
    return find_any_if(first, last, std::not_fn(f));
}

template <class It, class Func>
bool all_of(const It & first, const It & last, const Func & f)
{
    It check = find_any_if(first, last, std::not_fn(f));
    return check == last;
}

template <class It, class Func>
bool any_of(const It & first, const It & last, const Func & f)
{
    It check = find_any_if(first, last, f);
    return check != last;
}

template <class It, class Func>
bool none_of(const It & first, const It & last, const Func & f)
{
    It check = find_any_if(first, last, f);
    return check == last;
}

template <class It, class Func>
_ifRAIt<It, Func> for_each(It first, const It & last, const Func & f)
{
    It * splited = _split(first, last);
    for (uint8_t i = 0; i < _num_of_threads; ++i)
    {
        _threads[i] = std::thread(std::for_each<It, Func>, splited[i], splited[i + 1], f);
    }
    _joinAllThreads();
    delete[] splited;
    return std::move(f);
}

template <class It, class Func>
_ifnotRAIt<It, Func> for_each(It first, const It & last, const Func & f)
{
    std::thread * beg = _threads;
    std::thread * end = _threads + _num_of_threads;
    for (;beg != end; ++first, ++beg)
    {
        *beg = std::thread(std::for_each<_IteratorWrapper<It>, Func>, _IteratorWrapper<It>(first, last), _IteratorWrapper<It>(last), f);
    }
    return std::move(f);
}

template <class It, class Func>
_ifRAIt<It, uint> count_if(It first, const It & last, const Func & f)
{
    It * splited = _split(first, last);
    std::future<uint> * results = new std::future<uint>[_num_of_threads];
    for (uint8_t i = 0; i < _num_of_threads; ++i)
    {
        results[i] = std::async(std::count_if<It, Func>, splited[i], splited[i + 1], std::cref(f));
    }
    delete[] splited;
    delete[] results;
    return std::accumulate(results, results + _num_of_threads, 0, std::bind(std::plus<>(),
                                                                            std::bind(std::mem_fn(&std::future<uint>::get),
                                                                                      std::placeholders::_1),
                                                                            std::bind(std::mem_fn(&std::future<uint>::get),
                                                                                      std::placeholders::_2)));
}

template <class It, class Func>
_ifnotRAIt<It, uint> count_if(It first, const It & last, const Func & f)
{
    std::future<uint> * results = new std::future<uint>[_num_of_threads];
    std::future<uint> * beg = results;
    std::future<uint> * end = results + _num_of_threads;
    for (; beg != end; ++beg, ++first)
    {
        *beg = std::async(std::count_if<_IteratorWrapper<It>, Func>,
                          _IteratorWrapper<It>(first, last),
                          _IteratorWrapper<It>(last),
                          std::cref(f));
    }
    return std::accumulate(results, results + _num_of_threads, 0, std::bind(std::plus<>(),
                                                                            std::bind(std::mem_fn(&std::future<uint>::get),
                                                                                      std::placeholders::_1),
                                                                            std::bind(std::mem_fn(&std::future<uint>::get),
                                                                                      std::placeholders::_2)));
}

template <class It>
void _inThreadAnyMismatch(It first1, const It & last1, It first2, const It & trueLast, std::atomic<std::pair<It, It>> & result)
{
    for(; (first1 != last1); ++first1, ++first2)
    {
        if (result.first != trueLast)
            return;
        if (*first1 != *first2)
        {
            result = std::make_pair(first1, first2);
            return;
        }
    }
}

template <class It>
_ifRAIt<It, std::pair<It, It>> mismatch_any(It first1, const It & last1, It first2)
{
    It * splited = _split(first1, last1);
    std::atomic<std::pair<It, It>> result = std::make_pair(last1, last1);
    for (uint8_t i = 0; i < _num_of_threads; ++i, first2 += (splited[i] - splited[i - 1]))
    {
        _threads[i] = std::thread(_inThreadAnyMismatch<It>,
                                  splited[i],
                                  splited[i + 1],
                                  first2,
                                  std::cref(last1),
                                  std::ref(result));
    }
    _joinAllThreads();
    delete[] splited;
    return static_cast<std::pair<It, It>>(result);
}

template <class It>
_ifnotRAIt<It, std::pair<It, It>> mismatch_any(It first1, const It & last1, It first2)
{
    std::atomic<std::pair<It, It>> result = std::make_pair(last1, last1);
    std::thread * beg = _threads;
    std::thread * end = _threads + _num_of_threads;
    for (;beg != end; ++beg, ++first1, ++first2)
    {
        *beg = std::thread(_inThreadAnyMismatch<It>, first1, std::cref(last1), first2, std::cref(last1), std::ref(result));
    }
    _joinAllThreads();
    return static_cast<std::pair<It, It>>(result);
}

template <class It>
bool equal(It first1, const It & last1, It first2)
{
    std::pair<It, It> check = mismatch_any(first1, last1, first2);
    return check.first == last1;
}

template <class InputIt, class OutputIt, class Func>
_ifAllRAIt<void, InputIt, OutputIt> transform(InputIt first1, const InputIt & last1, OutputIt first2, const Func & f)
{
    size_t * splited = _splitSize(last1 - first1);
    for (uint8_t i = 0; i < _num_of_threads; first1 += splited[i], first2 += splited[i], ++i)
    {
        _threads[i] = std::thread(std::transform<InputIt, OutputIt, Func>, first1, last1, first2, f);
    }
    _joinAllThreads();
    delete [] splited;
}

template <class InputIt, class OutputIt, class Func>
_ifAnyNotRAIt<void, InputIt, OutputIt> transform(InputIt first1, const InputIt & last1, OutputIt first2, const Func & f)
{
    for (uint8_t i = 0; i < _num_of_threads; ++first1, ++first2, ++i)
    {
        _threads[i] = std::thread(std::transform<_IteratorWrapper<InputIt>, _IteratorWrapper<OutputIt>, Func>,
                                  _IteratorWrapper<InputIt>(first1, last1),
                                  _IteratorWrapper<InputIt>(last1),
                                  _IteratorWrapper<OutputIt>(first2), f);
    }
    _joinAllThreads();
}

template <class InputIt1, class InputIt2, class OutputIt, class Func>
_ifAllRAIt<void, InputIt1, InputIt2, OutputIt> transform(InputIt1 first1, const InputIt1 & last1, InputIt2 first2, OutputIt first3, const Func & f)
{
    size_t * splited = _splitSize(last1 - first1);
    for (uint8_t i = 0; i < _num_of_threads; first1 += splited[i], first2 += splited[i], first3 += splited, ++i)
    {
        _threads[i] = std::thread(std::transform<InputIt1, InputIt2, OutputIt, Func>, first1, last1, first2, first3, f);
    }
    _joinAllThreads();
    delete [] splited;
}

template <class InputIt1, class InputIt2, class OutputIt, class Func>
_ifAnyNotRAIt<void, InputIt1, InputIt2, OutputIt> transform(InputIt1 first1, const InputIt1 & last1, InputIt2 first2, OutputIt first3, const Func & f)
{
    for (uint8_t i = 0; i < _num_of_threads; ++first1, ++first2, ++i)
    {
        _threads[i] = std::thread(std::transform, _IteratorWrapper<InputIt1>(first1, last1),
                                                  _IteratorWrapper<InputIt1>(last1),
                                                  _IteratorWrapper<InputIt2>(first2),
                                                  _IteratorWrapper<OutputIt>(first3),  f);
    }
    _joinAllThreads();
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
_ifAllRAIt<void, InputIt, OutputIt> transform_n(InputIt first1, size_t n, OutputIt first2, const Func & f)
{
    size_t * splited = _splitSize(n);
    for (uint8_t i = 0; i < _num_of_threads; first1 += splited[i], first2 += splited[i], ++i)
    {
        _threads[i] = std::thread(_oneThreadTransformN<InputIt, OutputIt, Func>, first1, splited[i], first2, std::cref(f));
    }
    _joinAllThreads();
    delete [] splited;
}

template <class InputIt, class OutputIt, class Func>
_ifAnyNotRAIt<void, InputIt, OutputIt> transform_n(InputIt first1, size_t n, OutputIt first2, const Func & f)
{
    size_t * splited = _splitSize(n);
    for (uint8_t i = 0; i < _num_of_threads; ++first1, ++first2, ++i)
    {
        _threads[i] = std::thread(_oneThreadTransformN<_IteratorWrapper<InputIt>, _IteratorWrapper<OutputIt>, Func>,
                                  _IteratorWrapper<InputIt>(first1),
                                  splited[i],
                                  _IteratorWrapper<OutputIt>(first2), std::cref(f));
    }
    _joinAllThreads();
    delete [] splited;
}

template <class InputIt1, class InputIt2, class OutputIt, class Func>
_ifAllRAIt<void, InputIt1, InputIt2, OutputIt> transform_n(InputIt1 first1, size_t n, InputIt2 first2, OutputIt first3, const Func & f)
{
    size_t * splited = _splitSize(n);
    for (uint8_t i = 0; i < _num_of_threads; first1 += splited[i], first2 += splited[i], ++i)
    {
        _threads[i] = std::thread(_oneThreadTransformN<InputIt1, InputIt2, OutputIt, Func>, first1, splited[i], first2, first3, std::cref(f));
    }
    _joinAllThreads();
    delete [] splited;
}

template <class InputIt1, class InputIt2, class OutputIt, class Func>
_ifAnyNotRAIt<void, InputIt1, InputIt2, OutputIt> transform_n(InputIt1 first1, size_t n, InputIt2 first2, OutputIt first3, const Func & f)
{
    size_t * splited = _splitSize(n);
    for (uint8_t i = 0; i < _num_of_threads; ++first1, ++first2, ++first3, ++i)
    {
        _threads[i] = std::thread(_oneThreadTransformN<_IteratorWrapper<InputIt1>, _IteratorWrapper<InputIt2>, _IteratorWrapper<OutputIt>, Func>,
                                  _IteratorWrapper<InputIt1>(first1),
                                  splited[i],
                                  _IteratorWrapper<InputIt2>(first2),
                                  _IteratorWrapper<OutputIt>(first3), std::cref(f));
    }
    _joinAllThreads();
    delete [] splited;
}

template <class InputIt, class OutputIt>
void copy(InputIt first1, const InputIt & last1, OutputIt first2)
{
    transform(first1, last1, first2, [](const auto & item) { return item; });
}

template <class InputIt, class OutputIt>
void copy_n(InputIt first1, size_t n, OutputIt first2)
{
    transform_n(first1, n, first2, [](const auto & item) { return item; });
}

template <class InputIt, class OutputIt>
void copy_backward(InputIt first1, const InputIt & last1, OutputIt last2)
{
    transform(first1, last1, std::make_reverse_iterator(last2), [](const auto & item) { return item; });
}

template <class InputIt, class OutputIt>
void move(InputIt first1, const InputIt & last1, OutputIt first2)
{
    transform(first1, last1, first2, [](const auto & item) { return std::move(item); });
}

template <class InputIt, class OutputIt>
void move_backward(InputIt first1, const InputIt & last1, OutputIt last2)
{
    transform(first1, last1, std::make_reverse_iterator(last2), [](const auto & item) { return std::move(item); });
}

template <class It, typename T>
void fill(It first, const It & last, const T & item)
{
    transform(first, last, first, [&item](const auto &) { return item; });
}

template <class It, typename T>
void fill_n(It first, size_t n, const T & item)
{
    transform_n(first, n, first, [&item](const auto &) { return item; });
}

template <class It, class Func>
void generate(It first, const It & last, const Func & f)
{
    transform(first, last, first, [&f](const auto &) { return f(); });
}

template <class It, class Func>
void generate_n(It first, size_t n, const Func & f)
{
    transform_n(first, n, first, [&f](const auto &) { return f(); });
}

template <class InputIt, class OutputIt>
void reverce_copy(InputIt first1, const InputIt & last1, OutputIt first2)
{
    transform(std::make_reverse_iterator(last1), std::make_reverse_iterator(first1), first2, [](const auto & item) { return item;});
}

template <class InputIt, class OutputIt, class Func, typename T>
void replace_copy_if(InputIt first1, const InputIt & last1, OutputIt first2, const Func & f, const T & new_value)
{
    transform(first1, last1, first2, [&f, &new_value](const auto & item) { return f(item) ? new_value : item; });
}

template <class InputIt, class OutputIt, typename T>
void replace_copy(InputIt first1, const InputIt & last1, OutputIt first2, const T & old_value, const T & new_value)
{
    transform(first1, last1, first2, [&old_value, &new_value](const auto & item) { return (item == old_value) ? new_value : item; });
}

template <class It, class Func, typename T>
void replace_if(It first1, const It & last1, const Func & f, const T & new_value)
{
    for_each(first1, last1, [&f, &new_value](auto & item) { if (f(item)) item = new_value; });
}

template <class It, typename T>
void replace_if(It first1, const It & last1, const T & old_value, const T & new_value)
{
    for_each(first1, last1, [&old_value, &new_value](auto & item) { if (item == old_value) item = new_value; });
}

}
#endif // ALG_H
