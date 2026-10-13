#pragma once
#include <memory>
#include <queue>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

template <typename T>
using TArray = std::vector<T>;

template <typename T>
using TDoubleLinkedList = std::list<T>;

using TBitArray = std::vector<bool>;

template <typename Key, typename Value>
using TMap = std::unordered_map<Key, Value>;

template <typename Key, typename Value>
using TMultimap = std::unordered_map<Key, Value>;

template <typename T>
using TSet = std::unordered_set<T>;

template <typename T>
using TQueue = std::queue<T>;

template <typename T>
using TSharedPtr = std::shared_ptr<T>;

template <typename T>
using TWeakPtr = std::weak_ptr<T>;

template <typename T>
using TUniquePtr = std::unique_ptr<T>;

using FString = std::string;

using int8 = int8_t;
using uint8 = uint8_t;
using int16 = int16_t;
using uint16 = uint16_t;
using int32 = int32_t;
using uint32 = uint32_t;
using int64 = int64_t;
using uint64 = uint64_t;
using TCHAR = wchar_t;
using ANSICHAR = char;
using UTF8CHAR = char8_t;
using UTF16CHAR = char16_t;
using UTF32CHAR = char32_t;
using SIZE_T = std::size_t;
using PTRINT = std::intptr_t;
using UPTRINT = std::uintptr_t;
