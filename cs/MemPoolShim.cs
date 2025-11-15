using System;
// CPP: #include "MemPool.h"
// CPP: #include "StringPool.h"
// CPP: #include "IOHelper.g.h"
// CPP: #include "StringUtils.g.h"

namespace MiniScript {

	// Shim class to provide cross-platform pool management.
	// In C#, these are no-ops since we don't use pools.
	// In C++, these map to actual MemPoolManager and StringPool calls.
	public static class MemPoolShim {

		// Get an unused pool number (C++), or 0 (C#)
		public static Byte GetUnusedPool() {
			Byte result = 0; // CPP: Byte result = MemPoolManager::findUnusedPool();
			// IOHelper.Print(StringUtils.Format("GetUnusedPool() --> {0}", (Int32)result));
			return result;
		}

		// Get the current default string pool
		public static Byte GetDefaultStringPool() {
			return 0; // CPP: return String::defaultPool;
		}

		// Set the default string pool
		public static void SetDefaultStringPool(Byte poolNum) {
			// IOHelper.Print(StringUtils.Format("SetDefaultStringPool({0})", (Int32)poolNum));
			// CPP: String::defaultPool = poolNum;
		}

		// Clear a string pool
		public static void ClearStringPool(Byte poolNum) {
			// IOHelper.Print(StringUtils.Format("ClearStringPool({0})", (Int32)poolNum));
			// CPP: StringPool::clearPool(poolNum);
		}

		// Intern a string in the current default pool (C++), or just return it (C#)
		public static String InternString(String str) {
			return str; // CPP: return String(str.c_str());
		}
	}
}
