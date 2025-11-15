// AUTO-GENERATED FILE.  DO NOT MODIFY.
// Transpiled from: IOHelper.cs

// IOHelper
//	This is a simple wrapper for console output on each platform.

#include "IOHelper.g.h"


	void IOHelper::Print(String message) {
		std::cout << message.c_str() << std::endl;
	}
	
	String IOHelper::Input(String prompt) {
		std::cout << prompt.c_str();
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
		List<String> lines = List<String>();
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
	
