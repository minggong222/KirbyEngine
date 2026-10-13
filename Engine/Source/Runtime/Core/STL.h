#pragma once
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <queue>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

// Lightweight Unreal-style compatibility wrappers for a custom C++ engine.
// This is a useful subset, not a full reimplementation of Unreal Engine's Core types.

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
// -----------------------------------------------------------------------------
// TArray
// -----------------------------------------------------------------------------
template <typename T>
class TArray
{
private:
    using FStorage = std::vector<T>;

public:
    using ElementType = T;
    using SizeType = int32;
    using Iterator = FStorage::iterator;
    using ConstIterator = FStorage::const_iterator;

    TArray() = default;
    TArray(std::initializer_list<T> Init) : Elements(Init) {}

    int32 Add(const T& Value)
    {
        const int32 Index = Num();
        Elements.push_back(Value);
        return Index;
    }

    int32 Add(T&& Value)
    {
        const int32 Index = Num();
        Elements.push_back(std::move(Value));
        return Index;
    }

    int32 AddUnique(const T& Value)
    {
        const int32 ExistingIndex = Find(Value);
        return ExistingIndex != INDEX_NONE ? ExistingIndex : Add(Value);
    }

    template <typename... TArgs>
    int32 Emplace(TArgs&&... Args)
    {
        const int32 Index = Num();
        Elements.emplace_back(std::forward<TArgs>(Args)...);
        return Index;
    }

    int32 Num() const { return static_cast<int32>(Elements.size()); }
    bool IsEmpty() const { return Elements.empty(); }
    bool IsValidIndex(int32 Index) const { return Index >= 0 && Index < Num(); }

    void Reserve(int32 Capacity)
    {
        assert(Capacity >= 0);
        Elements.reserve(static_cast<std::size_t>(Capacity));
    }

    void SetNum(int32 NewNum)
    {
        assert(NewNum >= 0);
        Elements.resize(static_cast<std::size_t>(NewNum));
    }

    void SetNumZeroed(int32 NewNum)
    {
        assert(NewNum >= 0);
        const auto OldSize = Elements.size();
        Elements.resize(static_cast<std::size_t>(NewNum));
        if (NewNum > static_cast<int32>(OldSize))
        {
            for (std::size_t Index = OldSize; Index < Elements.size(); ++Index)
            {
                Elements[Index] = T{};
            }
        }
    }

    // Clear elements while retaining current capacity. NewSize is desired capacity slack.
    void Reset(int32 NewSize = 0)
    {
        assert(NewSize >= 0);
        Elements.clear();
        if (static_cast<std::size_t>(NewSize) > Elements.capacity())
        {
            Elements.reserve(static_cast<std::size_t>(NewSize));
        }
    }

    // Release current storage, optionally reserving Slack elements for reuse.
    void Empty(int32 Slack = 0)
    {
        assert(Slack >= 0);
        FStorage NewStorage;
        if (Slack > 0)
        {
            NewStorage.reserve(static_cast<std::size_t>(Slack));
        }
        Elements.swap(NewStorage);
    }

    bool Contains(const T& Value) const
    {
        return std::find(Elements.begin(), Elements.end(), Value) != Elements.end();
    }

    int32 Find(const T& Value) const
    {
        const auto It = std::find(Elements.begin(), Elements.end(), Value);
        return It == Elements.end() ? INDEX_NONE : static_cast<int32>(std::distance(Elements.begin(), It));
    }

    int32 FindLast(const T& Value) const
    {
        const auto It = std::find(Elements.rbegin(), Elements.rend(), Value);
        return It == Elements.rend() ? INDEX_NONE : Num() - 1 - static_cast<int32>(std::distance(Elements.rbegin(), It));
    }

    template <typename Predicate>
    T* FindByPredicate(Predicate Pred)
    {
        const auto It = std::find_if(Elements.begin(), Elements.end(), Pred);
        return It == Elements.end() ? nullptr : &(*It);
    }

    template <typename Predicate>
    const T* FindByPredicate(Predicate Pred) const
    {
        const auto It = std::find_if(Elements.begin(), Elements.end(), Pred);
        return It == Elements.end() ? nullptr : &(*It);
    }

    // Removes every matching value and returns the number removed.
    int32 Remove(const T& Value)
    {
        const auto OldSize = Elements.size();
        Elements.erase(std::remove(Elements.begin(), Elements.end(), Value), Elements.end());
        return static_cast<int32>(OldSize - Elements.size());
    }

    bool RemoveSingle(const T& Value)
    {
        const auto It = std::find(Elements.begin(), Elements.end(), Value);
        if (It == Elements.end()) return false;
        Elements.erase(It);
        return true;
    }

    void RemoveAt(int32 Index, int32 Count = 1, bool bAllowShrinking = true)
    {
        if (Index < 0 || Count < 0 || Index > Num() || Count > Num() - Index)
        {
            assert(false && "Invalid TArray::RemoveAt range");
            return;
        }
        if (Count == 0) return;
        Elements.erase(Elements.begin() + Index, Elements.begin() + Index + Count);
        if (bAllowShrinking) Elements.shrink_to_fit();
    }

    void RemoveAtSwap(int32 Index, int32 Count = 1, bool bAllowShrinking = true)
    {
        if (Index < 0 || Count < 0 || Index > Num() || Count > Num() - Index)
        {
            assert(false && "Invalid TArray::RemoveAtSwap range");
            return;
        }
        for (int32 Removed = 0; Removed < Count; ++Removed)
        {
            const int32 CurrentIndex = Index;
            const int32 LastIndex = Num() - 1;
            if (CurrentIndex != LastIndex)
            {
                Elements[static_cast<std::size_t>(CurrentIndex)] = std::move(Elements.back());
            }
            Elements.pop_back();
        }
        if (bAllowShrinking) Elements.shrink_to_fit();
    }

    template <typename Predicate>
    int32 RemoveAll(Predicate Pred)
    {
        const auto OldSize = Elements.size();
        Elements.erase(std::remove_if(Elements.begin(), Elements.end(), Pred), Elements.end());
        return static_cast<int32>(OldSize - Elements.size());
    }

    void Insert(const T& Value, int32 Index)
    {
        assert(Index >= 0 && Index <= Num());
        Elements.insert(Elements.begin() + Index, Value);
    }

    void Insert(T&& Value, int32 Index)
    {
        assert(Index >= 0 && Index <= Num());
        Elements.insert(Elements.begin() + Index, std::move(Value));
    }

    void Append(const TArray& Other)
    {
        if (this == &Other)
        {
            const FStorage Copy = Elements;
            Elements.insert(Elements.end(), Copy.begin(), Copy.end());
            return;
        }
        Elements.insert(Elements.end(), Other.Elements.begin(), Other.Elements.end());
    }

    template <typename IteratorType>
    void Append(IteratorType First, IteratorType Last)
    {
        Elements.insert(Elements.end(), First, Last);
    }

    template <typename Predicate>
    TArray FilterByPredicate(Predicate Pred) const
    {
        TArray Result;
        for (const T& Value : Elements)
        {
            if (Pred(Value)) Result.Add(Value);
        }
        return Result;
    }

    void Sort()
    {
        std::sort(Elements.begin(), Elements.end());
    }

    template <typename Predicate>
    void Sort(Predicate Pred)
    {
        std::sort(Elements.begin(), Elements.end(), Pred);
    }

    template <typename Predicate>
    void StableSort(Predicate Pred)
    {
        std::stable_sort(Elements.begin(), Elements.end(), Pred);
    }

    T Pop(bool bAllowShrinking = false)
    {
        assert(!Elements.empty());
        T Value = std::move(Elements.back());
        Elements.pop_back();
        if (bAllowShrinking) Elements.shrink_to_fit();
        return Value;
    }

    T& Last(int32 IndexFromTheEnd = 0)
    {
        assert(IndexFromTheEnd >= 0 && IndexFromTheEnd < Num());
        return Elements[Elements.size() - 1 - static_cast<std::size_t>(IndexFromTheEnd)];
    }

    const T& Last(int32 IndexFromTheEnd = 0) const
    {
        assert(IndexFromTheEnd >= 0 && IndexFromTheEnd < Num());
        return Elements[Elements.size() - 1 - static_cast<std::size_t>(IndexFromTheEnd)];
    }

    T& operator[](int32 Index) { assert(IsValidIndex(Index)); return Elements[static_cast<std::size_t>(Index)]; }
    const T& operator[](int32 Index) const { assert(IsValidIndex(Index)); return Elements[static_cast<std::size_t>(Index)]; }
    T* GetData() { return Elements.data(); }
    const T* GetData() const { return Elements.data(); }

    Iterator begin() { return Elements.begin(); }
    Iterator end() { return Elements.end(); }
    ConstIterator begin() const { return Elements.begin(); }
    ConstIterator end() const { return Elements.end(); }
    ConstIterator cbegin() const { return Elements.cbegin(); }
    ConstIterator cend() const { return Elements.cend(); }

    // Common STL compatibility while migrating old code.
    void push_back(const T& Value) { Elements.push_back(Value); }
    void push_back(T&& Value) { Elements.push_back(std::move(Value)); }
    template <typename... Args> T& emplace_back(Args&&... args) { return Elements.emplace_back(std::forward<Args>(args)...); }
    std::size_t size() const { return Elements.size(); }
    bool empty() const { return Elements.empty(); }
    void clear() { Elements.clear(); }
    void reserve(std::size_t Capacity) { Elements.reserve(Capacity); }
    void resize(std::size_t NewSize) { Elements.resize(NewSize); }
    T& at(std::size_t Index) { return Elements.at(Index); }
    const T& at(std::size_t Index) const { return Elements.at(Index); }

    static constexpr int32 INDEX_NONE = -1;

private:
    FStorage Elements;
};

// -----------------------------------------------------------------------------
// TDoubleLinkedList: node-based API with range-for support.
// -----------------------------------------------------------------------------
template <typename T>
class TDoubleLinkedList
{
public:
    class TDoubleLinkedListNode
    {
        friend class TDoubleLinkedList<T>;
    public:
        explicit TDoubleLinkedListNode(const T& InValue) : Value(InValue) {}
        explicit TDoubleLinkedListNode(T&& InValue) : Value(std::move(InValue)) {}
        T& GetValue() { return Value; }
        const T& GetValue() const { return Value; }
        TDoubleLinkedListNode* GetNextNode() const { return Next; }
        TDoubleLinkedListNode* GetPrevNode() const { return Prev; }
    private:
        T Value;
        TDoubleLinkedListNode* Prev = nullptr;
        TDoubleLinkedListNode* Next = nullptr;
    };

    template <bool bConst>
    class TIteratorBase
    {
        using FNode = TDoubleLinkedListNode;
        using FNodePtr = std::conditional_t<bConst, const FNode*, FNode*>;
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = std::conditional_t<bConst, const T*, T*>;
        using reference = std::conditional_t<bConst, const T&, T&>;
        TIteratorBase() = default;
        explicit TIteratorBase(FNodePtr InNode) : Node(InNode) {}
        reference operator*() const { return Node->Value; }
        pointer operator->() const { return &Node->Value; }
        TIteratorBase& operator++() { Node = Node->Next; return *this; }
        TIteratorBase operator++(int) { auto Copy = *this; ++(*this); return Copy; }
        bool operator==(const TIteratorBase& Other) const { return Node == Other.Node; }
        bool operator!=(const TIteratorBase& Other) const { return !(*this == Other); }
    private:
        FNodePtr Node = nullptr;
    };

    using Iterator = TIteratorBase<false>;
    using ConstIterator = TIteratorBase<true>;

    TDoubleLinkedList() = default;
    TDoubleLinkedList(const TDoubleLinkedList&) = delete;
    TDoubleLinkedList& operator=(const TDoubleLinkedList&) = delete;
    TDoubleLinkedList(TDoubleLinkedList&& Other) noexcept { MoveFrom(std::move(Other)); }
    TDoubleLinkedList& operator=(TDoubleLinkedList&& Other) noexcept
    {
        if (this != &Other) { Empty(); MoveFrom(std::move(Other)); }
        return *this;
    }
    ~TDoubleLinkedList() { Empty(); }

    bool AddHead(const T& Value) { AddHeadNode(new TDoubleLinkedListNode(Value)); return true; }
    bool AddHead(T&& Value) { AddHeadNode(new TDoubleLinkedListNode(std::move(Value))); return true; }
    bool AddTail(const T& Value) { AddTailNode(new TDoubleLinkedListNode(Value)); return true; }
    bool AddTail(T&& Value) { AddTailNode(new TDoubleLinkedListNode(std::move(Value))); return true; }

    TDoubleLinkedListNode* GetHead() const { return Head; }
    TDoubleLinkedListNode* GetTail() const { return Tail; }
    int32 Num() const { return Count; }
    bool IsEmpty() const { return Count == 0; }

    void RemoveNode(TDoubleLinkedListNode* Node, bool bDeleteNode = true)
    {
        if (Node == nullptr) return;
        if (Node->Prev) Node->Prev->Next = Node->Next; else Head = Node->Next;
        if (Node->Next) Node->Next->Prev = Node->Prev; else Tail = Node->Prev;
        Node->Prev = Node->Next = nullptr;
        --Count;
        if (bDeleteNode) delete Node;
    }

    void RemoveNode(const T& Value)
    {
        for (auto* Node = Head; Node != nullptr; Node = Node->Next)
        {
            if (Node->Value == Value) { RemoveNode(Node); return; }
        }
    }

    void Empty()
    {
        while (Head != nullptr) RemoveNode(Head);
    }

    Iterator begin() { return Iterator(Head); }
    Iterator end() { return Iterator(nullptr); }
    ConstIterator begin() const { return ConstIterator(Head); }
    ConstIterator end() const { return ConstIterator(nullptr); }

    // Common std::list-style compatibility while migrating old code.
    void push_front(const T& Value) { AddHead(Value); }
    void push_back(const T& Value) { AddTail(Value); }
    bool empty() const { return IsEmpty(); }
    std::size_t size() const { return static_cast<std::size_t>(Count); }
    void clear() { Empty(); }
    T& front() { assert(Head); return Head->Value; }
    const T& front() const { assert(Head); return Head->Value; }
    T& back() { assert(Tail); return Tail->Value; }
    const T& back() const { assert(Tail); return Tail->Value; }

private:
    void AddHeadNode(TDoubleLinkedListNode* Node)
    {
        Node->Next = Head;
        if (Head) Head->Prev = Node; else Tail = Node;
        Head = Node;
        ++Count;
    }
    void AddTailNode(TDoubleLinkedListNode* Node)
    {
        Node->Prev = Tail;
        if (Tail) Tail->Next = Node; else Head = Node;
        Tail = Node;
        ++Count;
    }
    void MoveFrom(TDoubleLinkedList&& Other) noexcept
    {
        Head = Other.Head; Tail = Other.Tail; Count = Other.Count;
        Other.Head = Other.Tail = nullptr; Other.Count = 0;
    }
    TDoubleLinkedListNode* Head = nullptr;
    TDoubleLinkedListNode* Tail = nullptr;
    int32 Count = 0;
};

// -----------------------------------------------------------------------------
// TBitArray: std::vector<bool>-backed compatibility subset.
// -----------------------------------------------------------------------------
class TBitArray
{
private:
    using FStorage = std::vector<bool>;
public:
    TBitArray() = default;
    TBitArray(bool bInitialValue, int32 NumBits) { Init(bInitialValue, NumBits); }

    int32 Add(bool bValue)
    {
        const int32 Index = Num();
        Bits.push_back(bValue);
        return Index;
    }
    void Init(bool bValue, int32 NumBits)
    {
        assert(NumBits >= 0);
        Bits.assign(static_cast<std::size_t>(NumBits), bValue);
    }
    void SetNum(int32 NewNum, bool bValue = false)
    {
        assert(NewNum >= 0);
        Bits.resize(static_cast<std::size_t>(NewNum), bValue);
    }
    int32 Num() const { return static_cast<int32>(Bits.size()); }
    bool IsEmpty() const { return Bits.empty(); }
    bool Get(int32 Index) const { assert(Index >= 0 && Index < Num()); return Bits[static_cast<std::size_t>(Index)]; }
    void Set(int32 Index, bool bValue) { assert(Index >= 0 && Index < Num()); Bits[static_cast<std::size_t>(Index)] = bValue; }
    std::vector<bool>::reference operator[](int32 Index) { assert(Index >= 0 && Index < Num()); return Bits[static_cast<std::size_t>(Index)]; }
    bool operator[](int32 Index) const { return Get(Index); }
    void Reset() { Bits.clear(); }
    void Empty() { FStorage EmptyBits; Bits.swap(EmptyBits); }
    int32 CountSetBits() const { return static_cast<int32>(std::ranges::count(Bits, true)); }
    std::vector<bool>::iterator begin() { return Bits.begin(); }
    std::vector<bool>::iterator end() { return Bits.end(); }
    std::vector<bool>::const_iterator begin() const { return Bits.begin(); }
    std::vector<bool>::const_iterator end() const { return Bits.end(); }
    // STL compatibility.
    void push_back(bool Value) { Bits.push_back(Value); }
    std::size_t size() const { return Bits.size(); }
    bool empty() const { return Bits.empty(); }
    void clear() { Bits.clear(); }
    void resize(std::size_t NewSize) { Bits.resize(NewSize); }
private:
    FStorage Bits;
};

// -----------------------------------------------------------------------------
// TMap
// -----------------------------------------------------------------------------
template <typename Key, typename Value>
class TMap
{
private:
    using FStorage = std::unordered_map<Key, Value>;
public:
    using ElementType = FStorage::value_type;
    using Iterator = FStorage::iterator;
    using ConstIterator = FStorage::const_iterator;

    Value& Add(const Key& InKey, const Value& InValue) { return Elements.insert_or_assign(InKey, InValue).first->second; }
    Value& Add(Key&& InKey, Value&& InValue) { return Elements.insert_or_assign(std::move(InKey), std::move(InValue)).first->second; }
    Value& Add(const Key& InKey) { return Elements[InKey]; }

    Value* Find(const Key& InKey)
    {
        auto It = Elements.find(InKey);
        return It == Elements.end() ? nullptr : &It->second;
    }
    const Value* Find(const Key& InKey) const
    {
        auto It = Elements.find(InKey);
        return It == Elements.end() ? nullptr : &It->second;
    }
    Value FindRef(const Key& InKey) const
    {
        const auto It = Elements.find(InKey);
        return It == Elements.end() ? Value{} : It->second;
    }
    Value& FindOrAdd(const Key& InKey) { return Elements[InKey]; }
    bool Contains(const Key& InKey) const { return Elements.contains(InKey); }
    int32 Remove(const Key& InKey) { return static_cast<int32>(Elements.erase(InKey)); }
    int32 Num() const { return static_cast<int32>(Elements.size()); }
    bool IsEmpty() const { return Elements.empty(); }
    void Reserve(int32 Capacity) { assert(Capacity >= 0); Elements.reserve(static_cast<std::size_t>(Capacity)); }
    void Reset() { Elements.clear(); }
    void Empty(int32 Slack = 0)
    {
        assert(Slack >= 0);
        Elements.clear();
        Elements.rehash(0);
        if (Slack > 0) Elements.reserve(static_cast<std::size_t>(Slack));
    }
    void Append(const TMap& Other) { for (const auto& Pair : Other.Elements) Elements.insert_or_assign(Pair.first, Pair.second); }
    void GenerateKeyArray(TArray<Key>& OutKeys) const { OutKeys.Reset(Num()); for (const auto& Pair : Elements) OutKeys.Add(Pair.first); }
    void GenerateValueArray(TArray<Value>& OutValues) const { OutValues.Reset(Num()); for (const auto& Pair : Elements) OutValues.Add(Pair.second); }

    Value& operator[](const Key& InKey) { return Elements[InKey]; }
    Iterator begin() { return Elements.begin(); }
    Iterator end() { return Elements.end(); }
    ConstIterator begin() const { return Elements.begin(); }
    ConstIterator end() const { return Elements.end(); }

    // STL compatibility.
    std::size_t size() const { return Elements.size(); }
    bool empty() const { return Elements.empty(); }
    void clear() { Elements.clear(); }
    Iterator find(const Key& InKey) { return Elements.find(InKey); }
    ConstIterator find(const Key& InKey) const { return Elements.find(InKey); }
    std::pair<Iterator, bool> emplace(const Key& InKey, const Value& InValue) { return Elements.emplace(InKey, InValue); }
    std::size_t erase(const Key& InKey) { return Elements.erase(InKey); }
private:
    FStorage Elements;
};

// -----------------------------------------------------------------------------
// TMultimap (unlike TMap, duplicate keys are allowed).
// -----------------------------------------------------------------------------
template <typename Key, typename Value>
class TMultimap
{
private:
    using FStorage = std::unordered_multimap<Key, Value>;
public:
    using Iterator = FStorage::iterator;
    using ConstIterator = FStorage::const_iterator;

    Value& Add(const Key& InKey, const Value& InValue) { return Elements.emplace(InKey, InValue)->second; }
    Value& Add(Key&& InKey, Value&& InValue) { return Elements.emplace(std::move(InKey), std::move(InValue))->second; }
    bool Contains(const Key& InKey) const { return Elements.contains(InKey); }
    int32 Remove(const Key& InKey) { return static_cast<int32>(Elements.erase(InKey)); }
    int32 Num() const { return static_cast<int32>(Elements.size()); }
    bool IsEmpty() const { return Elements.empty(); }
    void Reserve(int32 Capacity) { assert(Capacity >= 0); Elements.reserve(static_cast<std::size_t>(Capacity)); }
    void Reset() { Elements.clear(); }
    void Empty() { Elements.clear(); Elements.rehash(0); }

    Value* Find(const Key& InKey)
    {
        auto It = Elements.find(InKey);
        return It == Elements.end() ? nullptr : &It->second;
    }
    const Value* Find(const Key& InKey) const
    {
        auto It = Elements.find(InKey);
        return It == Elements.end() ? nullptr : &It->second;
    }
    const Value* FindPair(const Key& InKey, const Value& InValue) const
    {
        const auto Range = Elements.equal_range(InKey);
        for (auto It = Range.first; It != Range.second; ++It)
            if (It->second == InValue) return &It->second;
        return nullptr;
    }
    void MultiFind(const Key& InKey, TArray<Value>& OutValues) const
    {
        OutValues.Reset();
        const auto Range = Elements.equal_range(InKey);
        for (auto It = Range.first; It != Range.second; ++It) OutValues.Add(It->second);
    }

    Iterator begin() { return Elements.begin(); }
    Iterator end() { return Elements.end(); }
    ConstIterator begin() const { return Elements.begin(); }
    ConstIterator end() const { return Elements.end(); }
    std::size_t size() const { return Elements.size(); }
    bool empty() const { return Elements.empty(); }
    void clear() { Elements.clear(); }
    Iterator find(const Key& InKey) { return Elements.find(InKey); }
    ConstIterator find(const Key& InKey) const { return Elements.find(InKey); }
    auto equal_range(const Key& InKey) { return Elements.equal_range(InKey); }
private:
    FStorage Elements;
};

// -----------------------------------------------------------------------------
// TSet
// -----------------------------------------------------------------------------
template <typename T>
class TSet
{
private:
    using FStorage = std::unordered_set<T>;
public:
    using Iterator = FStorage::iterator;
    using ConstIterator = FStorage::const_iterator;

    // Returns true when a new item was inserted.
    bool Add(const T& Value) { return Elements.insert(Value).second; }
    bool Add(T&& Value) { return Elements.insert(std::move(Value)).second; }
    bool Contains(const T& Value) const { return Elements.contains(Value); }
    int32 Remove(const T& Value) { return static_cast<int32>(Elements.erase(Value)); }
    int32 Num() const { return static_cast<int32>(Elements.size()); }
    bool IsEmpty() const { return Elements.empty(); }
    void Reserve(int32 Capacity) { assert(Capacity >= 0); Elements.reserve(static_cast<std::size_t>(Capacity)); }
    void Reset() { Elements.clear(); }
    void Empty() { Elements.clear(); Elements.rehash(0); }
    TArray<T> Array() const { TArray<T> Result; Result.Reserve(Num()); for (const T& Value : Elements) Result.Add(Value); return Result; }

    Iterator begin() { return Elements.begin(); }
    Iterator end() { return Elements.end(); }
    ConstIterator begin() const { return Elements.begin(); }
    ConstIterator end() const { return Elements.end(); }
    std::size_t size() const { return Elements.size(); }
    bool empty() const { return Elements.empty(); }
    void clear() { Elements.clear(); }
    std::pair<Iterator, bool> insert(const T& Value) { return Elements.insert(Value); }
    Iterator find(const T& Value) { return Elements.find(Value); }
    ConstIterator find(const T& Value) const { return Elements.find(Value); }
    std::size_t erase(const T& Value) { return Elements.erase(Value); }
private:
    FStorage Elements;
};

// -----------------------------------------------------------------------------
// TQueue - std::queue-backed; this is NOT thread-safe.
// -----------------------------------------------------------------------------
template <typename T>
class TQueue
{
private:
    using FStorage = std::queue<T>;
public:
    bool Enqueue(const T& Item) { Elements.push(Item); return true; }
    bool Enqueue(T&& Item) { Elements.push(std::move(Item)); return true; }
    bool Dequeue(T& OutItem)
    {
        if (Elements.empty()) return false;
        OutItem = std::move(Elements.front());
        Elements.pop();
        return true;
    }
    bool Peek(T& OutItem) const
    {
        if (Elements.empty()) return false;
        OutItem = Elements.front();
        return true;
    }
    T* Peek() { return Elements.empty() ? nullptr : &Elements.front(); }
    const T* Peek() const { return Elements.empty() ? nullptr : &Elements.front(); }
    bool Pop()
    {
        if (Elements.empty()) return false;
        Elements.pop();
        return true;
    }
    bool IsEmpty() const { return Elements.empty(); }
    void Empty() { FStorage EmptyQueue; Elements.swap(EmptyQueue); }

    // STL compatibility.
    void push(const T& Item) { Elements.push(Item); }
    void push(T&& Item) { Elements.push(std::move(Item)); }
    void pop() { Elements.pop(); }
    T& front() { return Elements.front(); }
    const T& front() const { return Elements.front(); }
    std::size_t size() const { return Elements.size(); }
    bool empty() const { return Elements.empty(); }
private:
    FStorage Elements;
};

// -----------------------------------------------------------------------------
// Smart pointer wrappers. TSharedPtr/TWeakPtr/TUniquePtr are NOT for Unreal UObjects.
// -----------------------------------------------------------------------------
template <typename T> class TWeakPtr;

template <typename T>
class TSharedPtr
{
    template <typename U> friend class TWeakPtr;
public:
    TSharedPtr() = default;
    TSharedPtr(std::nullptr_t) {}
    TSharedPtr(std::shared_ptr<T> InPtr) : Ptr(std::move(InPtr)) {}

    template <typename U, typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
    TSharedPtr(const TSharedPtr<U>& Other) : Ptr(Other.Ptr) {}

    T* Get() const { return Ptr.get(); }
    bool IsValid() const { return static_cast<bool>(Ptr); }
    void Reset() { Ptr.reset(); }
    long GetSharedReferenceCount() const { return Ptr.use_count(); }
    std::shared_ptr<T> ToStd() const { return Ptr; }
    explicit operator bool() const { return IsValid(); }
    T* operator->() const { return Ptr.get(); }
    T& operator*() const { return *Ptr; }

    // std::shared_ptr-style compatibility.
    T* get() const { return Ptr.get(); }
    void reset() { Ptr.reset(); }
    long use_count() const { return Ptr.use_count(); }
    operator std::shared_ptr<T>() const { return Ptr; }
private:
    std::shared_ptr<T> Ptr;
};

template <typename T>
class TWeakPtr
{
public:
    TWeakPtr() = default;
    TWeakPtr(const TSharedPtr<T>& InPtr) : Ptr(InPtr.Ptr) {}
    TWeakPtr(std::weak_ptr<T> InPtr) : Ptr(std::move(InPtr)) {}
    TSharedPtr<T> Pin() const { return TSharedPtr<T>(Ptr.lock()); }
    bool IsValid() const { return !Ptr.expired(); }
    void Reset() { Ptr.reset(); }
    std::weak_ptr<T> ToStd() const { return Ptr; }

    // std::weak_ptr-style compatibility.
    std::shared_ptr<T> lock() const { return Ptr.lock(); }
    bool expired() const { return Ptr.expired(); }
    void reset() { Ptr.reset(); }
private:
    std::weak_ptr<T> Ptr;
};

template <typename T>
class TUniquePtr
{
public:
    TUniquePtr() = default;
    TUniquePtr(std::nullptr_t) {}
    explicit TUniquePtr(T* RawPtr) : Ptr(RawPtr) {}
    TUniquePtr(std::unique_ptr<T> InPtr) : Ptr(std::move(InPtr)) {}
    TUniquePtr(const TUniquePtr&) = delete;
    TUniquePtr& operator=(const TUniquePtr&) = delete;
    TUniquePtr(TUniquePtr&&) noexcept = default;
    TUniquePtr& operator=(TUniquePtr&&) noexcept = default;

    T* Get() const { return Ptr.get(); }
    bool IsValid() const { return Ptr != nullptr; }
    void Reset(T* RawPtr = nullptr) { Ptr.reset(RawPtr); }
    T* Release() { return Ptr.release(); }
    explicit operator bool() const { return IsValid(); }
    T* operator->() const { return Ptr.get(); }
    T& operator*() const { return *Ptr; }
    std::unique_ptr<T> ToStd() && { return std::move(Ptr); }

    // std::unique_ptr-style compatibility.
    T* get() const { return Ptr.get(); }
    void reset(T* RawPtr = nullptr) { Ptr.reset(RawPtr); }
    T* release() { return Ptr.release(); }
private:
    std::unique_ptr<T> Ptr;
};

template <typename T, typename... TArgs>
TSharedPtr<T> MakeShared(TArgs&&... Args)
{
    return TSharedPtr<T>(std::make_shared<T>(std::forward<TArgs>(Args)...));
}

template <typename T, typename... TArgs>
TUniquePtr<T> MakeUnique(TArgs&&... Args)
{
    return TUniquePtr<T>(std::make_unique<T>(std::forward<TArgs>(Args)...));
}

// -----------------------------------------------------------------------------
// FString: std::string-backed UTF-8-ish string facade. Not equivalent to UE FString.
// -----------------------------------------------------------------------------
class FString
{
public:
    using SizeType = std::string::size_type;
    static constexpr int32 INDEX_NONE = -1;

    FString() = default;
    FString(const char* InText) : Data(InText ? InText : "") {}
    FString(const std::string& InText) : Data(InText) {}
    FString(std::string&& InText) : Data(std::move(InText)) {}
    FString(std::string_view InText) : Data(InText) {}

    int32 Len() const { return static_cast<int32>(Data.size()); }
    bool IsEmpty() const { return Data.empty(); }
    bool Contains(std::string_view Substring) const { return Data.find(Substring) != std::string::npos; }
    bool StartsWith(std::string_view Prefix) const { return Data.size() >= Prefix.size() && Data.compare(0, Prefix.size(), Prefix) == 0; }
    bool EndsWith(std::string_view Suffix) const { return Data.size() >= Suffix.size() && Data.compare(Data.size() - Suffix.size(), Suffix.size(), Suffix) == 0; }

    FString& Append(std::string_view Text) { Data.append(Text); return *this; }
    FString Left(int32 Count) const
    {
        const std::size_t SafeCount = Count <= 0 ? 0 : static_cast<std::size_t>(Count);
        return FString(Data.substr(0, SafeCount));
    }
    FString Right(int32 Count) const
    {
        const std::size_t SafeCount = Count <= 0 ? 0 : static_cast<std::size_t>(Count);
        return FString(Data.substr(SafeCount >= Data.size() ? 0 : Data.size() - SafeCount));
    }
    FString Mid(int32 Start, int32 Count = INDEX_NONE) const
    {
        if (Start < 0 || static_cast<std::size_t>(Start) > Data.size()) return FString();
        const std::size_t Length = Count < 0 ? std::string::npos : static_cast<std::size_t>(Count);
        return FString(Data.substr(static_cast<std::size_t>(Start), Length));
    }
    int32 Find(std::string_view Substring, bool bSearchFromEnd = false) const
    {
        const std::size_t Position = bSearchFromEnd ? Data.rfind(Substring) : Data.find(Substring);
        return Position == std::string::npos ? INDEX_NONE : static_cast<int32>(Position);
    }
    FString Replace(std::string_view From, std::string_view To) const
    {
        if (From.empty()) return *this;
        std::string Result = Data;
        std::size_t Position = 0;
        while ((Position = Result.find(From, Position)) != std::string::npos)
        {
            Result.replace(Position, From.size(), To);
            Position += To.size();
        }
        return FString(std::move(Result));
    }
    FString ToLower() const
    {
        std::string Result = Data;
        std::ranges::transform(Result, Result.begin(), [](unsigned char Ch) { return static_cast<char>(std::tolower(Ch)); });
        return FString(std::move(Result));
    }
    FString ToUpper() const
    {
        std::string Result = Data;
        std::ranges::transform(Result, Result.begin(), [](unsigned char Ch) { return static_cast<char>(std::toupper(Ch)); });
        return FString(std::move(Result));
    }
    void Empty() { Data.clear(); }

    const char* c_str() const { return Data.c_str(); }
    char* data() { return Data.data(); }
    const char* data() const { return Data.data(); }
    char& operator[](std::size_t Index) { return Data[Index]; }
    const char& operator[](std::size_t Index) const { return Data[Index]; }
    FString& operator+=(std::string_view Text) { Data.append(Text); return *this; }
    friend FString operator+(const FString& Left, const FString& Right) { return FString(Left.Data + Right.Data); }
    friend bool operator==(const FString& Left, const FString& Right) { return Left.Data == Right.Data; }
    friend bool operator!=(const FString& Left, const FString& Right) { return !(Left == Right); }
    friend bool operator<(const FString& Left, const FString& Right) { return Left.Data < Right.Data; }

    // std::string compatibility. Explicit access remains available through ToStd().
    SizeType size() const { return Data.size(); }
    bool empty() const { return Data.empty(); }
    std::size_t find(std::string_view Text) const { return Data.find(Text); }
    FString substr(std::size_t Start = 0, std::size_t Count = std::string::npos) const { return FString(Data.substr(Start, Count)); }
    void clear() { Data.clear(); }
    void append(std::string_view Text) { Data.append(Text); }
    std::string& ToStd() { return Data; }
    const std::string& ToStd() const { return Data; }
    operator const std::string&() const { return Data; }
    operator std::string&() { return Data; }
private:
    std::string Data;
};
