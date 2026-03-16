// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: IntrinsicResult.cs

#include "IntrinsicResult.g.h"

namespace MiniScript {

IntrinsicResult::IntrinsicResult(Value result,Boolean done ) {
	this->result = result;
	this->done = done;
}
const IntrinsicResult IntrinsicResult::Null = IntrinsicResult(val_null);
const IntrinsicResult IntrinsicResult::EmptyString = IntrinsicResult(val_empty_string);
const IntrinsicResult IntrinsicResult::Zero = IntrinsicResult(val_zero);
const IntrinsicResult IntrinsicResult::One = IntrinsicResult(val_one);

} // end of namespace MiniScript
