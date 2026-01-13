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
Byte MemPoolShim::GetDefaultStringPool() {
	return String::defaultPool;
}
void MemPoolShim::SetDefaultStringPool(Byte poolNum) {
	// IOHelper::Print(StringUtils::Format("SetDefaultStringPool({0})", (Int32)poolNum));
	String::defaultPool = poolNum;
}
void MemPoolShim::ClearStringPool(Byte poolNum) {
	// IOHelper::Print(StringUtils::Format("ClearStringPool({0})", (Int32)poolNum));
	StringPool::clearPool(poolNum);
}
String MemPoolShim::InternString(String str) {
	return String(str.c_str());
}


} // end of namespace MiniScript
