// This is our Dictionary template used with transpiled C# --> C++ code.
// It matches the API and behavior of System.Collections.Generic.Dictionary
// as well as possible.  Memory management is done via MemPool.

#pragma once
#include "MemPool.h"
#include <initializer_list>
#include <utility>  // for std::pair

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

// DictionaryStorage - the actual storage for dictionary data
template<typename TKey, typename TValue>
struct DictionaryStorage {
	int count;
	int capacity;
	int freeList;  // Index of first free entry, or -1
	int freeCount;  // Count of entries in free list
	// Data follows: buckets array (int[capacity]) then entries array (DictEntry[capacity])

	// Helper to get the buckets array (stores indices into entries)
	int* getBuckets() {
		return reinterpret_cast<int*>(reinterpret_cast<char*>(this) + sizeof(DictionaryStorage<TKey, TValue>));
	}

	const int* getBuckets() const {
		return reinterpret_cast<const int*>(reinterpret_cast<const char*>(this) + sizeof(DictionaryStorage<TKey, TValue>));
	}

	// Helper to get the entries array
	DictEntry<TKey, TValue>* getEntries() {
		return reinterpret_cast<DictEntry<TKey, TValue>*>(
			reinterpret_cast<char*>(this) + sizeof(DictionaryStorage<TKey, TValue>) + capacity * sizeof(int));
	}

	const DictEntry<TKey, TValue>* getEntries() const {
		return reinterpret_cast<const DictEntry<TKey, TValue>*>(
			reinterpret_cast<const char*>(this) + sizeof(DictionaryStorage<TKey, TValue>) + capacity * sizeof(int));
	}

	// Calculate total size needed
	static size_t calculateSize(int capacity) {
		return sizeof(DictionaryStorage<TKey, TValue>) +
		       capacity * sizeof(int) +  // buckets
		       capacity * sizeof(DictEntry<TKey, TValue>);  // entries
	}

	// Initialize a new DictionaryStorage
	static void initialize(DictionaryStorage<TKey, TValue>* storage, int initialCapacity) {
		storage->count = 0;
		storage->capacity = initialCapacity;
		storage->freeList = -1;
		storage->freeCount = 0;

		// Initialize buckets to -1 (empty)
		int* buckets = storage->getBuckets();
		for (int i = 0; i < initialCapacity; i++) {
			buckets[i] = -1;
		}
	}
};

// Simple hash function for integers
inline int HashInt(int value) {
	return value & 0x7FFFFFFF;  // Ensure non-negative
}

// Dictionary - hash table using MemPool
template<typename TKey, typename TValue>
class Dictionary {
private:
	MemRef storage;  // Reference to DictionaryStorage<TKey, TValue>

	DictionaryStorage<TKey, TValue>* getStorage() const {
		return static_cast<DictionaryStorage<TKey, TValue>*>(MemPoolManager::getPtr(storage));
	}

	void createStorage(int initialCapacity) {
		if (initialCapacity < 4) initialCapacity = 4;
		storage = MemPoolManager::alloc(DictionaryStorage<TKey, TValue>::calculateSize(initialCapacity), storage.poolNum);
		if (!storage.isNull()) {
			DictionaryStorage<TKey, TValue>::initialize(getStorage(), initialCapacity);
		}
	}

	void resize() {
		DictionaryStorage<TKey, TValue>* oldStorage = getStorage();
		if (!oldStorage) return;

		int newCapacity = oldStorage->capacity * 2;
		if (newCapacity < 4) newCapacity = 4;

		// Allocate new storage
		MemRef newRef = MemPoolManager::alloc(
			DictionaryStorage<TKey, TValue>::calculateSize(newCapacity),
			storage.poolNum);
		if (newRef.isNull()) return;

		// Initialize new storage
		DictionaryStorage<TKey, TValue>* newStorage =
			static_cast<DictionaryStorage<TKey, TValue>*>(MemPoolManager::getPtr(newRef));
		DictionaryStorage<TKey, TValue>::initialize(newStorage, newCapacity);

		// Rehash all entries
		int* newBuckets = newStorage->getBuckets();
		DictEntry<TKey, TValue>* newEntries = newStorage->getEntries();
		const DictEntry<TKey, TValue>* oldEntries = oldStorage->getEntries();

		int newIndex = 0;
		for (int i = 0; i < oldStorage->capacity; i++) {
			if (oldEntries[i].next >= -1) {  // Entry is in use
				int bucket = oldEntries[i].hashCode % newCapacity;
				newEntries[newIndex].key = oldEntries[i].key;
				newEntries[newIndex].value = oldEntries[i].value;
				newEntries[newIndex].hashCode = oldEntries[i].hashCode;
				newEntries[newIndex].next = newBuckets[bucket];
				newBuckets[bucket] = newIndex;
				newIndex++;
			}
		}
		newStorage->count = newIndex;

		// Free old storage and use new
		MemPoolManager::free(storage);
		storage = newRef;
	}

	int findEntry(const TKey& key) const {
		DictionaryStorage<TKey, TValue>* s = getStorage();
		if (!s) return -1;

		int hashCode = HashInt(key);  // Assuming TKey is int for now
		int bucket = hashCode % s->capacity;

		const int* buckets = s->getBuckets();
		const DictEntry<TKey, TValue>* entries = s->getEntries();

		for (int i = buckets[bucket]; i >= 0; i = entries[i].next) {
			if (entries[i].hashCode == hashCode && entries[i].key == key) {
				return i;
			}
		}
		return -1;
	}

public:
	// Constructors
	Dictionary(uint8_t poolNum = 0) : storage(poolNum, 0) {}

	// Copy constructor and assignment
	Dictionary(const Dictionary<TKey, TValue>& other) = default;
	Dictionary<TKey, TValue>& operator=(const Dictionary<TKey, TValue>& other) = default;

	// Destructor
	~Dictionary() = default;

	// Properties
	int Count() const {
		DictionaryStorage<TKey, TValue>* s = getStorage();
		return s ? s->count : 0;
	}

	bool Empty() const { return Count() == 0; }

	// Add or update
	void Add(const TKey& key, const TValue& value) {
		DictionaryStorage<TKey, TValue>* s = getStorage();
		if (!s) {
			createStorage(4);
			s = getStorage();
			if (!s) return;
		}

		int hashCode = HashInt(key);
		int bucket = hashCode % s->capacity;

		int* buckets = s->getBuckets();
		DictEntry<TKey, TValue>* entries = s->getEntries();

		// Check if key already exists
		for (int i = buckets[bucket]; i >= 0; i = entries[i].next) {
			if (entries[i].hashCode == hashCode && entries[i].key == key) {
				// Update existing
				entries[i].value = value;
				return;
			}
		}

		// Add new entry
		if (s->count >= s->capacity * 0.75) {
			resize();
			s = getStorage();
			if (!s) return;
			buckets = s->getBuckets();
			entries = s->getEntries();
			bucket = hashCode % s->capacity;
		}

		int index = s->count;
		entries[index].key = key;
		entries[index].value = value;
		entries[index].hashCode = hashCode;
		entries[index].next = buckets[bucket];
		buckets[bucket] = index;
		s->count++;
	}

	// Indexer - get value by key (returns default if not found)
	TValue& operator[](const TKey& key) {
		int index = findEntry(key);
		DictionaryStorage<TKey, TValue>* s = getStorage();
		if (index >= 0 && s) {
			return s->getEntries()[index].value;
		}
		// Key not found - add with default value
		static TValue defaultValue = TValue();
		Add(key, defaultValue);
		index = findEntry(key);
		if (index >= 0 && s) {
			return s->getEntries()[index].value;
		}
		return defaultValue;
	}

	const TValue& operator[](const TKey& key) const {
		int index = findEntry(key);
		DictionaryStorage<TKey, TValue>* s = getStorage();
		if (index >= 0 && s) {
			return s->getEntries()[index].value;
		}
		static TValue defaultValue = TValue();
		return defaultValue;
	}

	// ContainsKey
	bool ContainsKey(const TKey& key) const {
		return findEntry(key) >= 0;
	}

	// TryGetValue - C# style
	bool TryGetValue(const TKey& key, TValue& value) const {
		int index = findEntry(key);
		if (index >= 0) {
			DictionaryStorage<TKey, TValue>* s = getStorage();
			if (s) {
				value = s->getEntries()[index].value;
				return true;
			}
		}
		return false;
	}

	// Remove
	bool Remove(const TKey& key) {
		DictionaryStorage<TKey, TValue>* s = getStorage();
		if (!s) return false;

		int hashCode = HashInt(key);
		int bucket = hashCode % s->capacity;

		int* buckets = s->getBuckets();
		DictEntry<TKey, TValue>* entries = s->getEntries();

		int last = -1;
		for (int i = buckets[bucket]; i >= 0; last = i, i = entries[i].next) {
			if (entries[i].hashCode == hashCode && entries[i].key == key) {
				if (last < 0) {
					buckets[bucket] = entries[i].next;
				} else {
					entries[last].next = entries[i].next;
				}
				entries[i].next = -2;  // Mark as removed
				s->count--;
				return true;
			}
		}
		return false;
	}

	// Clear
	void Clear() {
		DictionaryStorage<TKey, TValue>* s = getStorage();
		if (!s) return;

		int* buckets = s->getBuckets();
		for (int i = 0; i < s->capacity; i++) {
			buckets[i] = -1;
		}
		s->count = 0;
		s->freeList = -1;
		s->freeCount = 0;
	}

	// Iterator support - simple key iteration
	class KeyIterator {
	private:
		const Dictionary<TKey, TValue>* dict;
		int index;

		void findNext() {
			const DictionaryStorage<TKey, TValue>* s = dict->getStorage();
			if (!s) return;
			const DictEntry<TKey, TValue>* entries = s->getEntries();
			while (index < s->capacity && entries[index].next < -1) {
				index++;
			}
		}

	public:
		KeyIterator(const Dictionary<TKey, TValue>* d, int idx) : dict(d), index(idx) {
			findNext();
		}

		TKey operator*() const {
			const DictionaryStorage<TKey, TValue>* s = dict->getStorage();
			return s->getEntries()[index].key;
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
			const DictionaryStorage<TKey, TValue>* s = dict->getStorage();
			return KeyIterator(dict, s ? s->capacity : 0);
		}
	};

	Keys GetKeys() const {
		return Keys(this);
	}

	// Pool management
	uint8_t getPoolNum() const { return storage.poolNum; }
	MemRef getMemRef() const { return storage; }
	bool isValid() const { return !storage.isNull() && getStorage() != nullptr; }
};
