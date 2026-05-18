// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: ShellIntrinsics.cs

#include "ShellIntrinsics.g.h"
#include "Intrinsic.g.h"
#include "IntrinsicAPI.g.h"
#include "VM.g.h"
#include "value_list.h"
#include "value_map.h"
#include "value_string.h"
#include "StringUtils.g.h"
#include <cstdlib>
#include <cstring>
extern "C" {
extern char **environ;
}

namespace MiniScript {

Boolean ShellIntrinsics::ExitASAP = Boolean(false);
Int32 ShellIntrinsics::ExitCode = 0;
Value ShellIntrinsics::_shellArgs = val_null;
Value ShellIntrinsics::_envMap = val_null;
void ShellIntrinsics::SetShellArgs(List<String> args,Int32 startIdx) {
	Int32 count = args.Count() - startIdx;
	if (count < 0) count = 0;
	_shellArgs = make_list(count);
	for (Int32 i = startIdx; i < args.Count(); i++) {
		list_push(_shellArgs, make_string(args[i]));
	}
	freeze_value(_shellArgs);
}
Value ShellIntrinsics::GetEnvMap() {
	if (!is_null(_envMap)) return _envMap;
	_envMap = make_map(64);
	for (char **current = environ; *current; current++) {
		char* eqPos = strchr(*current, '=');
		if (!eqPos) continue;
		String varName(*current, (size_t)(eqPos - *current));
		String valueStr(eqPos + 1);
		map_set(_envMap, make_string(varName), make_string(valueStr));
	}
	return _envMap;
}
void ShellIntrinsics::MarkRoots(object userData) {
	GCManager::Mark(_shellArgs);
	GCManager::Mark(_envMap);
}
void ShellIntrinsics::InvalidateCaches() {
	_shellArgs = val_null;
	_envMap = val_null;
}
void ShellIntrinsics::Init() {
	GCManager::RegisterMarkCallback(ShellIntrinsics::MarkRoots, nullptr);

	Intrinsic f;

	// exit(resultCode=null) — stop the VM and signal the host to exit.
	f = Intrinsic::Create("exit");
	f.AddParam("resultCode", val_null);
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		ShellIntrinsics::ExitASAP = Boolean(true);
		Value resultCode = ctx.GetArg(0);
		if (!is_null(resultCode)) {
			ShellIntrinsics::ExitCode = (Int32)as_double(resultCode);
		}
		ctx.vm.Stop();
		return IntrinsicResult::Null;
	});

	// shellArgs — return the command-line arguments following the script path.
	f = Intrinsic::Create("shellArgs");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		return IntrinsicResult(_shellArgs);
	});

	// env — return a map of all environment variables.
	f = Intrinsic::Create("env");
	f.set_Code([](Context ctx, IntrinsicResult partialResult) -> IntrinsicResult {
		return IntrinsicResult(GetEnvMap());
	});
}

} // end of namespace MiniScript
