using System;
using System.Runtime.CompilerServices;
using static System.Runtime.CompilerServices.MethodImplOptions;
using static MiniScript.ValueHelpers;
// H: #include "value.h"

namespace MiniScript {

// IntrinsicResult: represents the result of calling an intrinsic function
// (i.e. a function defined by the host app, for use in MiniScript code).
// This may be a final or "done" result, containing the return value of
// the intrinsic; or it may be a partial or "not done yet" result, in which
// case the intrinsic will be invoked again, with this partial result
// passed back so the intrinsic can continue its work.
public struct IntrinsicResult {
	public Boolean done;	// set to true if done, false if there is pending work
	public Value result;	// final result if done; in-progress data if not done

	public IntrinsicResult(Value result, Boolean done = true) {
		this.result = result;
		this.done = done;
	}

	// For backwards compatibility with 1.x:
	[MethodImpl(AggressiveInlining)]
	public Boolean Done() {
		return done;
	}

	// Some standard results you can efficiently use:
	public static readonly IntrinsicResult Null = new IntrinsicResult(val_null);
	public static readonly IntrinsicResult EmptyString = new IntrinsicResult(val_empty_string);
	public static readonly IntrinsicResult Zero = new IntrinsicResult(val_zero);
	public static readonly IntrinsicResult One = new IntrinsicResult(val_one);
}

}
