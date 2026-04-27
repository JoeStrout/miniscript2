// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: IOHelper.cs

#include "IOHelper.g.h"
#include <iostream>
#include <stdio.h>
#include <fstream>
#include <string>
#include <algorithm>

namespace MiniScript {

TextStyle IOHelper::currentStyle = TextStyle::Normal;
String IOHelper::GetStyleTermCode(TextStyle style) {
	if (style == TextStyle::Normal) {
		return "\x1b[0m";
	} else if (style == TextStyle::Subdued) {
		return "\x1b[0;90m";
	} else if (style == TextStyle::Strong) {
		return "\x1b[0;1m";
	} else if (style == TextStyle::Error) {
		return "\x1b[0;31m";
	} else {
		return "";
	}
}
void IOHelper::SetStyle(TextStyle style) {
	if (style == currentStyle) return;
	std::cout << GetStyleTermCode(style);
	currentStyle = style;
}
void IOHelper::NoteStyleSet(TextStyle style) {
	// (Used when other code has manually set the style via GetStyleTermCode)
	currentStyle = style;
}
void IOHelper::Print(String message,TextStyle style) {
	SetStyle(style);
	std::cout << message.c_str() << std::endl;
}
void IOHelper::PrintNoCR(String message,TextStyle style) {
	SetStyle(style);
	std::cout << message.c_str() << std::flush;
}
String IOHelper::Input(String prompt,TextStyle promptStyle,TextStyle inputStyle) {
	SetStyle(promptStyle);

	std::cout << prompt.c_str();
	SetStyle(inputStyle);
	char *line = NULL;
	size_t len = 0;
	
	String result;
	int bytes = getline(&line, &len, stdin);
	if (bytes != -1) {
		line[strcspn (line, "\n")] = 0;   // trim \n
		result = line;
		free(line);
	}
	return result;
}
List<String> IOHelper::ReadFile(String filePath) {
	List<String> lines =  List<String>::New();
	std::ifstream file(filePath.c_str());
	if (!file.is_open()) {
		Print(String("Error: Could not open file ") + filePath);
		return lines;
	}
	
	std::string line;
	while (std::getline(file, line)) {
		line.erase( std::remove(line.begin(), line.end(), '\r'), line.end() );
		lines.Add(String(line.c_str()));
	}
	file.close();
	return lines;
}

} // end of namespace MiniScript
