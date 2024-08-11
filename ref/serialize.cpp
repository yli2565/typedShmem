#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <sstream>
#include <type_traits>
// Utility traits to detect vectors and maps
// Forward declarations
template <typename T>
std::string serializePrimitive(const T &value);

template <typename T>
std::string serializeVector(const std::vector<T> &vec);

template <typename K, typename V>
std::string serializeMap(const std::map<K, V> &map);

template <typename T>
std::string serialize(const T &value);

// Utility traits to detect vectors and maps
template <typename T>
struct is_vector : std::false_type
{
};
// template<typename T> struct is_vector<std::vector<T>> : std::true_type {};
template <typename T, typename A>
struct is_vector<std::vector<T, A>> : std::true_type
{
};

template <typename T>
struct is_map : std::false_type
{
};
template <typename K, typename V, typename C, typename A>
struct is_map<std::map<K, V, C, A>> : std::true_type
{
};

// Function to serialize primitive types (integers, doubles, strings, etc.)
template <typename T>
std::string serializePrimitive(const T &value)
{
    if constexpr (std::is_same<T, std::string>::value)
    {
        return "\"" + value + "\"";
    }
    else
    {
        return std::to_string(value);
    }
}

// Function to serialize vectors
template <typename T>
std::string serializeVector(const std::vector<T> &vec)
{
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < vec.size(); ++i)
    {
        oss << serialize(vec[i]);
        if (i < vec.size() - 1)
        {
            oss << ",";
        }
    }
    oss << "]";
    return oss.str();
}

// Function to serialize maps
template <typename K, typename V>
std::string serializeMap(const std::map<K, V> &map)
{
    std::ostringstream oss;
    oss << "{";
    auto it = map.begin();
    while (it != map.end())
    {
        oss << "\"" << it->first << "\": " << serialize(it->second);
        if (++it != map.end())
        {
            oss << ",";
        }
    }
    oss << "}";
    return oss.str();
}

// Overloaded serialize function
template <typename T>
std::string serialize(const T &value)
{
    if constexpr (std::is_arithmetic<T>::value || std::is_same<T, std::string>::value)
    {
        return serializePrimitive(value);
    }
    else if constexpr (is_vector<T>::value)
    {
        return serializeVector(value);
    }
    else if constexpr (is_map<T>::value)
    {
        return serializeMap(value);
    }
    else
    {
        static_assert(std::is_same<T, void>::value, "Unsupported type for serialization");
    }
}

// Example usage
int main()
{
    std::vector<int> vec = {1, 2, 3};
    std::map<std::string, int> myMap = {{"one", 1}, {"two", 2}, {"three", 3}};
    std::map<std::string, std::vector<int>> nestedMap = {{"numbers", vec}};

    std::vector<int> myVec;
    // std::cout << is_vector<decltype(myVec)>::value << std::endl;
    std::string serializedVec = serialize(vec);
    std::string serializedMap = serialize(myMap);
    std::string serializedNestedMap = serialize(nestedMap);

    std::cout << "Serialized vector: " << serializedVec << std::endl;
    std::cout << "Serialized map: " << serializedMap << std::endl;
    std::cout << "Serialized nested map: " << serializedNestedMap << std::endl;

    return 0;
}
