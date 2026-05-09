//*** BEGIN CS_ONLY ***
using System;
using static MiniScript.ValueHelpers;

namespace MiniScript {
	/// <summary>
	/// ValueError represents a runtime error value in MiniScript 2.0.
	/// Errors are immutable, first-class values that propagate through
	/// most expressions (see LANGUAGE_CHANGES.md "Error Type").
	/// </summary>
	public class ValueError {
		/// <summary>The error message (a string Value).</summary>
		public Value Message { get; }

		/// <summary>An inner (wrapped) error, or null.</summary>
		public Value Inner { get; }

		/// <summary>Stack trace at creation time (a frozen list of strings).
		/// Currently stubbed: full source-location tracking not yet implemented.</summary>
		public Value Stack { get; }

		/// <summary>Optional __isa link to a more general error (used by
		/// se.err(msg, e) specialization), or null.</summary>
		public Value Isa { get; }

		public ValueError(Value message, Value inner, Value stack, Value isa) {
			Message = message;
			Inner = inner;
			Stack = stack;
			Isa = isa;
		}

		public override string ToString() {
			string msg = is_string(Message) ? as_cstring(Message) : Message.ToString(null);
			return "error: " + msg;
		}
	}
}
//*** END CS_ONLY ***
