// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: MemPoolShim.cs

#include "MemPoolShim.g.h"

namespace MiniScript {

	// Shim class to provide cross-platform pool management.
	// In C#, these are no-ops since we don't use pools.
	// In C++, these map to actual MemPoolManager and StringPool calls.

		// Get an unused pool number (C++), or 0 (C#)
		Byte MemPoolShim::GetUnusedPool() {
			Byte result = MemPoolManager::findUnusedPool();
			// IOHelper.Print(StringUtils.Format("GetUnusedPool() --> {0}", (Int32)result));
			return result;
		}

		// Get the current default string pool
		Byte MemPoolShim::GetDefaultStringPool() {
			return String::defaultPool;
		}

		// Set the default string pool
		void MemPoolShim::SetDefaultStringPool(Byte poolNum) {
			// IOHelper.Print(StringUtils.Format("SetDefaultStringPool({0})", (Int32)poolNum));
			String::defaultPool = poolNum;
		}

		// Clear a string pool
		void MemPoolShim::ClearStringPool(Byte poolNum) {
			// IOHelper.Print(StringUtils.Format("ClearStringPool({0})", (Int32)poolNum));
			StringPool::clearPool(poolNum);
		}

		// Intern a string in the current default pool (C++), or just return it (C#)
		String MemPoolShim::InternString(String str) {
			return String(str.c_str());
		}
}
