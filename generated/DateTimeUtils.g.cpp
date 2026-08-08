// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: DateTimeUtils.cs

#include "DateTimeUtils.g.h"
#include <time.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>

namespace MiniScript {

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
// Works in bytes throughout: the patterns we match are ASCII, and byte access
// is O(1) where character access has to walk the string from the start.
static bool Match(const String& s, int *posB, const String& match) {
	int matchLenB = match.LengthB();
	if (*posB + matchLenB <= s.LengthB() && s.SubstringB(*posB, matchLenB) == match) {
		*posB += matchLenB;
		return true;
	}
	return false;
}

// Byte length of the UTF-8 sequence beginning at byte offset posB of s.
static int Utf8SeqLenB(const String& s, int posB) {
	unsigned char b = (unsigned char)s.AtB(posB);
	if (b < 0x80) return 1;
	if ((b & 0xE0) == 0xC0) return 2;
	if ((b & 0xF0) == 0xE0) return 3;
	if ((b & 0xF8) == 0xF0) return 4;
	return 1;	// (invalid lead byte; just step over it)
}
String DateTimeUtils::FormatDate(Double dateTime,String formatSpec ) {
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
	int lenB = formatSpec.LengthB();
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
		} else if (formatSpec.AtB(posB) == '"') {
			// Find the closing quote, and output the contained string literal
			int endPosB = posB + 1;
			while (endPosB < lenB) {
				char c = formatSpec.AtB(endPosB);
				if (c == '\\') endPosB++;
				else if (c == '"') break;
				else endPosB++;
			}
			result.Add(formatSpec.SubstringB(posB + 1, endPosB - posB - 1));
			posB = endPosB + 1;
		} else if (formatSpec.AtB(posB) == '\'') {
			// Find the closing quote, and output the contained string literal
			int endPosB = posB + 1;
			while (endPosB < lenB) {
				char c = formatSpec.AtB(endPosB);
				if (c == '\\') endPosB++;
				else if (c == '\'') break;
				else endPosB++;
			}
			result.Add(formatSpec.SubstringB(posB + 1, endPosB - posB - 1));
			posB = endPosB + 1;
		} else if (Match(formatSpec, &posB, "\\") && posB < lenB) {
			int seqLenB = Utf8SeqLenB(formatSpec, posB);	// (pass one whole character through)
			result.Add(formatSpec.SubstringB(posB, seqLenB));
			posB += seqLenB;
		} else {
			int seqLenB = Utf8SeqLenB(formatSpec, posB);
			result.Add(formatSpec.SubstringB(posB, seqLenB));
			posB += seqLenB;
		}
	}

	return String::Join(String(""), result);
}
Double DateTimeUtils::ParseDate(String dateStr) {
	Double result;
	TryParseDate(dateStr, &result);
	return result;
}
Boolean DateTimeUtils::TryParseDate(String dateStr,Double* result) {
	bool gotDate = false;
	bool gotTime = false;
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
			gotTime = true;
		} else {
			// An AM/PM marker only modifies a time; it is not, on its own,
			// enough to count the string as a recognizable date or time
			// (otherwise a stray word like the "a" in "not a date" would
			// look like an AM marker).
			part = part.ToUpper();
			if (part == "P" || part == "PM") pmTime = true;
		}
	}
	if (!gotDate && !gotTime) {
		// Nothing recognizable was found in the string.
		*result = 0;
		return false;
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
	*result = (double)mktime(&dateTime);
	return true;
}
Boolean DateTimeUtils::TryFormatDate(Double dateTime,String formatSpec,String* result) {
	// strftime is lenient: an unrecognized specifier yields odd output but
	// never fails, so on this side formatting always "succeeds".
	*result = FormatDate(dateTime, formatSpec);
	return true;
}

} // end of namespace MiniScript
