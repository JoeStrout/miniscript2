// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: StringUtils.cs

#include "StringUtils.g.h"

namespace MiniScript {

const String StringUtils::hexDigits = "0123456789ABCDEF";
String StringUtils::ToHex(UInt32 value) {
	char hexChars[9]{};
	for (Int32 i = 7; i >= 0; i--) {
		hexChars[i] = hexDigits[(int)(value & 0xF)];
		value >>= 4;
	}
	return  String::New(hexChars);
}
String StringUtils::ToHex(Byte value) {
	char hexChars[3]{};
	for (Int32 i = 1; i >= 0; i--) {
		hexChars[i] = hexDigits[(int)(value & 0xF)];
		value >>= 4;
	}
	return  String::New(hexChars);
}
String StringUtils::ZeroPad(Int32 value,Int32 digits ) {
	// set width and fill
	char format[] = "%05d";
	format[2] = '0' + digits;
	char buffer[20];
	snprintf(buffer, 20, format, value);
	return String(buffer);
}
String StringUtils::Spaces(Int32 count) {
	if (count < 1) return "";
	char* spaces = (char*)malloc(count + 1);
	memset(spaces, ' ', count);
	spaces[count] = '\0';
	String result(spaces);
	free(spaces);
	return result;
}
String StringUtils::SpacePad(String text,Int32 width) {
	// (null not possible)
	if (text.Length() >= width) return text.Substring(0, width);
	return text + Spaces(width - text.Length());
}
String StringUtils::Str(List<String> list) {
	// (null not possible)
	if (list.Count() == 0) return "[]";
	return  String::New("[\"") + String::Join("\", \"", list) + "\"]";
}
String StringUtils::FormatList(const String& fmt, const List<String>& values) {
	const int n = fmt.Length();

	List<String> parts; // collect chunks, then Join
	int i = 0;

	while (i < n) {
		const Char c = fmt[i];

		if (c == '{') {
			if (i + 1 < n && fmt[i + 1] == '{') {
				parts.Add(String("{")); i += 2; continue;
			}
			int j = i + 1;
			if (j >= n || !std::isdigit(static_cast<unsigned char>(fmt[j])))
				throw std::runtime_error("Invalid placeholder; expected {n}.");
			int num = 0;
			while (j < n && std::isdigit(static_cast<unsigned char>(fmt[j]))) {
				num = num * 10 + (fmt[j] - '0'); ++j;
			}
			if (j >= n || fmt[j] != '}')
				throw std::runtime_error("Invalid placeholder; expected closing '}'.");
			// bounds check
			if (num < 0 || num >= values.Count())
				throw std::runtime_error("Placeholder index out of range.");
			parts.Add(values[num]);
			i = j + 1;
		}
		else if (c == '}') {
			if (i + 1 < n && fmt[i + 1] == '}') {
				parts.Add(String("}")); i += 2; continue;
			}
			throw std::runtime_error("Single '}' in format string.");
		}
		else {
			// consume a run of non-brace chars as one chunk
			int start = i;
			while (i < n) {
				Char ch = fmt[i];
				if (ch == '{' || ch == '}') break;
				++i;
			}
			parts.Add(fmt.Substring(start, i - start));
		}
	}

	return String::Join(String(""), parts);  // join with empty separator
}

} // end of namespace MiniScript
