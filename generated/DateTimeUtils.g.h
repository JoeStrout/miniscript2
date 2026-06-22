// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: DateTimeUtils.cs

#pragma once
#include "core_includes.h"
#include "forward_decs.g.h"
// Cross-platform date/time utilities (conversion to/from string)
// for use in command-line MiniScript intrinsics.
// A timestamp is represented as a Double: seconds since the Unix epoch
// (1970-01-01 00:00:00), interpreted in local time.  We use Double rather
// than the C++ time_t so the type is valid in C# and so fractional seconds
// survive (the "f"/"F" format specifiers rely on them); the C++ side casts
// to time_t only at the libc (localtime_r / mktime) boundary.

#include "CS_String.h"
#include "value.h"
#include "value_list.h"

namespace MiniScript {

// DECLARATIONS

class DateTimeUtils {

	// Format a timestamp (seconds since the Unix epoch, local time) using a
	// .NET-style custom or standard format string.
	public: static String FormatDate(Double dateTime, String formatSpec = "yyyy-MM-dd HH:mm:ss");

	// Parse a date/time consisting of a date, time, or date and time separated
	// by one or more spaces.  Within a date, the separator is assumed to be '-'
	// and the parts are assumed to be year, year-month, or year-month-day.
	// Within a time, the separator is assumed to be ':' and the parts are assumed
	// to be hour, hour:minute, or hour:minute:second.  If there is additionally
	// a "P" or "PM" found in either the second field, or as a separate space-
	// delimited part, then we add 12 to any hour values < 12.
	// So basically this will parse a date in the format returned by default from
	// FormatDate (which is also the same as a SQL date), or simple variations thereof.
	// Returns seconds since the Unix epoch (local time).
	public: static Double ParseDate(String dateStr);

}; // end of struct DateTimeUtils

// INLINE METHODS

} // end of namespace MiniScript
