

// Helper function to check if a character is a word character
isWordChar = function(c)
	if c == null then return false
	return (c >= "a" and c <= "z") or (c >= "A" and c <= "Z") or 
	       (c >= "0" and c <= "9") or c == "_"
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
	gotLT = false
	gotGT = false
	if str.len > 1 and str[-1] == "&" then endPos -= 1
	for c in str[startPos:endPos]
		if c == "<" and not gotLT then
			gotLT = true
			continue
		else if c == ">" and gotLT and not gotGT then
			gotGT = true
			continue
		end if
		if not isWordChar(c) then return false
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
				fieldValue = self[p:]
				// Check for :w qualifier
				if fieldContent.indexOf(":w") != null then
					if not isWholeWord(self, p, self.len) then return null
					// Remove the :w qualifier from the field name
					fieldContent = fieldContent.replace(":w", "")
				end if
				result[fieldContent] = fieldValue
			end if
			continue
		end if
		p = self.indexOf(part, p-1)
		if p == null then return null  // literal part not found
		if i > 0 then
			fieldContent = parts[i-1][1:-1]
			fieldValue = self[lastStartPos:p]
			// Check for :w (whole word) qualifier
			if fieldContent.indexOf(":w") != null then
				if not isWholeWord(self, lastStartPos, p) then return null
				// Remove the :w qualifier from the field name
				fieldContent = fieldContent.replace(":w", "")
			end if
			result[fieldContent] = fieldValue
		end if
		lastStartPos = p
		p = p + part.len
	end for
	return result
end function

// string.fill: substitute values from argMap into
// this string at places identified by ≤key≥.
// (See string.match for the inverse operation.)
string.fill = function(args)
	s = self
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
		print "Actual: " + str(m)
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

	assertMatch "public static byte OP(uint instruction) => (byte)((instruction >> 24) & 0xFF);",
	  "≤scope:w≥ static ≤rtype:w≥ ≤name:w≥(≤params≥) => ≤expr≥;",
	  {"scope":"public", "rtype":"byte", "name":"OP", "params":"uint instruction",
	   "expr":"(byte)((instruction >> 24) & 0xFF)"}
	
	assertMatch "ADD = 3,", "≤name:w≥ = ≤value:w≥,",
	   { "name":"ADD", "value":"3" }
	
	print "All tests complete."
end if
