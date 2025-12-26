// This is our List template used with transpiled C# --> C++ code.
// It matches the API and behavior of System.Collections.Generic.List
// as well as possible.  Memory management is done via std::shared_ptr and std::vector.

#pragma once
#include <memory>
#include <vector>
#include <initializer_list>

// This module is part of Layer 3B (Host C# Compatibility Layer)
#define CORE_LAYER_3B

// Forward declaration
template<typename T> class List;

// List - wrapper around std::vector with C# List API
template<typename T>
class List {
private:
    std::shared_ptr<std::vector<T>> data;

    // Ensure data is allocated
    void ensureData() {
        if (!data) {
            data = std::make_shared<std::vector<T>>();
        }
    }

public:
    // Constructors
    List() : data(std::make_shared<std::vector<T>>()) {}

    // poolNum parameter for compatibility - ignored
    List(uint8_t /*poolNum*/) : data(std::make_shared<std::vector<T>>()) {}

    // Copy constructor - shares the underlying vector
    List(const List<T>& other) = default;

    // Assignment operator - shares the underlying vector
    List<T>& operator=(const List<T>& other) = default;

    // Destructor
    ~List() = default;

    // Constructor that adopts an array
    List(T* adoptedArray, int arrayCount, uint8_t /*poolNum*/ = 0) {
        data = std::make_shared<std::vector<T>>(adoptedArray, adoptedArray + arrayCount);
        free(adoptedArray);
    }

    // Initializer list constructor
    List(std::initializer_list<T> items, uint8_t /*poolNum*/ = 0)
        : data(std::make_shared<std::vector<T>>(items)) {}


    // Properties
    int Count() const {
        return data ? static_cast<int>(data->size()) : 0;
    }

    int Capacity() const {
        return data ? static_cast<int>(data->capacity()) : 0;
    }

    bool Empty() const { return Count() == 0; }

    // Element access
    T& operator[](int index) {
        ensureData();
        return (*data)[index];
    }

    const T& operator[](int index) const {
        return (*data)[index];
    }

    T& At(int index) {
        if (!data || index < 0 || index >= static_cast<int>(data->size())) {
            static T defaultValue = T();
            return defaultValue;
        }
        return (*data)[index];
    }

    const T& At(int index) const {
        if (!data || index < 0 || index >= static_cast<int>(data->size())) {
            static T defaultValue = T();
            return defaultValue;
        }
        return (*data)[index];
    }


    // Adding elements
    void Add(const T& item) {
        ensureData();
        data->push_back(item);
    }

    // Move version for types that can't be copied (like unique_ptr)
    void Add(T&& item) {
        ensureData();
        data->push_back(std::move(item));
    }

    void AddRange(const List<T>& collection) {
        if (collection.Empty()) return;
        ensureData();
        if (collection.data) {
            data->insert(data->end(), collection.data->begin(), collection.data->end());
        }
    }

    // Clear
    void Clear() {
        if (data) data->clear();
    }

    // Reverse
    void Reverse() {
        if (!data || data->empty()) return;
        size_t left = 0;
        size_t right = data->size() - 1;
        while (left < right) {
            T temp = (*data)[left];
            (*data)[left] = (*data)[right];
            (*data)[right] = temp;
            left++;
            right--;
        }
    }


    // Contains - check if item exists in list
    bool Contains(const T& item) const {
        if (!data) return false;
        for (size_t i = 0; i < data->size(); i++) {
            if ((*data)[i] == item) return true;
        }
        return false;
    }

    // IndexOf - find first occurrence of item
    int IndexOf(const T& item) const {
        if (!data) return -1;
        for (size_t i = 0; i < data->size(); i++) {
            if ((*data)[i] == item) return static_cast<int>(i);
        }
        return -1;
    }

    // LastIndexOf - find last occurrence of item
    int LastIndexOf(const T& item) const {
        if (!data) return -1;
        for (int i = static_cast<int>(data->size()) - 1; i >= 0; i--) {
            if ((*data)[i] == item) return i;
        }
        return -1;
    }


    // Insert - insert item at specific index
    void Insert(int index, const T& item) {
        ensureData();
        if (index < 0 || index > static_cast<int>(data->size())) return;
        data->insert(data->begin() + index, item);
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
        if (!data || index < 0 || index >= static_cast<int>(data->size())) return;
        data->erase(data->begin() + index);
    }

    // RemoveRange - remove range of items
    void RemoveRange(int index, int count) {
        if (!data || index < 0 || count <= 0 || index >= static_cast<int>(data->size())) return;

        int actualCount = count;
        if (index + actualCount > static_cast<int>(data->size())) {
            actualCount = static_cast<int>(data->size()) - index;
        }

        data->erase(data->begin() + index, data->begin() + index + actualCount);
    }

    // Sort - simple bubble sort
    void Sort() {
        if (!data || data->size() <= 1) return;

        size_t n = data->size();
        for (size_t i = 0; i < n - 1; i++) {
            for (size_t j = 0; j < n - 1 - i; j++) {
                if ((*data)[j] > (*data)[j + 1]) {
                    T temp = (*data)[j];
                    (*data)[j] = (*data)[j + 1];
                    (*data)[j + 1] = temp;
                }
            }
        }
    }


    // ToArrayCopy - return a copy of the data as a regular array
    // (NOTE: This is the same as C#'s ToArray, but since in C land, this is
    // an *unmanaged* array, it's unlikely that a direct translation from
    // that to this will ever be correct code.  Thus the rename.)
    T* ToArrayCopy() const {
        if (!data || data->empty()) return nullptr;

        T* result = static_cast<T*>(malloc(data->size() * sizeof(T)));
        if (!result) return nullptr;

        for (size_t i = 0; i < data->size(); i++) {
            result[i] = (*data)[i];
        }
        return result;
    }

    // AsArray: more often what you actually want, this just exposes the
    // underlying array (same as begin()).
    T* AsArray() {
        return data ? data->data() : nullptr;
    }

    // Operators
    explicit operator bool() const {
        return !Empty();
    }

    // Iterator support
    T* begin() {
        return data ? data->data() : nullptr;
    }

    const T* begin() const {
        return data ? data->data() : nullptr;
    }

    T* end() {
        return data ? data->data() + data->size() : nullptr;
    }

    const T* end() const {
        return data ? data->data() + data->size() : nullptr;
    }

    // Compatibility methods (poolNum ignored with shared_ptr)
    uint8_t getPoolNum() const { return 0; }

    bool isValid() const { return data != nullptr; }
};

