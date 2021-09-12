#pragma once

#include <algorithm>
#include <array>
#include <assert.h>
#include <chrono>
#include <fstream>
#include <mutex>
#include <streambuf>
#include <string>
#include <sstream>
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
    if (value.size() == 0)
    {
        return;
    }
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
bool HexToBytes(const std::string&, std::vector<uint8_t>&);
void DumpBytes(const uint8_t* data, size_t size);

template <typename T>
bool StringToUint(const std::string& str, T& value)
{
    if (str.empty())
    {
        return true;
    }
    
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
        if (std::to_string(ull) != str)
        {
            return true;
        }

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

inline uint64_t DayBegin(uint64_t ts)
{
    return ts - ts % 86400;
}

inline uint64_t DayEnd(uint64_t ts)
{
    return ts - ts % 86400 + 86400;
}

inline bool StringContain(const std::string& str, char c)
{
    return str.find(c) != std::string::npos;
}

inline size_t StringCount(const std::string& str, char c)
{
    size_t count = 0;
    for (auto it = str.begin(); it != str.end(); ++it)
    {
        if (c == *it)
        {
            ++count;
        }
    }
    return count;
}

void StringLeftTrim(std::string&, const std::string&);
void StringRightTrim(std::string&, const std::string&);
void StringTrim(std::string&, const std::string&);


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

template <typename T>
void ToStringStream(std::stringstream& stream, T value)
{
    stream << value;
}

template<typename T, typename... Args>
void ToStringStream(std::stringstream& stream, T value, Args... args)
{
    stream << value;
    ToStringStream(stream, args...);
}

template<typename... Args>
std::string ToString(Args... args)
{
    std::stringstream stream;
    ToStringStream(stream, args...);
    return stream.str();
}

template <class Container>
bool Contain(const Container& container,
              const typename Container::value_type& element)
{
    return std::find(container.begin(), container.end(), element)
           != container.end();
}

// only for config parsing
class Url
{
public:
    Url();
    Url(const std::string&);
    void AddQuery(const std::string&, const std::string&);
    bool Parse(const std::string&);
    std::string String() const;
    uint16_t DefaultPort() const;
    explicit operator bool() const;
    bool CheckProtocol();

    std::string protocol_;
    std::string host_;
    uint16_t port_;
    std::string path_;

};

}  // namespace rai

#define IF_ERROR_RETURN(error, ret) \
    if (error)                      \
    {                               \
        return ret;                 \
    }

#define IF_ERROR_RETURN_VOID(error) \
    if (error)                      \
    {                               \
        return;                     \
    }

#define IF_ERROR_BREAK(error) \
    if (error)                \
    {                         \
        break;                \
    }

#define RAI_TODO 1
