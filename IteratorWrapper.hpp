#ifndef ITERATORWRAPPER_HPP
#define ITERATORWRAPPER_HPP
#include "algThreads.hpp"

namespace alg {

template <class It>
class _IteratorWrapper : public It
{
private:
    It last;
    bool useLast;
public:
    alg::_IteratorWrapper<It> & operator++();
    alg::_IteratorWrapper<It> operator++(int);
    alg::_IteratorWrapper<It> & operator--();
    alg::_IteratorWrapper<It> operator--(int);

    using It::operator=;
    _IteratorWrapper(const It & original) : It(original) { this->useLast = false; }
    _IteratorWrapper(It original, It last) : It(original) { this->useLast = true; this->last = last; }
};

template<class It>
alg::_IteratorWrapper<It> & alg::_IteratorWrapper<It>::operator++()
{
    for(uint8_t i = 0; (i < alg::_num_of_threads) && (!useLast || (*this != last)); ++i)
        It::operator++();
    return *this;
}

template<class It>
alg::_IteratorWrapper<It> alg::_IteratorWrapper<It>::operator++(int)
{
    alg::_IteratorWrapper<It> temp = *this;
    ++*this;
    return temp;
}

template<class It>
alg::_IteratorWrapper<It> & alg::_IteratorWrapper<It>::operator--()
{
    for(uint8_t i = 0; (i < alg::_num_of_threads) && (!useLast || (*this != last)); ++i)
        It::operator--();
    return *this;
}

template<class It>
alg::_IteratorWrapper<It> alg::_IteratorWrapper<It>::operator--(int)
{
    alg::_IteratorWrapper<It> temp = *this;
    --*this;
    return temp;
}

}

#endif // ITERATORWRAPPER_HPP
