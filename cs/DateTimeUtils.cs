// Cross-platform date/time utilities (conversion to/from string)
// for use in command-line MiniScript intrinsics.
//
// A timestamp is represented as a Double: seconds since the Unix epoch
// (1970-01-01 00:00:00), interpreted in local time.  We use Double rather
// than the C++ time_t so the type is valid in C# and so fractional seconds
// survive (the "f"/"F" format specifiers rely on them); the C++ side casts
// to time_t only at the libc (localtime_r / mktime) boundary.

using System;
using System.Globalization;
// H: #include "CS_String.h"
// H: #include "value.h"
// H: #include "value_list.h"
// CPP: #include <time.h>
// CPP: #include <math.h>
// CPP: #include <string.h>
// CPP: #include <stdio.h>
// CPP: #include <stdlib.h>
// CPP: #include <locale.h>

namespace MiniScript {

public static class DateTimeUtils {

	/*** BEGIN CPP_ONLY ***
	#if _WIN32 || _WIN64
	// Windows lacks localtime_r; provide a shim over the thread-unsafe call.
	// (If the transpiler emits this inside namespace MiniScript, it may need
	// to be hoisted to file scope.)
	static struct tm *localtime_r(const time_t *timer, struct tm *buf) {
		struct tm *newtime = _localtime64(timer);
		if (newtime == nullptr) return nullptr;
		*buf = *newtime;
		return buf;
	}
	#endif

	// Returns true (and advances *posB) if the substring of s at *posB equals match.
	static bool Match(const String& s, int *posB, const String& match) {
		int matchLen = match.Length();
		if (*posB + matchLen <= s.Length() && s.Substring(*posB, matchLen) == match) {
			*posB += matchLen;
			return true;
		}
		return false;
	}
	*** END CPP_ONLY ***/

	// Format a timestamp (seconds since the Unix epoch, local time) using a
	// .NET-style custom or standard format string.
	public static String FormatDate(Double dateTime, String formatSpec = "yyyy-MM-dd HH:mm:ss") {
		//*** BEGIN CS_ONLY ***
		DateTime dt = DateTime.UnixEpoch.AddSeconds(dateTime).ToLocalTime();
		if (String.IsNullOrEmpty(formatSpec)) return dt.ToString(CultureInfo.InvariantCulture);
		// .NET natively understands both the standard single-letter specifiers
		// (d, D, f, F, g, G, M, O, R, s, t, T, u, U, Y) and custom specifiers
		// (yyyy, MM, dd, HH, mm, ss, fff, tt, ...), including the leading '%'
		// used to force a single custom specifier.  Use the invariant culture
		// so month/day names and patterns are deterministic across platforms.
		return dt.ToString(formatSpec, CultureInfo.InvariantCulture);
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		time_t t = (time_t)dateTime;
		tm dt;
		struct tm *newtime = localtime_r(&t, &dt);
		if (newtime == nullptr) return String("");  // arg dateTime too large

		const int BUFSIZE = 128;
		char buffer[BUFSIZE];
		setlocale(LC_ALL, "");

		// First, check for special cases -- i.e. standard date formatters.
		// (Unless the first character is %, in which case skip that and
		// take the rest as an individual part, below.)
		// Note that "d", "g", and "G" are culture- and platform-specific,
		// and will not come out the same on different platforms.
		// All others have been implemented in standard ways assuming
		// an invariant culture (which may not match user expectations).
		if (formatSpec.Empty()) {
			strftime(buffer, BUFSIZE, "%c", &dt);
			return String(buffer);
		}
		int posB = 0;
		if (formatSpec[0] == '%') {
			posB = 1;
		} else if (formatSpec == "d") {	// short date
			// culture- and platform-specific
			strftime(buffer, BUFSIZE, "%x", &dt);
			return String(buffer);
		} else if (formatSpec == "D") { // long date
			formatSpec = "dddd, dd MMMM yyyy";
		} else if (formatSpec == "f") { // full date/time (short time)
			formatSpec = "dddd, dd MMMM yyyy HH:mm";
		} else if (formatSpec == "F") { // full date/time (long time)
			formatSpec = "dddd, dd MMMM yyyy HH:mm:ss";
		} else if (formatSpec == "g") { // general date/time (short time)
			// culture- and platform-specific
			strftime(buffer, BUFSIZE, "%x ", &dt);
			String result = String(buffer);
			snprintf(buffer, BUFSIZE, "%d:", dt.tm_hour);
			result = result + String(buffer);
			strftime(buffer, BUFSIZE, "%H %p", &dt);
			return result + String(buffer);
		} else if (formatSpec == "G") { // general date/time (long time)
			// culture- and platform-specific
			strftime(buffer, BUFSIZE, "%x %X", &dt);
			return String(buffer);
		} else if (formatSpec == "M" || formatSpec == "m") {	// month/day
			formatSpec = "d MMMM";
		} else if (formatSpec == "O" || formatSpec == "o") {	// round-trip (ISO) format
			formatSpec = "yyyy'-'MM'-'dd'T'HH':'mm':'ss'.'fffffffK";
		} else if (formatSpec == "R" || formatSpec == "r") {	// RFC1123 format
			formatSpec = "ddd, dd MMM yyyy HH':'mm':'ss 'GMT'";
		} else if (formatSpec == "s") {	// sortable
			formatSpec = "yyyy'-'MM'-'dd'T'HH':'mm':'ss";
		} else if (formatSpec == "t") { // short time
			formatSpec = "HH:mm";
		} else if (formatSpec == "T") { // long time
			formatSpec = "HH:mm:ss";
		} else if (formatSpec == "u") { // universal sortable
			formatSpec = "yyyy'-'MM'-'dd HH':'mm':'ss'Z'";
		} else if (formatSpec == "U") { // universal full date/time
			formatSpec = "dddd, dd MMMM yyyy HH:mm:ss";
		} else if (formatSpec == "Y" || formatSpec == "y") {	// year-month
			formatSpec = "yyyy MMMM";
		} else if (formatSpec.Length() == 1) {
			// invalid
			return String("");
		}

		// Otherwise, parse individual parts.
		List<String> result;
		int lenB = formatSpec.Length();
		while (posB < lenB) {
			if (Match(formatSpec, &posB, "yyyy")) {			// year
				snprintf(buffer, BUFSIZE, "%04d", dt.tm_year + 1900);
				result.Add(String(buffer));
			} else if (Match(formatSpec, &posB, "yyy")) {
				snprintf(buffer, BUFSIZE, "%03d", dt.tm_year + 1900);
				result.Add(String(buffer));
			} else if (Match(formatSpec, &posB, "yy")) {
				snprintf(buffer, BUFSIZE, "%02d", dt.tm_year % 100);
				result.Add(String(buffer));
			} else if (Match(formatSpec, &posB, "MMMM")) {	// month name
				strftime(buffer, BUFSIZE, "%B", &dt);
				result.Add(String(buffer));
			} else if (Match(formatSpec, &posB, "MMM")) {	// month abbreviation
				strftime(buffer, BUFSIZE, "%b", &dt);
				result.Add(String(buffer));
			} else if (Match(formatSpec, &posB, "MM")) {	// month number
				snprintf(buffer, BUFSIZE, "%02d", dt.tm_mon + 1);
				result.Add(String(buffer));
			} else if (Match(formatSpec, &posB, "M")) {
				snprintf(buffer, BUFSIZE, "%d", dt.tm_mon + 1);
				result.Add(String(buffer));
			} else if (Match(formatSpec, &posB, "dddd")) {	// weekday name
				strftime(buffer, BUFSIZE, "%A", &dt);
				result.Add(String(buffer));
			} else if (Match(formatSpec, &posB, "ddd")) {	// weekday abbreviation
				strftime(buffer, BUFSIZE, "%a", &dt);
				result.Add(String(buffer));
			} else if (Match(formatSpec, &posB, "dd")) {	// day (number)
				snprintf(buffer, BUFSIZE, "%02d", dt.tm_mday);
				result.Add(String(buffer));
			} else if (Match(formatSpec, &posB, "d")) {
				snprintf(buffer, BUFSIZE, "%d", dt.tm_mday);
				result.Add(String(buffer));
			} else if (Match(formatSpec, &posB, "hh")) {	// 12-hour hour
				int twelveHourHour = (dt.tm_hour == 0 ? 12 : (dt.tm_hour - 1) % 12 + 1);
				snprintf(buffer, BUFSIZE, "%02d", twelveHourHour);
				result.Add(String(buffer));
			} else if (Match(formatSpec, &posB, "h")) {
				int twelveHourHour = (dt.tm_hour == 0 ? 12 : (dt.tm_hour - 1) % 12 + 1);
				snprintf(buffer, BUFSIZE, "%d", twelveHourHour);
				result.Add(String(buffer));
			} else if (Match(formatSpec, &posB, "HH")) {	// 24-hour hour
				snprintf(buffer, BUFSIZE, "%02d", dt.tm_hour);
				result.Add(String(buffer));
			} else if (Match(formatSpec, &posB, "H")) {
				snprintf(buffer, BUFSIZE, "%d", dt.tm_hour);
				result.Add(String(buffer));
			} else if (Match(formatSpec, &posB, "mm")) {	// minute
				snprintf(buffer, BUFSIZE, "%02d", dt.tm_min);
				result.Add(String(buffer));
			} else if (Match(formatSpec, &posB, "m")) {
				snprintf(buffer, BUFSIZE, "%d", dt.tm_min);
				result.Add(String(buffer));
			} else if (Match(formatSpec, &posB, "ss")) {	// second
				snprintf(buffer, BUFSIZE, "%02d", dt.tm_sec);
				result.Add(String(buffer));
			} else if (Match(formatSpec, &posB, "s")) {
				snprintf(buffer, BUFSIZE, "%d", dt.tm_sec);
				result.Add(String(buffer));
			} else if (Match(formatSpec, &posB, "ffffff")) {	// fractional part of seconds value
				double d, i;
				d = modf(dateTime, &i);
				snprintf(buffer, BUFSIZE, "%.6f", d);
				result.Add(String(&buffer[2]));	// (skipping past "0.")
			} else if (Match(formatSpec, &posB, "fffff")) {
				double d, i;
				d = modf(dateTime, &i);
				snprintf(buffer, BUFSIZE, "%.5f", d);
				result.Add(String(&buffer[2]));	// (skipping past "0.")
			} else if (Match(formatSpec, &posB, "ffff")) {
				double d, i;
				d = modf(dateTime, &i);
				snprintf(buffer, BUFSIZE, "%.4f", d);
				result.Add(String(&buffer[2]));	// (skipping past "0.")
			} else if (Match(formatSpec, &posB, "fff")) {
				double d, i;
				d = modf(dateTime, &i);
				snprintf(buffer, BUFSIZE, "%.3f", d);
				result.Add(String(&buffer[2]));	// (skipping past "0.")
			} else if (Match(formatSpec, &posB, "ff")) {
				double d, i;
				d = modf(dateTime, &i);
				snprintf(buffer, BUFSIZE, "%.2f", d);
				result.Add(String(&buffer[2]));	// (skipping past "0.")
			} else if (Match(formatSpec, &posB, "f")) {
				double d, i;
				d = modf(dateTime, &i);
				snprintf(buffer, BUFSIZE, "%.1f", d);
				result.Add(String(&buffer[2]));	// (skipping past "0.")
			} else if (Match(formatSpec, &posB, "FFFFFF")) {	// fractional part of seconds value, if nonzero
				double d, i;
				d = modf(dateTime, &i);
				snprintf(buffer, BUFSIZE, "%.6f", d);
				String s(&buffer[2]);	// (skipping past "0.")
				if (s != "000000") result.Add(s);
			} else if (Match(formatSpec, &posB, "FFFFF")) {
				double d, i;
				d = modf(dateTime, &i);
				snprintf(buffer, BUFSIZE, "%.5f", d);
				String s(&buffer[2]);	// (skipping past "0.")
				if (s != "00000") result.Add(s);
			} else if (Match(formatSpec, &posB, "FFFF")) {
				double d, i;
				d = modf(dateTime, &i);
				snprintf(buffer, BUFSIZE, "%.4f", d);
				String s(&buffer[2]);	// (skipping past "0.")
				if (s != "0000") result.Add(s);
			} else if (Match(formatSpec, &posB, "FFF")) {
				double d, i;
				d = modf(dateTime, &i);
				snprintf(buffer, BUFSIZE, "%.3f", d);
				String s(&buffer[2]);	// (skipping past "0.")
				if (s != "000") result.Add(s);
			} else if (Match(formatSpec, &posB, "FF")) {
				double d, i;
				d = modf(dateTime, &i);
				snprintf(buffer, BUFSIZE, "%.2f", d);
				String s(&buffer[2]);	// (skipping past "0.")
				if (s != "00") result.Add(s);
			} else if (Match(formatSpec, &posB, "F")) {
				double d, i;
				d = modf(dateTime, &i);
				snprintf(buffer, BUFSIZE, "%.1f", d);
				String s(&buffer[2]);	// (skipping past "0.")
				if (s != "0") result.Add(s);
			} else if (Match(formatSpec, &posB, "tt")) {	// AM/PM
				strftime(buffer, BUFSIZE, "%p", &dt);
				result.Add(String(buffer));
			} else if (Match(formatSpec, &posB, "t")) {
				strftime(buffer, BUFSIZE, "%p", &dt);
				buffer[1] = 0;	// (truncate to length 1)
				result.Add(String(buffer));
			} else if (Match(formatSpec, &posB, "gg") || Match(formatSpec, &posB, "g")) {
				if (dt.tm_year + 1900 > 0) result.Add(String("A.D."));
				else result.Add(String("B.C."));
			} else if (Match(formatSpec, &posB, ":")) {
				// Ugh.  Getting these in C is hard.  For now we're going to punt.
				result.Add(String(":"));
			} else if (Match(formatSpec, &posB, "/")) {
				// See comment above.
				result.Add(String("/"));
			} else if (formatSpec[posB] == '"') {
				// Find the closing quote, and output the contained string literal
				int endPosB = posB + 1;
				while (endPosB < lenB) {
					Char c = formatSpec[endPosB];
					if (c == '\\') endPosB++;
					else if (c == '"') break;
					else endPosB++;
				}
				result.Add(formatSpec.Substring(posB + 1, endPosB - posB - 1));
				posB = endPosB + 1;
			} else if (formatSpec[posB] == '\'') {
				// Find the closing quote, and output the contained string literal
				int endPosB = posB + 1;
				while (endPosB < lenB) {
					Char c = formatSpec[endPosB];
					if (c == '\\') endPosB++;
					else if (c == '\'') break;
					else endPosB++;
				}
				result.Add(formatSpec.Substring(posB + 1, endPosB - posB - 1));
				posB = endPosB + 1;
			} else if (Match(formatSpec, &posB, "\\") && posB < lenB) {
				result.Add(formatSpec.Substring(posB, 1));
				posB++;
			} else {
				result.Add(formatSpec.Substring(posB, 1));
				posB++;
			}
		}

		return String::Join(String(""), result);
		*** END CPP_ONLY ***/
	}

	// Parse a date/time consisting of a date, time, or date and time separated
	// by one or more spaces.  Within a date, the separator is assumed to be '-'
	// and the parts are assumed to be year, year-month, or year-month-day.
	// Within a time, the separator is assumed to be ':' and the parts are assumed
	// to be hour, hour:minute, or hour:minute:second.  If there is additionally
	// a "P" or "PM" found in either the second field, or as a separate space-
	// delimited part, then we add 12 to any hour values < 12.
	//
	// So basically this will parse a date in the format returned by default from
	// FormatDate (which is also the same as a SQL date), or simple variations thereof.
	// Returns seconds since the Unix epoch (local time).
	public static Double ParseDate(String dateStr) {
		//*** BEGIN CS_ONLY ***
		DateTime dt;
		// .NET's parser handles "yyyy-MM-dd HH:mm:ss" and its sub-forms (date
		// only, time only, with AM/PM), filling in today's date when only a
		// time is given -- matching the reference behavior.  AssumeLocal makes
		// an unspecified-kind result be treated as local time, like mktime.
		if (!DateTime.TryParse(dateStr, CultureInfo.InvariantCulture,
				DateTimeStyles.AssumeLocal, out dt)) {
			return 0;
		}
		return (dt.ToUniversalTime() - DateTime.UnixEpoch).TotalSeconds;
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		bool gotDate = false;
		bool pmTime = false;
		tm dateTime;
		memset(&dateTime, 0, sizeof(tm));

		std::vector<String> parts = dateStr.Split(' ');
		for (size_t i = 0; i < parts.size(); i++) {
			String part = parts[i];
			if (part.Empty()) continue;
			if (part.Contains("-")) {
				// Parse a date
				std::vector<String> fields = part.Split('-');
				dateTime.tm_year = atoi(fields[0].c_str()) - 1900;
				if (fields.size() > 1) dateTime.tm_mon = atoi(fields[1].c_str()) - 1;
				if (fields.size() > 2) dateTime.tm_mday = atoi(fields[2].c_str());
				gotDate = true;
			} else if (part.Contains(":")) {
				// Parse a time
				std::vector<String> fields = part.Split(':');
				dateTime.tm_hour = atoi(fields[0].c_str());
				if (fields.size() > 1) dateTime.tm_min = atoi(fields[1].c_str());
				if (fields.size() > 2) dateTime.tm_sec = (int)atof(fields[2].c_str());
			} else {
				part = part.ToUpper();
				if (part == "P" || part == "PM") pmTime = true;
			}
		}
		if (pmTime && dateTime.tm_hour < 12) dateTime.tm_hour += 12;
		if (!gotDate) {
			// If no date is supplied, assume the current date
			tm now;
			time_t t;
			time(&t);
			localtime_r(&t, &now);
			dateTime.tm_year = now.tm_year;
			dateTime.tm_mon = now.tm_mon;
			dateTime.tm_mday = now.tm_mday;
		}
		dateTime.tm_isdst = -1;
		return (double)mktime(&dateTime);
		*** END CPP_ONLY ***/
	}

}

}
