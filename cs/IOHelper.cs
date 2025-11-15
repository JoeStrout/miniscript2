// IOHelper
//	This is a simple wrapper for console output on each platform.

using System;
using System.Collections.Generic;
// CPP: #include <iostream>
// CPP: #include <stdio.h>
// CPP: #include <fstream>
// CPP: #include <string>
// CPP: #include <algorithm>

public static class IOHelper {

	public static void Print(String message) {
		Console.WriteLine(message);  // CPP: std::cout << message.c_str() << std::endl;
	}
	
	public static String Input(String prompt) {
		//*** BEGIN CS_ONLY ***
		Console.Write(prompt);
		return Console.ReadLine() ?? "";
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
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
		*** END CPP_ONLY ***/
	}
	
	public static List<String> ReadFile(String filePath) {
		List<String> lines = new List<String>();
		//*** BEGIN CS_ONLY ***
		try {
			String[] fileLines = System.IO.File.ReadAllLines(filePath);
			for (Int32 i = 0; i < fileLines.Length; i++) {
				lines.Add(fileLines[i]);
			}
		} catch (Exception e) {
			Print("Error reading file: " + e.Message);
		}
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
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
		*** END CPP_ONLY ***/
		return lines;
	}
	
}
