TAB = char(9)
	NBSP = char(160)

// Helper function to check if a character is a word character
globals.isWordChar = function(c)
	if c == null then return false
	return (c >= "a" and c <= "z") or (c >= "A" and c <= "Z") or 
	       (c >= "0" and c <= "9") or c == "_" or c == NBSP
end function

// Helper function to check if a substring is a single word.  Note 
// that for our purposes, a parameterized type like "List<Value>" is
// considered a single word.
isWholeWord = function(str, startPos, endPos)
	if startPos == null then startPos = 0
	if endPos == null then endPos = str.len
	// Check character before the word
	if startPos > 0 and isWordChar(str[startPos-1]) then return false
	// Check character after the word  
	if endPos < str.len and isWordChar(str[endPos]) then return false
	// Check that the content itself contains only word characters,
	// possibly including a parameterized type.
	inAngleBrackets = false
	if str.len > 1 and str[-1] == "&" then endPos -= 1
	for c in str[startPos:endPos]
		if c == "<" then
			inAngleBrackets += 1
			continue
		else if c == ">" then
			inAngleBrackets -= 1
			continue
		end if
		if isWordChar(c) then continue
		if inAngleBrackets and (c == "," or c == " ") then continue
		return false
	end for
	return true
end function


// string.match: takes a pattern string that may
// contain fields to fill in, identified by ≤key≥.  If
// the this string can be matched to that pattern, then
// it returns a map with the key-value pairs filled in
// with the corresponding parts of this string.  If it
// can't match, then returns null.
// (See string.fill for the inverse operation.)
string.match = function(pattern)
	// Start by splitting this pattern into literal and field parts.
	parts = []
	p = 0
	while p < pattern.len
		// find the end of a literal part
		nextp = pattern.indexOf("≤", p-1)
		if nextp == null then
			parts.push pattern[p:]
			break
		end if
		if nextp > p then parts.push pattern[p:nextp]
		p = nextp
		// find the end of a field part
		nextp = pattern.indexOf("≥", p)
		if p == null then
			parts[-1] = parts[-1] + pattern[p:]
			break
		end if
		parts.push pattern[p:nextp+1]
		p = nextp + 1
	end while
	
	// If we seem to parameterized types in the string, replace any
	// space therein with char(160) (non-breaking space).
	anyNBSP = false
	s = self
	gtPos = -1
	while true
		ltPos = s.indexOf("<", gtPos)
		if ltPos == null then break
		gtPos = s.indexOf(">", ltPos)
		if gtPos == null then break
		spacePos = s.indexOf(" ", ltPos)
		if spacePos != null and spacePos < gtPos then
			s = s[:spacePos] + NBSP + s[spacePos+1:]
			anyNBSP = true
		end if
	end while
	
	// Now, find each literal parts in this string,
	// and fill in the variable parts (fields) in between.
	result = {}
	p = 0
	lastStartPos = 0
	for i in parts.indexes
		part = parts[i]
		if part[0] == "≤" and part[-1] == "≥" then
			lastStartPos = p
			if i == parts.len-1 then
				fieldContent = part[1:-1]
				fieldValue = s[p:]
				// Check for :w qualifier
				if fieldContent.indexOf(":w") != null then
					if not isWholeWord(s, p, s.len) then return null
					// Remove the :w qualifier from the field name
					fieldContent = fieldContent.replace(":w", "")
				end if
				result[fieldContent] = fieldValue
			end if
			continue
		end if
		p = s.indexOf(part, p-1)
		if p == null then return null  // literal part not found
		if i == 0 and p > 0 then return null
		if i > 0 then
			fieldContent = parts[i-1][1:-1]
			fieldValue = s[lastStartPos:p]
			// Check for :w (whole word) qualifier
			if fieldContent.indexOf(":w") != null then
				if not isWholeWord(s, lastStartPos, p) then return null
				// Remove the :w qualifier from the field name
				fieldContent = fieldContent.replace(":w", "")
			end if
			result[fieldContent] = fieldValue
		end if
		lastStartPos = p
		p = p + part.len
	end for
	if anyNBSP then
		for i in result.indexes; result[i] = result[i].replace(NBSP, " "); end for
	end if
	return result
end function

// string.fill: substitute values from argMap into
// this string at places identified by ≤key≥.
// (See string.match for the inverse operation.)
// Also, replace \t with char(9).
string.fill = function(args)
	s = self.replace("\t", TAB)
	for idx in args.indexes
		s = s.replace("≤" + idx + "≥", args[idx])
	end for
	return s
end function


if locals == globals then
	assertMatch = function(s, pattern, expected)
		m = s.match(pattern)
		if m == expected then return
		print "TEST FAILURE ON: " + s + " WITH PATTERN: " + pattern
		print "Expected: " + str(expected)
		print "Actual:   " + str(m)
		exit
	end function
	
	// Test.
	assertMatch "Hello world", "≤greeting≥ ≤whom≥", 
	  {"greeting":"Hello", "whom":"world"}
	
	assertMatch "public void foo(x, y)", "≤scope:w≥ ≤type:w≥ ≤name:w≥(≤args≥)",
	  {"scope":"public", "type":"void", "name":"foo", "args":"x, y"}
	
	assertMatch "int bar = whoops()", "≤scope:w≥ ≤type:w≥ ≤name:w≥(≤args≥)", null

	assertMatch "List<UInt32> ints = new List<UInt32>()", "≤type:w≥ ≤name:w≥ = ≤expr≥",
	  { "type": "List<UInt32>", "name":"ints", "expr":"new List<UInt32>()" }

	assertMatch "Dict<Int32, String> map = new Dict<Int32, String>()", "≤type:w≥ ≤name:w≥ = ≤expr≥",
	  { "type": "Dict<Int32, String>", "name":"map", "expr":"new Dict<Int32, String>()" }

	assertMatch "public static byte OP(uint instruction) => (byte)((instruction >> 24) & 0xFF);",
	  "≤scope:w≥ static ≤rtype:w≥ ≤name:w≥(≤params≥) => ≤expr≥;",
	  {"scope":"public", "rtype":"byte", "name":"OP", "params":"uint instruction",
	   "expr":"(byte)((instruction >> 24) & 0xFF)"}
	
	assertMatch "ADD = 3,", "≤name:w≥ = ≤value:w≥,",
	   { "name":"ADD", "value":"3" }
	
	assertMatch "foo bar baz", "bar baz", null
	assertMatch "foo bar baz", "bar ≤anything≥", null
	
	print "All tests complete."
end if
