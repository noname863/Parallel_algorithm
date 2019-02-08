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
    _IteratorWrapper<It> & operator++();
    _IteratorWrapper<It> operator++(int);
    _IteratorWrapper<It> & operator--();
    _IteratorWrapper<It> operator--(int);

    using It::operator=;
    _IteratorWrapper(const It & original) : It(original) { this->useLast = false; }
    _IteratorWrapper(It original, It last);
};

template<class It>
_IteratorWrapper<It>::_IteratorWrapper(It original, It last)
{
    It::It(original);
    this->last = last;
    this->useLast = true;
}

template<class It>
_IteratorWrapper<It> & _IteratorWrapper<It>::operator++()
{
    for(uint8_t i = 0; (i < _num_of_threads) && (!useLast || (*this != last)); ++i)
        It::operator++();
    return *this;
}

template<class It>
_IteratorWrapper<It> _IteratorWrapper<It>::operator++(int)
{
    _IteratorWrapper<It> temp = *this;
    ++*this;
    return temp;
}

template<class It>
_IteratorWrapper<It> & _IteratorWrapper<It>::operator--()
{
    for(uint8_t i = 0; (i < _num_of_threads) && (!useLast || (*this != last)); ++i)
        It::operator--();
    return *this;
}

template<class It>
_IteratorWrapper<It> _IteratorWrapper<It>::operator--(int)
{
    _IteratorWrapper<It> temp = *this;
    --*this;
    return temp;
}




}

#endif // ITERATORWRAPPER_HPP
