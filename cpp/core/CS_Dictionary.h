// This is our Dictionary template used with transpiled C# --> C++ code.
// It matches the API and behavior of System.Collections.Generic.Dictionary
// as well as possible.  Memory management is done via std::shared_ptr and std::vector.
//
// Dictionary is a lightweight handle (shared_ptr to DictionaryStorage), so
// copies share the same underlying data — matching C# reference semantics.

#pragma once
#include <memory>
#include <vector>
#include <initializer_list>
#include <utility>  // for std::pair

// Forward declaration
class String;

// This module is part of Layer 2B (Host C# Compatibility Layer)
#define CORE_LAYER_2B

// Forward declaration
template<typename TKey, typename TValue> class Dictionary;

// Entry for hash table
template<typename TKey, typename TValue>
struct DictEntry {
	TKey key;
	TValue value;
	int next;  // Index of next entry in chain, or -1
	int hashCode;

	DictEntry() : next(-1), hashCode(0) {}
};

// Hash functions for different key types
inline int Hash(int value) {
	return value & 0x7FFFFFFF;  // Ensure non-negative
}

// Hash for enum types (like TokenType)
template<typename T, typename std::enable_if<std::is_enum<T>::value, int>::type = 0>
inline int Hash(T value) {
	return Hash(static_cast<int>(value));
}

// Hash(const String&) is defined in CS_String.h to avoid circular dependencies

// Shared storage for Dictionary — holds all mutable state so that
// copied Dictionary handles see the same data (C# reference semantics).
template<typename TKey, typename TValue>
class DictionaryStorage {
	friend class Dictionary<TKey, TValue>;
  public:
	DictionaryStorage() : count(0) {}
  private:
	std::vector<int> buckets;
	std::vector<DictEntry<TKey, TValue>> entries;
	int count;

	void createStorage(int initialCapacity) {
		if (initialCapacity < 4) initialCapacity = 4;
		buckets.assign(initialCapacity, -1);
		entries.resize(initialCapacity);
		count = 0;
	}

	void resize() {
		int oldCapacity = static_cast<int>(buckets.size());
		int newCapacity = oldCapacity * 2;
		if (newCapacity < 4) newCapacity = 4;

		std::vector<int> newBuckets(newCapacity, -1);
		std::vector<DictEntry<TKey, TValue>> newEntries(newCapacity);

		// Rehash all active entries by following bucket chains
		int newIndex = 0;
		for (int bucket = 0; bucket < oldCapacity; bucket++) {
			for (int i = buckets[bucket]; i >= 0; i = entries[i].next) {
				int newBucket = entries[i].hashCode % newCapacity;
				newEntries[newIndex].key = entries[i].key;
				newEntries[newIndex].value = entries[i].value;
				newEntries[newIndex].hashCode = entries[i].hashCode;
				newEntries[newIndex].next = newBuckets[newBucket];
				newBuckets[newBucket] = newIndex;
				newIndex++;
			}
		}

		buckets = std::move(newBuckets);
		entries = std::move(newEntries);
		count = newIndex;
	}

	int findEntry(const TKey& key) const {
		if (buckets.empty()) return -1;

		int hashCode = Hash(key);
		int capacity = static_cast<int>(buckets.size());
		int bucket = hashCode % capacity;

		for (int i = buckets[bucket]; i >= 0; i = entries[i].next) {
			if (entries[i].hashCode == hashCode && entries[i].key == key) {
				return i;
			}
		}
		return -1;
	}
};

// Dictionary - lightweight handle wrapping a shared DictionaryStorage
template<typename TKey, typename TValue>
class Dictionary {
private:
	std::shared_ptr<DictionaryStorage<TKey, TValue>> data;

	void ensureData() {
		if (!data) {
			data = std::make_shared<DictionaryStorage<TKey, TValue>>();
			data->createStorage(4);
		} else if (data->buckets.empty()) {
			data->createStorage(4);
		}
	}

public:
	// Constructor
	Dictionary() {}

    // Factory method - allocates (matches C# "new Dictionary<K,V>()")
    static Dictionary<TKey, TValue> New() {
        Dictionary<TKey, TValue> result;
        result.data = std::make_shared<DictionaryStorage<TKey, TValue>>();
        result.data->createStorage(4);
        return result;
    }

	// Copy constructor and assignment — shares the same storage
	Dictionary(const Dictionary<TKey, TValue>& other) = default;
	Dictionary<TKey, TValue>& operator=(const Dictionary<TKey, TValue>& other) = default;

	// nullptr constructor - creates null/unallocated dictionary
	Dictionary(std::nullptr_t) {}

	// nullptr assignment - resets to null/unallocated state
	Dictionary<TKey, TValue>& operator=(std::nullptr_t) {
		data = nullptr;
		return *this;
	}

	// Destructor
	~Dictionary() = default;

    // Check if dictionary is null (unallocated)
    friend bool IsNull(const Dictionary<TKey, TValue>& dict) {
        return dict.data == nullptr;
    }

	// Properties
	int Count() const {
		return data ? data->count : 0;
	}

	bool Empty() const { return Count() == 0; }

	// Add or update
	void Add(const TKey& key, const TValue& value) {
		ensureData();

		int hashCode = Hash(key);
		int capacity = static_cast<int>(data->buckets.size());
		int bucket = hashCode % capacity;

		// Check if key already exists
		for (int i = data->buckets[bucket]; i >= 0; i = data->entries[i].next) {
			if (data->entries[i].hashCode == hashCode && data->entries[i].key == key) {
				// Update existing
				data->entries[i].value = value;
				return;
			}
		}

		// Add new entry - resize if needed (use threshold = 3/4 of capacity)
		if (data->count * 4 >= capacity * 3) {
			data->resize();
			capacity = static_cast<int>(data->buckets.size());
			bucket = hashCode % capacity;
		}

		int index = data->count;
		data->entries[index].key = key;
		data->entries[index].value = value;
		data->entries[index].hashCode = hashCode;
		data->entries[index].next = data->buckets[bucket];
		data->buckets[bucket] = index;
		data->count++;
	}

	// Indexer - get value by key (returns default if not found)
	TValue& operator[](const TKey& key) {
		ensureData();
		int index = data->findEntry(key);
		if (index >= 0) {
			return data->entries[index].value;
		}
		// Key not found - add with default value, then look it up
		// again and return a reference to the value in the map
		// so that it can be assigned to.
		static TValue defaultValue = TValue();
		Add(key, defaultValue);
		index = data->findEntry(key);
		if (index >= 0) {
			return data->entries[index].value;
		}
		return defaultValue;
	}

	const TValue& operator[](const TKey& key) const {
		if (!data) {
			static TValue defaultValue = TValue();
			return defaultValue;
		}
		int index = data->findEntry(key);
		if (index >= 0) {
			return data->entries[index].value;
		}
		static TValue defaultValue = TValue();
		return defaultValue;
	}

	// ContainsKey
	bool ContainsKey(const TKey& key) const {
		return data && data->findEntry(key) >= 0;
	}

	// TryGetValue - C# style
	bool TryGetValue(const TKey& key, TValue* value) const {
		if (!data) return false;
		int index = data->findEntry(key);
		if (index >= 0) {
			*value = data->entries[index].value;
			return true;
		}
		return false;
	}

	// Remove
	bool Remove(const TKey& key) {
		if (!data) return false;

		int hashCode = Hash(key);
		int capacity = static_cast<int>(data->buckets.size());
		int bucket = hashCode % capacity;

		int last = -1;
		for (int i = data->buckets[bucket]; i >= 0; last = i, i = data->entries[i].next) {
			if (data->entries[i].hashCode == hashCode && data->entries[i].key == key) {
				if (last < 0) {
					data->buckets[bucket] = data->entries[i].next;
				} else {
					data->entries[last].next = data->entries[i].next;
				}
				data->entries[i].next = -2;  // Mark as removed
				data->count--;
				return true;
			}
		}
		return false;
	}

	// Clear
	void Clear() {
		if (!data) return;

		int capacity = static_cast<int>(data->buckets.size());
		for (int i = 0; i < capacity; i++) {
			data->buckets[i] = -1;
		}
		data->count = 0;
	}

	// Iterator support - simple key iteration
	class KeyIterator {
	private:
		const DictionaryStorage<TKey, TValue>* storage;
		int index;

		void findNext() {
			if (!storage) return;
			while (index < storage->count && storage->entries[index].next < -1) {
				index++;
			}
		}

	public:
		KeyIterator(const DictionaryStorage<TKey, TValue>* s, int idx) : storage(s), index(idx) {
			findNext();
		}

		TKey operator*() const {
			return storage->entries[index].key;
		}

		KeyIterator& operator++() {
			index++;
			findNext();
			return *this;
		}

		bool operator!=(const KeyIterator& other) const {
			return index != other.index;
		}
	};

	class Keys {
	private:
		const DictionaryStorage<TKey, TValue>* storage;
	public:
		Keys(const DictionaryStorage<TKey, TValue>* s) : storage(s) {}
		KeyIterator begin() const { return KeyIterator(storage, 0); }
		KeyIterator end() const {
			return KeyIterator(storage, storage ? storage->count : 0);
		}
	};

	Keys GetKeys() const {
		return Keys(data.get());
	}

	// Iterator support - simple value iteration
	class ValueIterator {
	private:
		const DictionaryStorage<TKey, TValue>* storage;
		int index;

		void findNext() {
			if (!storage) return;
			while (index < storage->count && storage->entries[index].next < -1) {
				index++;
			}
		}

	public:
		ValueIterator(const DictionaryStorage<TKey, TValue>* s, int idx) : storage(s), index(idx) {
			findNext();
		}

		TValue operator*() const {
			return storage->entries[index].value;
		}

		ValueIterator& operator++() {
			index++;
			findNext();
			return *this;
		}

		bool operator!=(const ValueIterator& other) const {
			return index != other.index;
		}
	};

	class Values {
	private:
		const DictionaryStorage<TKey, TValue>* storage;
	public:
		Values(const DictionaryStorage<TKey, TValue>* s) : storage(s) {}
		ValueIterator begin() const { return ValueIterator(storage, 0); }
		ValueIterator end() const {
			return ValueIterator(storage, storage ? storage->count : 0);
		}
	};

	Values GetValues() const {
		return Values(data.get());
	}

	// Compatibility methods (poolNum ignored with shared_ptr)
	uint8_t getPoolNum() const { return 0; }
	bool isValid() const { return true; }  // Always valid with shared_ptr
};
