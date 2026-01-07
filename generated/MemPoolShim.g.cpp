// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: MemPoolShim.cs

#include "MemPoolShim.g.h"
#include "MemPool.h"
#include "StringPool.h"
#include "IOHelper.g.h"
#include "StringUtils.g.h"

namespace MiniScript {


Byte MemPoolShim::GetUnusedPool() {
		Byte result = MemPoolManager::findUnusedPool();
		// IOHelper::Print(StringUtils::Format("GetUnusedPool() --> {0}", (Int32)result));
		return result;
	}

	// Get the current default string pool
	public static Byte GetDefaultStringPool() {
		return String::defaultPool;
	}

	// Set the default string pool
	public static void SetDefaultStringPool(Byte poolNum) {
		// IOHelper::Print(StringUtils::Format("SetDefaultStringPool({0})", (Int32)poolNum));
		String::defaultPool = poolNum;
	}

	// Clear a string pool
	public static void ClearStringPool(Byte poolNum) {
		// IOHelper::Print(StringUtils::Format("ClearStringPool({0})", (Int32)poolNum));
		StringPool::clearPool(poolNum);
	}

	// Intern a string in the current default pool (C++), or just return it (C#)
	public static String InternString(String str) {
		return String(str.c_str());
	}
}


} // end of namespace MiniScript
