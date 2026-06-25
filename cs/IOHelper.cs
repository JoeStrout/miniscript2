// IOHelper
//	This is a simple wrapper for console output on each platform.

using System;
using System.Collections.Generic;
// CPP: #include <iostream>
// CPP: #include <stdio.h>
// CPP: #include <fstream>
// CPP: #include <string>
// CPP: #include <algorithm>
// CPP: #include "keyboard.h"

/*** BEGIN CPP_ONLY ***
#ifdef _WIN32 // define POSIX getline if on Windows
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstdlib>

int getline(char** lineptr, size_t* n, FILE* stream) {
    if (!lineptr || !n || !stream) return -1;

    size_t pos = 0;

    // Allocate if needed
    if (*lineptr == nullptr || *n == 0) {
        *n = 128;
        *lineptr = (char*)malloc(*n);
        if (!*lineptr) return -1;
    }

    int c;
    while ((c = fgetc(stream)) != EOF) {
        // Grow buffer if needed
        if (pos + 1 >= *n) {
            size_t new_size = *n * 2;
            char* new_ptr = (char*)realloc(*lineptr, new_size);
            if (!new_ptr) return -1;
            *lineptr = new_ptr;
            *n = new_size;
        }

        (*lineptr)[pos++] = (char)c;

        if (c == '\n') break;
    }

    if (pos == 0 && c == EOF) {
        return -1; // EOF with no data
    }

    (*lineptr)[pos] = '\0';
    return (int)pos;
}
#endif // defined(_WIN32)
*** END CPP_ONLY ***/

namespace MiniScript {

public enum TextStyle : Int32 {
	Normal,
	Subdued,
	Strong,
	Error
}

public static class IOHelper {

	private static TextStyle currentStyle = TextStyle.Normal;
	private static bool ansiInitialized = false;
	private static bool ansiEnabled = true; // CPP: static bool ansiEnabled = false;

	private static void EnsureAnsiEnabled() {
		if (ansiInitialized) return;
		ansiInitialized = true;
		//*** BEGIN CS_ONLY ***
		// Nothing needed; .NET enables ANSI on supported platforms automatically.
		//*** END CS_ONLY ***
		/*** BEGIN CPP_ONLY ***
		#ifdef _WIN32
		HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
		if (hOut != INVALID_HANDLE_VALUE) {
		    DWORD mode = 0;
		    if (GetConsoleMode(hOut, &mode)) {
		        ansiEnabled = SetConsoleMode(hOut, mode | ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
		    }
		}
		#else
		ansiEnabled = true;
		#endif
		*** END CPP_ONLY ***/
	}

	public static String GetStyleTermCode(TextStyle style) {
		EnsureAnsiEnabled();
		if (!ansiEnabled) return "";
		if (style == TextStyle.Normal) {
			return "\x1b[0m";
		} else if (style == TextStyle.Subdued) {
			return "\x1b[0;90m";
		} else if (style == TextStyle.Strong) {
			return "\x1b[0;1m";
		} else if (style == TextStyle.Error) {
			return "\x1b[0;31m";
		} else {
			return "";
		}
	}

	public static void SetStyle(TextStyle style) {
		if (style == currentStyle) return;
		Console.Write(GetStyleTermCode(style)); // CPP: std::cout << GetStyleTermCode(style);
		currentStyle = style;
	}

	public static void NoteStyleSet(TextStyle style) {
		// (Used when other code has manually set the style via GetStyleTermCode)
		currentStyle = style;
	}

	public static void Print(String message, TextStyle style=TextStyle.Normal) {
		SetStyle(style);
		Console.WriteLine(message);  // CPP: std::cout << message.c_str() << std::endl;
	}
	
	public static void PrintNoCR(String message, TextStyle style=TextStyle.Normal) {
		SetStyle(style);
		Console.Write(message);  // CPP: std::cout << message.c_str() << std::flush;
	}
	
	public static String Input(String prompt, TextStyle promptStyle=TextStyle.Normal, TextStyle inputStyle=TextStyle.Normal) {
		SetStyle(promptStyle);

		//*** BEGIN CS_ONLY ***
		Console.Write(prompt);
		SetStyle(inputStyle);
		return Console.ReadLine();
		//*** END CS_ONLY ***

		/*** BEGIN CPP_ONLY ***
		std::cout << prompt.c_str();
		SetStyle(inputStyle);
		// If the `key` module has put the terminal in raw (cbreak) mode, drop
		// back to cooked mode so the user gets echo and line editing here. We
		// leave it cooked afterward (last-writer-wins); the next key.get/
		// key.available re-enters raw mode.
		Keyboard::EnterCookedMode();
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

}
