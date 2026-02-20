// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: CoreIntrinsics.cs

#include "CoreIntrinsics.g.h"
#include "gc.h"
#include "value_list.h"
#include "value_string.h"
#include "value_map.h"
#include "IOHelper.g.h"
#include "StringUtils.g.h"
#include "VM.g.h"

namespace MiniScript {

void CoreIntrinsics::Init() {
	Intrinsic f;

	// print(s="")
	f = Intrinsic::Create("print");
	f.AddParam("s", make_string(""));
	f.set_Code([](List<Value> stk, Int32 bi, Int32 ac) -> Value {
		String output = StringUtils::Format("{0}", stk[bi + 1]);
		VM vm = VM::ActiveVM();
		if (VMStorage::sPrintCallback) {
			VMStorage::sPrintCallback(output);
		} else {
			IOHelper::Print(output);
		}
		return make_null();
	});

	// input(prompt=null)
	f = Intrinsic::Create("input");
	f.AddParam("prompt");
	f.set_Code([](List<Value> stk, Int32 bi, Int32 ac) -> Value {
		String prompt =  String::New("");
		if (!is_null(stk[bi + 1])) {
			prompt = StringUtils::Format("{0}", stk[bi + 1]);
		}
		String result = IOHelper::Input(prompt);
		return make_string(result);
	});

	// val(x)
	f = Intrinsic::Create("val");
	f.AddParam("x");
	f.set_Code([](List<Value> stk, Int32 bi, Int32 ac) -> Value {
		return to_number(stk[bi + 1]);
	});

	// str(x)
	f = Intrinsic::Create("str");
	f.AddParam("x");
	f.set_Code([](List<Value> stk, Int32 bi, Int32 ac) -> Value {
		return make_string(StringUtils::Format("{0}", stk[bi + 1]));
	});

	// len(x)
	f = Intrinsic::Create("len");
	f.AddParam("x");
	f.set_Code([](List<Value> stk, Int32 bi, Int32 ac) -> Value {
		GC_PUSH_SCOPE();
		Value container = stk[bi + 1]; GC_PROTECT(&container);
		Value result = make_null(); GC_PROTECT(&result);
		if (is_list(container)) {
			result = make_int(list_count(container));
		} else if (is_string(container)) {
			result = make_int(string_length(container));
		} else if (is_map(container)) {
			result = make_int(map_count(container));
		}
		GC_POP_SCOPE();
		return result;
	});

	// remove(self, index)
	f = Intrinsic::Create("remove");
	f.AddParam("self");
	f.AddParam("index");
	f.set_Code([](List<Value> stk, Int32 bi, Int32 ac) -> Value {
		GC_PUSH_SCOPE();
		Value container = stk[bi + 1]; GC_PROTECT(&container);
		int result = 0;
		if (is_list(container)) {
			result = list_remove(container, as_int(stk[bi + 2])) ? 1 : 0;
		} else if (is_map(container)) {
			result = map_remove(container, stk[bi + 2]) ? 1 : 0;
		} else {
			IOHelper::Print("ERROR: `remove` must be called on list or map");
		}
		GC_POP_SCOPE();
		return make_int(result);
	});

	// freeze(x)
	f = Intrinsic::Create("freeze");
	f.AddParam("x");
	f.set_Code([](List<Value> stk, Int32 bi, Int32 ac) -> Value {
		freeze_value(stk[bi + 1]);
		return make_null();
	});

	// isFrozen(x)
	f = Intrinsic::Create("isFrozen");
	f.AddParam("x");
	f.set_Code([](List<Value> stk, Int32 bi, Int32 ac) -> Value {
		return make_int(is_frozen(stk[bi + 1]));
	});

	// frozenCopy(x)
	f = Intrinsic::Create("frozenCopy");
	f.AddParam("x");
	f.set_Code([](List<Value> stk, Int32 bi, Int32 ac) -> Value {
		return frozen_copy(stk[bi + 1]);
	});
}

} // end of namespace MiniScript
