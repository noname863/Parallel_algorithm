#ifndef ALL_IS_SAME_HPP
#define ALL_IS_SAME_HPP
#include <type_traits>

template <typename first, typename second, typename... Types>
struct all_is_same
{
    constexpr static bool value = std::is_same<first, second>::value && all_is_same<first, Types...>::value;
};

template <typename first, typename second>
struct all_is_same<first, second>
{
    constexpr static bool value = std::is_same<first, second>::value;
};

#endif // ALL_IS_SAME_HPP
