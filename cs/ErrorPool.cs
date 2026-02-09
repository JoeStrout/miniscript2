// ErrorPool.cs - Simple error collection for MiniScript compilation and execution.
// Shared across lexer, parser, code generator, and VM stages.

using System;
using System.Collections.Generic;

namespace MiniScript {

public struct ErrorPool {
	private List<String> _errors;

	// Use Create() to get an initialized ErrorPool before copying it.
	// This ensures all copies share the same underlying list.
	public static ErrorPool Create() {
		ErrorPool pool = new ErrorPool();
		pool._errors = new List<String>();
		return pool;
	}

	private void EnsureList() {
		if (_errors == null) _errors = new List<String>();
	}

	public void Add(String message) {
		EnsureList();
		_errors.Add(message);
	}

	public Boolean HasError() {
		return _errors != null && _errors.Count > 0;
	}

	public String TopError() {
		if (_errors == null || _errors.Count == 0) return null;
		return _errors[0];
	}

	public List<String> GetErrors() {
		EnsureList();
		return _errors;
	}

	public void Clear() {
		if (_errors != null) _errors.Clear();
		else _errors = new List<String>();
	}
}

}
