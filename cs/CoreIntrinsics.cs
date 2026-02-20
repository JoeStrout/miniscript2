// CoreIntrinsics.cs - Definitions of all built-in intrinsic functions.

using System;
using System.Collections.Generic;
using static MiniScript.ValueHelpers;
// H: #include "value.h"
// H: #include "Intrinsic.g.h"
// CPP: #include "gc.h"
// CPP: #include "value_list.h"
// CPP: #include "value_string.h"
// CPP: #include "value_map.h"
// CPP: #include "IOHelper.g.h"
// CPP: #include "StringUtils.g.h"
// CPP: #include "VM.g.h"

namespace MiniScript {

public static class CoreIntrinsics {

	public static void Init() {
		Intrinsic f;

		// print(s="")
		f = Intrinsic.Create("print");
		f.AddParam("s", make_string(""));
		f.Code = (List<Value> stk, Int32 bi, Int32 ac) => {
			String output = StringUtils.Format("{0}", stk[bi + 1]);
			VM vm = VM.ActiveVM();
			if (vm.GetPrintCallback() != null) {  // CPP: if (VMStorage::sPrintCallback) {
				vm.GetPrintCallback()(output);    // CPP: VMStorage::sPrintCallback(output);
			} else {
				IOHelper.Print(output);
			}
			return make_null();
		};

		// input(prompt=null)
		f = Intrinsic.Create("input");
		f.AddParam("prompt");
		f.Code = (List<Value> stk, Int32 bi, Int32 ac) => {
			String prompt = new String("");
			if (!is_null(stk[bi + 1])) {
				prompt = StringUtils.Format("{0}", stk[bi + 1]);
			}
			String result = IOHelper.Input(prompt);
			return make_string(result);
		};

		// val(x)
		f = Intrinsic.Create("val");
		f.AddParam("x");
		f.Code = (List<Value> stk, Int32 bi, Int32 ac) => {
			return to_number(stk[bi + 1]);
		};

		// str(x)
		f = Intrinsic.Create("str");
		f.AddParam("x");
		f.Code = (List<Value> stk, Int32 bi, Int32 ac) => {
			return make_string(StringUtils.Format("{0}", stk[bi + 1]));
		};

		// len(x)
		f = Intrinsic.Create("len");
		f.AddParam("x");
		f.Code = (List<Value> stk, Int32 bi, Int32 ac) => {
			Value container = stk[bi + 1];
			Value result = make_null();
			if (is_list(container)) {
				result = make_int(list_count(container));
			} else if (is_string(container)) {
				result = make_int(string_length(container));
			} else if (is_map(container)) {
				result = make_int(map_count(container));
			}
			return result;
		};

		// remove(self, index)
		f = Intrinsic.Create("remove");
		f.AddParam("self");
		f.AddParam("index");
		f.Code = (List<Value> stk, Int32 bi, Int32 ac) => {
			Value container = stk[bi + 1];
			int result = 0;
			if (is_list(container)) {
				result = list_remove(container, as_int(stk[bi + 2])) ? 1 : 0;
			} else if (is_map(container)) {
				result = map_remove(container, stk[bi + 2]) ? 1 : 0;
			} else {
				IOHelper.Print("ERROR: `remove` must be called on list or map");
			}
			return make_int(result);
		};

		// freeze(x)
		f = Intrinsic.Create("freeze");
		f.AddParam("x");
		f.Code = (List<Value> stk, Int32 bi, Int32 ac) => {
			freeze_value(stk[bi + 1]);
			return make_null();
		};

		// isFrozen(x)
		f = Intrinsic.Create("isFrozen");
		f.AddParam("x");
		f.Code = (List<Value> stk, Int32 bi, Int32 ac) => {
			return make_int(is_frozen(stk[bi + 1]));
		};

		// frozenCopy(x)
		f = Intrinsic.Create("frozenCopy");
		f.AddParam("x");
		f.Code = (List<Value> stk, Int32 bi, Int32 ac) => {
			return frozen_copy(stk[bi + 1]);
		};
	}
}

}
