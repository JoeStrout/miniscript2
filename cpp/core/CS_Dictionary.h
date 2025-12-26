// This is our Dictionary template used with transpiled C# --> C++ code.
// It matches the API and behavior of System.Collections.Generic.Dictionary
// as well as possible.  Memory management is done via std::shared_ptr and std::vector.

#pragma once
#include <memory>
#include <vector>
#include <initializer_list>
#include <utility>  // for std::pair

// Forward declaration
class String;

// This module is part of Layer 3B (Host C# Compatibility Layer)
#define CORE_LAYER_3B

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

// Hash(const String&) is defined in CS_String.h to avoid circular dependencies

// Dictionary - hash table using std::shared_ptr and std::vector
template<typename TKey, typename TValue>
class Dictionary {
private:
	std::shared_ptr<std::vector<int>> buckets;
	std::shared_ptr<std::vector<DictEntry<TKey, TValue>>> entries;
	int count;

	void ensureData() {
		if (!buckets || !entries) {
			createStorage(4);
		}
	}

	void createStorage(int initialCapacity) {
		if (initialCapacity < 4) initialCapacity = 4;
		buckets = std::make_shared<std::vector<int>>(initialCapacity, -1);
		entries = std::make_shared<std::vector<DictEntry<TKey, TValue>>>(initialCapacity);
		count = 0;
	}

	void resize() {
		if (!buckets || !entries) return;

		int oldCapacity = static_cast<int>(buckets->size());
		int newCapacity = oldCapacity * 2;
		if (newCapacity < 4) newCapacity = 4;

		// Create new buckets and entries
		auto newBuckets = std::make_shared<std::vector<int>>(newCapacity, -1);
		auto newEntries = std::make_shared<std::vector<DictEntry<TKey, TValue>>>(newCapacity);

		// Rehash all active entries by following bucket chains
		int newIndex = 0;
		for (int bucket = 0; bucket < oldCapacity; bucket++) {
			for (int i = (*buckets)[bucket]; i >= 0; i = (*entries)[i].next) {
				int newBucket = (*entries)[i].hashCode % newCapacity;
				(*newEntries)[newIndex].key = (*entries)[i].key;
				(*newEntries)[newIndex].value = (*entries)[i].value;
				(*newEntries)[newIndex].hashCode = (*entries)[i].hashCode;
				(*newEntries)[newIndex].next = (*newBuckets)[newBucket];
				(*newBuckets)[newBucket] = newIndex;
				newIndex++;
			}
		}

		// Use new storage
		buckets = newBuckets;
		entries = newEntries;
		count = newIndex;
	}

	int findEntry(const TKey& key) const {
		if (!buckets || !entries) return -1;

		int hashCode = Hash(key);
		int capacity = static_cast<int>(buckets->size());
		int bucket = hashCode % capacity;

		for (int i = (*buckets)[bucket]; i >= 0; i = (*entries)[i].next) {
			if ((*entries)[i].hashCode == hashCode && (*entries)[i].key == key) {
				return i;
			}
		}
		return -1;
	}

public:
	// Constructors
	Dictionary(uint8_t /*poolNum*/ = 0) : count(0) {}

	// Copy constructor and assignment
	Dictionary(const Dictionary<TKey, TValue>& other) = default;
	Dictionary<TKey, TValue>& operator=(const Dictionary<TKey, TValue>& other) = default;

	// Destructor
	~Dictionary() = default;

	// Properties
	int Count() const {
		return count;
	}

	bool Empty() const { return Count() == 0; }

	// Add or update
	void Add(const TKey& key, const TValue& value) {
		ensureData();

		int hashCode = Hash(key);
		int capacity = static_cast<int>(buckets->size());
		int bucket = hashCode % capacity;

		// Check if key already exists
		for (int i = (*buckets)[bucket]; i >= 0; i = (*entries)[i].next) {
			if ((*entries)[i].hashCode == hashCode && (*entries)[i].key == key) {
				// Update existing
				(*entries)[i].value = value;
				return;
			}
		}

		// Add new entry - resize if needed (use threshold = 3/4 of capacity)
		if (count * 4 >= capacity * 3) {
			resize();
			capacity = static_cast<int>(buckets->size());
			bucket = hashCode % capacity;
		}

		int index = count;
		(*entries)[index].key = key;
		(*entries)[index].value = value;
		(*entries)[index].hashCode = hashCode;
		(*entries)[index].next = (*buckets)[bucket];
		(*buckets)[bucket] = index;
		count++;
	}

	// Indexer - get value by key (returns default if not found)
	TValue& operator[](const TKey& key) {
		int index = findEntry(key);
		if (index >= 0) {
			return (*entries)[index].value;
		}
		// Key not found - add with default value, then look it up
		// again and return a reference to the value in the map
		// so that it can be assigned to.
		static TValue defaultValue = TValue();
		Add(key, defaultValue);
		index = findEntry(key);
		if (index >= 0) {
			return (*entries)[index].value;
		}
		return defaultValue;
	}

	const TValue& operator[](const TKey& key) const {
		int index = findEntry(key);
		if (index >= 0) {
			return (*entries)[index].value;
		}
		static TValue defaultValue = TValue();
		return defaultValue;
	}

	// ContainsKey
	bool ContainsKey(const TKey& key) const {
		return findEntry(key) >= 0;
	}

	// TryGetValue - C# style
	bool TryGetValue(const TKey& key, TValue* value) const {
		int index = findEntry(key);
		if (index >= 0) {
			*value = (*entries)[index].value;
			return true;
		}
		return false;
	}

	// Remove
	bool Remove(const TKey& key) {
		if (!buckets || !entries) return false;

		int hashCode = Hash(key);
		int capacity = static_cast<int>(buckets->size());
		int bucket = hashCode % capacity;

		int last = -1;
		for (int i = (*buckets)[bucket]; i >= 0; last = i, i = (*entries)[i].next) {
			if ((*entries)[i].hashCode == hashCode && (*entries)[i].key == key) {
				if (last < 0) {
					(*buckets)[bucket] = (*entries)[i].next;
				} else {
					(*entries)[last].next = (*entries)[i].next;
				}
				(*entries)[i].next = -2;  // Mark as removed
				count--;
				return true;
			}
		}
		return false;
	}

	// Clear
	void Clear() {
		if (!buckets || !entries) return;

		int capacity = static_cast<int>(buckets->size());
		for (int i = 0; i < capacity; i++) {
			(*buckets)[i] = -1;
		}
		count = 0;
	}

	// Iterator support - simple key iteration
	class KeyIterator {
	private:
		const Dictionary<TKey, TValue>* dict;
		int index;

		void findNext() {
			if (!dict->entries) return;
			// Only iterate through active entries (0 to count-1)
			while (index < dict->count && (*dict->entries)[index].next < -1) {
				index++;
			}
		}

	public:
		KeyIterator(const Dictionary<TKey, TValue>* d, int idx) : dict(d), index(idx) {
			findNext();
		}

		TKey operator*() const {
			return (*dict->entries)[index].key;
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
		const Dictionary<TKey, TValue>* dict;
	public:
		Keys(const Dictionary<TKey, TValue>* d) : dict(d) {}
		KeyIterator begin() const { return KeyIterator(dict, 0); }
		KeyIterator end() const {
			return KeyIterator(dict, dict->count);
		}
	};

	Keys GetKeys() const {
		return Keys(this);
	}

	// Compatibility methods (poolNum ignored with shared_ptr)
	uint8_t getPoolNum() const { return 0; }
	bool isValid() const { return true; }  // Always valid with shared_ptr
};
