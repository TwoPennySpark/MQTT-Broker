#ifndef PACK_H
#define PACK_H

#include <vector>
#include <stdint.h>
#include <iostream>

template <typename T>
void pack(std::vector<uint8_t>& buf, uint32_t& iterator, const T& val)
{
    uint8_t size = sizeof(T);
    for (uint8_t i = 1; i <= size; i++)
        buf[iterator++] = val >> (size-i)*8;
//    {
//        **buf = val >> (size-i)*8;
//        (*buf)++;
//    }
}

template <>
inline void pack(std::vector<uint8_t>& buf, uint32_t& iterator, const std::string& val)
{
    for (uint32_t i = 0; i < val.size(); i++)
        pack(buf, iterator, val[i]);
}

template <typename T>
inline void pack(std::vector<uint8_t>& buf, uint32_t& iterator, const std::vector<T>& val)
{
    for (uint32_t i = 0; i < val.size(); i++)
        pack(buf, iterator, val[i]);
}

template <typename T>
void unpack(const std::vector<uint8_t>& buf, uint32_t& iterator, T& val)
{
    uint8_t size = sizeof(T);
//    for (uint8_t i = size; i > 0; i--)
//        val |= buf[iterator--] << (size-i)*8;

    for (uint8_t i = 0; i < size; i++)
        val |= buf[iterator++] << (size-i-1)*8;
}

template <>
inline void unpack(const std::vector<uint8_t>& buf, uint32_t& iterator, std::string& val)
{
    for (uint32_t i = 0; i < val.size(); i++)
        unpack(buf, iterator, val[i]);
}

template <typename T>
inline void unpack(const std::vector<uint8_t>& buf, uint32_t& iterator, std::vector<T>& val)
{
    for (uint32_t i = 0; i < val.size(); i++)
        unpack(buf, iterator, val[i]);
}

#endif // PACK_H
