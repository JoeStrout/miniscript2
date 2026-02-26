// ValueTest.cs
// Tests for Value module (Layer 1 - critical foundation)

using System;
using static MiniScript.ValueHelpers;
// CPP: #include "gc.h"
// CPP: #include "value.h"
// CPP: #include "value_list.h"
// CPP: #include "value_map.h"
// CPP: #include "value_string.h"
// CPP: #include "IOHelper.g.h"
// CPP: #include "TestFramework.g.h"

namespace MiniScript {

public static class ValueTest {

	public static Boolean TestNullValue() {
		// CPP: GC_PUSH_SCOPE();
		Value v = val_null;
		Boolean ok = true;
		ok = TestFramework.Assert(is_null(v), "make_null creates null value") && ok;
		ok = TestFramework.Assert(!is_int(v), "null is not int") && ok;
		ok = TestFramework.Assert(!is_double(v), "null is not double") && ok;
		ok = TestFramework.Assert(!is_string(v), "null is not string") && ok;
		// CPP: GC_POP_SCOPE();
		return ok;
	}

	public static Boolean TestIntValue() {
		// CPP: GC_PUSH_SCOPE();
		Value v = make_int(42);
		Boolean ok = true;
		ok = TestFramework.Assert(is_int(v), "make_int creates int value") && ok;
		ok = TestFramework.AssertEqual(as_int(v), 42, "int value is 42") && ok;
		ok = TestFramework.Assert(!is_null(v), "int is not null") && ok;
		ok = TestFramework.Assert(!is_double(v), "int is not double") && ok;
		ok = TestFramework.Assert(is_number(v), "int is a number") && ok;
		// CPP: GC_POP_SCOPE();
		return ok;
	}

	public static Boolean TestNegativeInt() {
		// CPP: GC_PUSH_SCOPE();
		Value v = make_int(-100);
		Boolean ok = true;
		ok = TestFramework.Assert(is_int(v), "make_int creates int value") && ok;
		ok = TestFramework.AssertEqual(as_int(v), -100, "int value is -100") && ok;
		// CPP: GC_POP_SCOPE();
		return ok;
	}

	public static Boolean TestDoubleValue() {
		// CPP: GC_PUSH_SCOPE();
		Value v = make_double(3.14);
		Boolean ok = true;
		ok = TestFramework.Assert(is_double(v), "make_double creates double value") && ok;
		ok = TestFramework.Assert(!is_int(v), "double is not int") && ok;
		ok = TestFramework.Assert(!is_null(v), "double is not null") && ok;
		ok = TestFramework.Assert(is_number(v), "double is a number") && ok;
		// CPP: GC_POP_SCOPE();
		return ok;
	}

	public static Boolean TestStringValue() {
		// CPP: GC_PUSH_SCOPE();
		Value v = make_string("hello");
		Boolean ok = true;
		ok = TestFramework.Assert(is_string(v), "make_string creates string value") && ok;
		ok = TestFramework.AssertEqual(as_cstring(v), "hello", "string value is 'hello'") && ok;
		ok = TestFramework.Assert(!is_int(v), "string is not int") && ok;
		ok = TestFramework.Assert(!is_null(v), "string is not null") && ok;
		// CPP: GC_POP_SCOPE();
		return ok;
	}

	public static Boolean TestEmptyString() {
		// CPP: GC_PUSH_SCOPE();
		Value v = make_string("");
		Boolean ok = true;
		ok = TestFramework.Assert(is_string(v), "empty string is a string") && ok;
		ok = TestFramework.AssertEqual(as_cstring(v), "", "empty string value") && ok;
		// CPP: GC_POP_SCOPE();
		return ok;
	}

	public static Boolean TestIntArithmetic() {
		// CPP: GC_PUSH_SCOPE();
		Value a = make_int(10);
		Value b = make_int(5);
		Boolean ok = true;

		Value sum = value_add(a, b);
		ok = TestFramework.Assert(is_int(sum), "int + int = int") && ok;
		ok = TestFramework.AssertEqual(as_int(sum), 15, "10 + 5 = 15") && ok;

		Value diff = value_sub(a, b);
		ok = TestFramework.Assert(is_int(diff), "int - int = int") && ok;
		ok = TestFramework.AssertEqual(as_int(diff), 5, "10 - 5 = 5") && ok;

		Value prod = value_mult(a, b);
		ok = TestFramework.Assert(is_int(prod), "int * int = int") && ok;
		ok = TestFramework.AssertEqual(as_int(prod), 50, "10 * 5 = 50") && ok;

		Value quot = value_div(a, b);
		ok = TestFramework.Assert(is_int(quot), "int / int = int") && ok;
		ok = TestFramework.AssertEqual(as_int(quot), 2, "10 / 5 = 2") && ok;

		// CPP: GC_POP_SCOPE();
		return ok;
	}

	public static Boolean TestIntComparison() {
		// CPP: GC_PUSH_SCOPE();
		Value a = make_int(10);
		Value b = make_int(5);
		Value c = make_int(10);
		Boolean ok = true;

		ok = TestFramework.Assert(value_lt(b, a), "5 < 10") && ok;
		ok = TestFramework.Assert(!value_lt(a, b), "!(10 < 5)") && ok;
		ok = TestFramework.Assert(!value_lt(a, c), "!(10 < 10)") && ok;

		ok = TestFramework.Assert(value_le(b, a), "5 <= 10") && ok;
		ok = TestFramework.Assert(value_le(a, c), "10 <= 10") && ok;
		ok = TestFramework.Assert(!value_le(a, b), "!(10 <= 5)") && ok;

		ok = TestFramework.Assert(value_equal(a, c), "10 == 10") && ok;
		ok = TestFramework.Assert(!value_equal(a, b), "!(10 == 5)") && ok;

		// CPP: GC_POP_SCOPE();
		return ok;
	}

	public static Boolean TestStringComparison() {
		// CPP: GC_PUSH_SCOPE();
		Value a = make_string("hello");
		Value b = make_string("world");
		Value c = make_string("hello");
		Boolean ok = true;

		ok = TestFramework.Assert(value_equal(a, c), "\"hello\" == \"hello\"") && ok;
		ok = TestFramework.Assert(!value_equal(a, b), "!(\"hello\" == \"world\")") && ok;
		ok = TestFramework.Assert(value_lt(a, b), "\"hello\" < \"world\"") && ok;

		// CPP: GC_POP_SCOPE();
		return ok;
	}

	public static Boolean TestStringConcatenation() {
		// CPP: GC_PUSH_SCOPE();
		Value a = make_string("Hello");
		Value b = make_string(" World");
		Value result = value_add(a, b);
		Boolean ok = true;

		ok = TestFramework.Assert(is_string(result), "string + string = string") && ok;
		ok = TestFramework.AssertEqual(as_cstring(result), "Hello World", "concatenation result") && ok;

		// CPP: GC_POP_SCOPE();
		return ok;
	}

	public static Boolean TestTruthiness() {
		// CPP: GC_PUSH_SCOPE();
		Boolean ok = true;

		// Null is falsy
		ok = TestFramework.Assert(!is_truthy(val_null), "null is falsy") && ok;

		// Zero is falsy
		ok = TestFramework.Assert(!is_truthy(make_int(0)), "0 is falsy") && ok;

		// Non-zero is truthy
		ok = TestFramework.Assert(is_truthy(make_int(1)), "1 is truthy") && ok;
		ok = TestFramework.Assert(is_truthy(make_int(-1)), "-1 is truthy") && ok;

		// Empty string is falsy
		ok = TestFramework.Assert(!is_truthy(make_string("")), "empty string is falsy") && ok;

		// Non-empty string is truthy
		ok = TestFramework.Assert(is_truthy(make_string("x")), "non-empty string is truthy") && ok;

		// CPP: GC_POP_SCOPE();
		return ok;
	}

	public static Boolean TestListCreation() {
		// CPP: GC_PUSH_SCOPE();
		Value list = make_empty_list();
		Boolean ok = true;

		ok = TestFramework.Assert(is_list(list), "make_empty_list creates list") && ok;
		ok = TestFramework.AssertEqual(list_count(list), 0, "empty list has count 0") && ok;
		ok = TestFramework.Assert(!is_null(list), "list is not null") && ok;
		ok = TestFramework.Assert(!is_int(list), "list is not int") && ok;

		// CPP: GC_POP_SCOPE();
		return ok;
	}

	public static Boolean TestListOperations() {
		// CPP: GC_PUSH_SCOPE();
		Value list = make_empty_list();
		Boolean ok = true;

		// Add items
		list_push(list, make_int(10));
		list_push(list, make_int(20));
		list_push(list, make_int(30));

		ok = TestFramework.AssertEqual(list_count(list), 3, "list has 3 items") && ok;

		// Get items
		Value item0 = list_get(list, 0);
		Value item1 = list_get(list, 1);
		Value item2 = list_get(list, 2);

		ok = TestFramework.AssertEqual(as_int(item0), 10, "list[0] == 10") && ok;
		ok = TestFramework.AssertEqual(as_int(item1), 20, "list[1] == 20") && ok;
		ok = TestFramework.AssertEqual(as_int(item2), 30, "list[2] == 30") && ok;

		// Set item
		list_set(list, 1, make_int(25));
		Value modified = list_get(list, 1);
		ok = TestFramework.AssertEqual(as_int(modified), 25, "list[1] modified to 25") && ok;

		// CPP: GC_POP_SCOPE();
		return ok;
	}

	public static Boolean TestMapCreation() {
		// CPP: GC_PUSH_SCOPE();
		Value map = make_empty_map();
		Boolean ok = true;

		ok = TestFramework.Assert(is_map(map), "make_empty_map creates map") && ok;
		ok = TestFramework.AssertEqual(map_count(map), 0, "empty map has count 0") && ok;
		ok = TestFramework.Assert(!is_null(map), "map is not null") && ok;

		// CPP: GC_POP_SCOPE();
		return ok;
	}

	public static Boolean TestMapOperations() {
		// CPP: GC_PUSH_SCOPE();
		Value map = make_empty_map();
		Boolean ok = true;

		// Set values
		Value key1 = make_string("name");
		Value key2 = make_string("age");
		map_set(map, key1, make_string("Alice"));
		map_set(map, key2, make_int(30));

		ok = TestFramework.AssertEqual(map_count(map), 2, "map has 2 entries") && ok;

		// Get values
		Value name = map_get(map, key1);
		Value age = map_get(map, key2);

		ok = TestFramework.AssertEqual(as_cstring(name), "Alice", "map[\"name\"] == \"Alice\"") && ok;
		ok = TestFramework.AssertEqual(as_int(age), 30, "map[\"age\"] == 30") && ok;

		// Test has_key
		ok = TestFramework.Assert(map_has_key(map, key1), "map has key \"name\"") && ok;
		ok = TestFramework.Assert(!map_has_key(map, make_string("missing")), "map doesn't have key \"missing\"") && ok;

		// Test remove
		Boolean removed = map_remove(map, key1);
		ok = TestFramework.Assert(removed, "remove returned true") && ok;
		ok = TestFramework.AssertEqual(map_count(map), 1, "map has 1 entry after remove") && ok;
		ok = TestFramework.Assert(!map_has_key(map, key1), "map no longer has key \"name\"") && ok;

		// CPP: GC_POP_SCOPE();
		return ok;
	}

	// Main test runner for this module
	public static Boolean RunAll() {
		IOHelper.Print("=== Value Tests ===");
		TestFramework.Reset();

		Boolean allPassed = true;
		allPassed = TestNullValue() && allPassed;
		allPassed = TestIntValue() && allPassed;
		allPassed = TestNegativeInt() && allPassed;
		allPassed = TestDoubleValue() && allPassed;
		allPassed = TestStringValue() && allPassed;
		allPassed = TestEmptyString() && allPassed;
		allPassed = TestIntArithmetic() && allPassed;
		allPassed = TestIntComparison() && allPassed;
		allPassed = TestStringComparison() && allPassed;
		allPassed = TestStringConcatenation() && allPassed;
		allPassed = TestTruthiness() && allPassed;
		allPassed = TestListCreation() && allPassed;
		allPassed = TestListOperations() && allPassed;
		allPassed = TestMapCreation() && allPassed;
		allPassed = TestMapOperations() && allPassed;

		TestFramework.PrintSummary("Value");
		return TestFramework.AllPassed();
	}
}

}
