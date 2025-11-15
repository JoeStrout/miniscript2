// This is our List template used with transpiled C# --> C++ code.
// It matches the API and behavior of System.Collections.Generic.List
// as well as possible.  Memory management is done via MemPool.

#pragma once
#include "MemPool.h"
#include <type_traits>
#include <new>
#include <cstring>
#include <initializer_list>

// This module is part of Layer 3B (Host C# Compatibility Layer)
#define CORE_LAYER_3B

// Forward declaration
template<typename T> class List;

// ListStorage - the actual storage for list data
template<typename T>
struct ListStorage {
    int count;
    int capacity;
    // Data follows immediately after this struct
    // T data[capacity] would be here, but we can't use flexible array member in C++
    
    // Helper to get the data array
    T* getData() {
        return reinterpret_cast<T*>(reinterpret_cast<char*>(this) + sizeof(ListStorage<T>));
    }
    
    const T* getData() const {
        return reinterpret_cast<const T*>(reinterpret_cast<const char*>(this) + sizeof(ListStorage<T>));
    }
    
    // Calculate total size needed for ListStorage + capacity T items
    static size_t calculateSize(int capacity) {
        return sizeof(ListStorage<T>) + capacity * sizeof(T);
    }
    
    // Initialize a new ListStorage
    static void initialize(ListStorage<T>* storage, int initialCapacity) {
        storage->count = 0;
        storage->capacity = initialCapacity;
        // Don't need to initialize T items yet - they'll be constructed as needed
    }
};

// List - trivially copyable list using MemPool
template<typename T>
class List {
private:
    MemRef storage;  // Reference to ListStorage<T>
    
    ListStorage<T>* getStorage() const {
        return static_cast<ListStorage<T>*>(MemPoolManager::getPtr(storage));
    }
    
    void ensureCapacity(int minCapacity) {
        ListStorage<T>* s = getStorage();
        if (!s) {
            // No storage yet - create it
            createStorage(minCapacity > 4 ? minCapacity : 4);
            return;
        }
        
        if (minCapacity <= s->capacity) return;
        
        int newCapacity = s->capacity > 0 ? s->capacity * 2 : 4;
        if (newCapacity < minCapacity) newCapacity = minCapacity;
        
        // Allocate new storage
        MemRef newRef = MemPoolManager::realloc(storage, ListStorage<T>::calculateSize(newCapacity));
        if (newRef.isNull()) return; // Allocation failed
        
        storage = newRef;
        ListStorage<T>* newStorage = getStorage();
        newStorage->capacity = newCapacity;
        
        // Note: realloc preserves existing data, so count and existing T items are fine
    }
    
    void createStorage(int initialCapacity) {
        storage = MemPoolManager::alloc(ListStorage<T>::calculateSize(initialCapacity), storage.poolNum);
        if (!storage.isNull()) {
            ListStorage<T>::initialize(getStorage(), initialCapacity);
        }
    }
    
    // Helper methods for efficient copying (same as before)
    void copyElements(T* dest, const T* src, int elementCount) {
        copyElementsImpl(dest, src, elementCount, typename std::is_trivially_copyable<T>::type{});
    }
    
    void copyElementsImpl(T* dest, const T* src, int elementCount, std::true_type) {
        // Type is trivially copyable - use fast memcpy
        memcpy(dest, src, elementCount * sizeof(T));
    }
    
    void copyElementsImpl(T* dest, const T* src, int elementCount, std::false_type) {
        // Type needs proper copy construction
        for (int i = 0; i < elementCount; i++) {
            new(&dest[i]) T(src[i]);
        }
    }
    
public:
    // Constructors - all trivial!
    List(uint8_t poolNum = 0) : storage(poolNum, 0) {}
    
    // Copy constructor and assignment (trivially copyable!)
    List(const List<T>& other) = default;
    List<T>& operator=(const List<T>& other) = default;
    
    // Destructor - trivial! Memory is managed by pool lifecycle
    ~List() = default;
    
    // Constructor that adopts an array (like before)
    List(T* adoptedArray, int arrayCount, uint8_t poolNum = 0) : storage(poolNum, 0) {
        storage = MemPoolManager::alloc(ListStorage<T>::calculateSize(arrayCount), poolNum);
        if (!storage.isNull()) {
            ListStorage<T>* s = getStorage();
            ListStorage<T>::initialize(s, arrayCount);
            s->count = arrayCount;
            
            // Copy the adopted data
            copyElements(s->getData(), adoptedArray, arrayCount);
            
            // Free the original array
            free(adoptedArray);
        }
    }
    
    // Initializer list constructor
    List(std::initializer_list<T> items, uint8_t poolNum = 0) : storage(poolNum, 0) {
        if (items.size() > 0) {
            ensureCapacity(static_cast<int>(items.size()));
            ListStorage<T>* s = getStorage();
            if (s) {
                int i = 0;
                for (const auto& item : items) {
                    new(&s->getData()[i]) T(item);
                    i++;
                }
                s->count = static_cast<int>(items.size());
            }
        }
    }
    
    // Properties
    int Count() const { 
        ListStorage<T>* s = getStorage();
        return s ? s->count : 0;
    }
    
    int Capacity() const { 
        ListStorage<T>* s = getStorage();
        return s ? s->capacity : 0;
    }
    
    bool Empty() const { return Count() == 0; }
    
    // Element access
    T& operator[](int index) {
        ListStorage<T>* s = getStorage();
        return s->getData()[index];
    }
    
    const T& operator[](int index) const {
        ListStorage<T>* s = getStorage();
        return s->getData()[index];
    }
    
    T& At(int index) {
        ListStorage<T>* s = getStorage();
        if (!s || index < 0 || index >= s->count) {
            static T defaultValue = T();
            return defaultValue;
        }
        return s->getData()[index];
    }
    
    const T& At(int index) const {
        ListStorage<T>* s = getStorage();
        if (!s || index < 0 || index >= s->count) {
            static T defaultValue = T();
            return defaultValue;
        }
        return s->getData()[index];
    }
    
    // Adding elements
    void Add(const T& item) {
        ensureCapacity(Count() + 1);
        ListStorage<T>* s = getStorage();
        if (s) {
            new(&s->getData()[s->count]) T(item);
            s->count++;
        }
    }
    
    void AddRange(const List<T>& collection) {
        if (collection.Empty()) return;
        
        ensureCapacity(Count() + collection.Count());
        ListStorage<T>* s = getStorage();
        ListStorage<T>* other = collection.getStorage();
        
        if (s && other) {
            copyElements(s->getData() + s->count, other->getData(), other->count);
            s->count += other->count;
        }
    }
    
    // Clear - doesn't free memory, just resets count
    void Clear() {
        ListStorage<T>* s = getStorage();
        if (s) {
            // Destroy existing elements
            for (int i = 0; i < s->count; i++) {
                s->getData()[i].~T();
            }
            s->count = 0;
        }
    }
    
    // Reverse - reverse the order of elements
    void Reverse() {
        ListStorage<T>* s = getStorage();
        if (!s || s->count <= 1) return;
        
        T* data = s->getData();
        int left = 0;
        int right = s->count - 1;
        
        while (left < right) {
            // Swap elements
            T temp = data[left];
            data[left] = data[right];
            data[right] = temp;
            left++;
            right--;
        }
    }
    
    // Contains - check if item exists in list
    bool Contains(const T& item) const {
        ListStorage<T>* s = getStorage();
        if (!s) return false;
        
        T* data = s->getData();
        for (int i = 0; i < s->count; i++) {
            if (data[i] == item) return true;
        }
        return false;
    }
    
    // IndexOf - find first occurrence of item
    int IndexOf(const T& item) const {
        ListStorage<T>* s = getStorage();
        if (!s) return -1;
        
        T* data = s->getData();
        for (int i = 0; i < s->count; i++) {
            if (data[i] == item) return i;
        }
        return -1;
    }
    
    // LastIndexOf - find last occurrence of item
    int LastIndexOf(const T& item) const {
        ListStorage<T>* s = getStorage();
        if (!s) return -1;
        
        T* data = s->getData();
        for (int i = s->count - 1; i >= 0; i--) {
            if (data[i] == item) return i;
        }
        return -1;
    }
    
    // Insert - insert item at specific index
    void Insert(int index, const T& item) {
        ListStorage<T>* s = getStorage();
        int currentCount = s ? s->count : 0;
        
        if (index < 0 || index > currentCount) return;
        
        ensureCapacity(currentCount + 1);
        s = getStorage(); // refresh after potential reallocation
        if (!s) return;
        
        T* data = s->getData();
        // Move elements to make room
        for (int i = s->count; i > index; i--) {
            new(&data[i]) T(data[i-1]);
            data[i-1].~T();
        }
        
        new(&data[index]) T(item);
        s->count++;
    }
    
    // Remove - remove first occurrence of item
    bool Remove(const T& item) {
        int index = IndexOf(item);
        if (index == -1) return false;
        RemoveAt(index);
        return true;
    }
    
    // RemoveAt - remove item at specific index
    void RemoveAt(int index) {
        ListStorage<T>* s = getStorage();
        if (!s || index < 0 || index >= s->count) return;
        
        T* data = s->getData();
        data[index].~T();
        
        // Shift remaining elements down
        for (int i = index; i < s->count - 1; i++) {
            new(&data[i]) T(data[i+1]);
            data[i+1].~T();
        }
        
        s->count--;
    }
    
    // RemoveRange - remove range of items
    void RemoveRange(int index, int count) {
        ListStorage<T>* s = getStorage();
        if (!s || index < 0 || count <= 0 || index >= s->count) return;
        
        int actualCount = count;
        if (index + actualCount > s->count) {
            actualCount = s->count - index;
        }
        
        T* data = s->getData();
        
        // Destroy elements being removed
        for (int i = index; i < index + actualCount; i++) {
            data[i].~T();
        }
        
        // Shift remaining elements down
        for (int i = index; i < s->count - actualCount; i++) {
            new(&data[i]) T(data[i + actualCount]);
            data[i + actualCount].~T();
        }
        
        s->count -= actualCount;
    }
    
    // Sort - simple bubble sort for now (could be improved)
    void Sort() {
        ListStorage<T>* s = getStorage();
        if (!s || s->count <= 1) return;
        
        T* data = s->getData();
        for (int i = 0; i < s->count - 1; i++) {
            for (int j = 0; j < s->count - 1 - i; j++) {
                if (data[j] > data[j + 1]) {
                    T temp = data[j];
                    data[j] = data[j + 1];
                    data[j + 1] = temp;
                }
            }
        }
    }
    
    // ToArrayCopy - return a copy of the data as a regular array
    // (NOTE: This is the same as C#'s ToArray, but since in C land, this is
    // an *unmanaged* array, it's unlikely that a direct translation from
    // that to this will ever be correct code.  Thus the rename.)
    T* ToArrayCopy() const {
        ListStorage<T>* s = getStorage();
        if (!s || s->count == 0) return nullptr;
        
        T* result = static_cast<T*>(malloc(s->count * sizeof(T)));
        if (!result) return nullptr;
        
        copyElements(result, s->getData(), s->count);
        return result;
    }
    
    // AsArray: more often what you actually want, this just exposes the
    // underlying array (same as begin()).
    T* AsArray() {
        ListStorage<T>* s = getStorage();
        return s ? s->getData() : nullptr;
    }
    
    // Operators
	explicit operator bool() const {
		return !Empty();
	}
    
    // Iterator support
    T* begin() { 
        ListStorage<T>* s = getStorage();
        return s ? s->getData() : nullptr;
    }
    
    const T* begin() const { 
        ListStorage<T>* s = getStorage();
        return s ? s->getData() : nullptr;
    }
    
    T* end() { 
        ListStorage<T>* s = getStorage();
        return s ? s->getData() + s->count : nullptr;
    }
    
    const T* end() const { 
        ListStorage<T>* s = getStorage();
        return s ? s->getData() + s->count : nullptr;
    }
    
    // Pool management
    uint8_t getPoolNum() const { return storage.poolNum; }
    MemRef getMemRef() const { return storage; }

    bool isValid() const { return !storage.isNull() && getStorage() != nullptr; }
};

