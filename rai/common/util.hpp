#pragma once

#include <array>
#include <assert.h>
#include <chrono>
#include <fstream>
#include <mutex>
#include <streambuf>
#include <string>
#include <type_traits>
#include <vector>
#include <boost/endian/conversion.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/iostreams/device/file.hpp>
#include <boost/iostreams/stream.hpp>

namespace
{
template <typename T>
struct is_bytes : std::false_type
{
};

template <size_t N>
struct is_bytes<std::array<uint8_t, N>> : std::true_type
{
};

template <>
struct is_bytes<std::vector<uint8_t>> : std::true_type
{
};

template <typename T>
struct is_bytes_array : std::false_type
{
};

template <size_t N>
struct is_bytes_array<std::array<uint8_t, N>> : std::true_type
{
};

}  // namespace

namespace rai
{
using Stream = std::basic_streambuf<uint8_t>;

template <typename T>
typename std::enable_if<!is_bytes_array<T>::value && !std::is_enum<T>::value,
                        bool>::type
Read(rai::Stream& stream, T& value)
{
    static_assert(std::is_pod<T>::value,
                  "Can't stream read non-standard layout types");
    auto num(stream.sgetn(reinterpret_cast<uint8_t*>(&value), sizeof(value)));
    boost::endian::big_to_native_inplace(value);
    return num != sizeof(value);
}

template <typename T>
typename std::enable_if<std::is_enum<T>::value, bool>::type Read(
    rai::Stream& stream, T& value)
{
    typedef typename std::underlying_type<T>::type EnumBaseType;
    EnumBaseType value_l;
    bool ret = Read(stream, value_l);
    value = static_cast<T>(value_l);
    return ret;
}

template <typename T>
typename std::enable_if<is_bytes_array<T>::value, bool>::type Read(
    rai::Stream& stream, T& value)
{
    static_assert(std::is_pod<T>::value,
                  "Can't stream read non-standard layout types");
    auto num(
        stream.sgetn(reinterpret_cast<uint8_t*>(value.data()), value.size()));
    return num != sizeof(value);
}

template <typename T>
typename std::enable_if<!is_bytes<T>::value && !std::is_enum<T>::value>::type
Write(rai::Stream& stream, T const& value)
{
    static_assert(std::is_pod<T>::value,
                  "Can't stream write non-standard layout types");
    T value_l = boost::endian::native_to_big(value);
    auto num(stream.sputn(reinterpret_cast<const uint8_t*>(&value_l),
                          sizeof(value_l)));
    assert(num == sizeof(value_l));
}

template <typename T>
typename std::enable_if<std::is_enum<T>::value>::type Write(rai::Stream& stream,
                                                            T const& value)
{
    typedef typename std::underlying_type<T>::type EnumBaseType;
    EnumBaseType value_l = static_cast<EnumBaseType>(value);
    Write(stream, value_l);
}

template <typename T>
typename std::enable_if<is_bytes<T>::value>::type Write(rai::Stream& stream,
                                                        T const& value)
{
    auto num(stream.sputn(reinterpret_cast<const uint8_t*>(value.data()),
                          value.size()));
    assert(num == value.size());
}

bool Read(rai::Stream& stream, size_t size, std::vector<uint8_t>& data);

bool StreamEnd(rai::Stream&);

template <typename T>
bool ReadFile(std::ifstream& stream, T& value)
{
    static_assert(std::is_pod<T>::value,
                  "Can't stream read non-standard layout types");
    stream.read(reinterpret_cast<char*>(&value), sizeof(value));
    return !stream.good();
}

template <typename T>
void WriteFile(std::ofstream& stream, T const& value)
{
    static_assert(std::is_pod<T>::value,
                  "Can't stream write non-standard layout types");
    stream.write(reinterpret_cast<const char*>(&value), sizeof(value));
}


using BufferStream = boost::iostreams::stream_buffer<
    boost::iostreams::basic_array_source<uint8_t>>;
using VectorStream = boost::iostreams::stream_buffer<
    boost::iostreams::back_insert_device<std::vector<uint8_t>>>;
using Ptree = boost::property_tree::ptree;

std::string BytesToHex(const uint8_t* data, size_t size);
void DumpBytes(const uint8_t* data, size_t size);

template <typename T>
bool StringToUint(const std::string& str, T& value)
{
    if (str.find_first_not_of("0123456789") != std::string::npos)
    {
        return true;
    }

    if ((str.size() > 1) && ('0' == str[0]))
    {
        return true;
    }

    try
    {
        unsigned long long ull = std::stoull(str);
        if (ull > std::numeric_limits<T>::max())
        {
            return true;
        }

        value = static_cast<T>(ull);
    }
    catch (const std::exception&)
    {
        return true;
    }

    return false;
}

inline uint64_t CurrentTimestamp()
{
    auto result = std::chrono::duration_cast<std::chrono::seconds>(
                      std::chrono::system_clock::now().time_since_epoch())
                      .count();
    return static_cast<uint64_t>(result);
}

inline uint64_t CurrentTimestampMilliseconds()
{
    auto result = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::system_clock::now().time_since_epoch())
                      .count();
    return static_cast<uint64_t>(result);
}

inline bool SameDay(uint64_t t1, uint64_t t2)
{
    uint64_t constexpr day = 24 * 60 * 60;
    return t1 / day == t2 / day;
}

template <typename... T>
class ObserverContainer
{
public:
    void Add(const std::function<void(T...)>& observer)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        observers_.push_back(observer);
    }

    void Notify(T... args)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& i : observers_)
        {
            i(args...);
        }
    }

private:
    std::mutex mutex_;
    std::vector<std::function<void(T...)>> observers_;
};

}  // namespace rai

#define IF_ERROR_RETURN(error, ret) \
    if (error)                      \
    {                               \
        return ret;                 \
    }
    
#define RAI_TODO 0
