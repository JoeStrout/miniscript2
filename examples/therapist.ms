// Therapist
// a cheezy little Eliza knock-off by Joe Strout
// with some updates by Jeff Epler
// hacked into a module and updated by Jez Higgins
// and finally translated from Python into MiniScript by Joe Strout :)

if version.hostName == "Mini Micro" then
	vt = {}
	setPromptColor = function; text.color = color.silver; end function
	setNormalColor = function; text.color = color.orange; end function
else
	import "vt"
	clear = function
		print vt.clear + vt.reset + vt.normal
	end function
	setPromptColor = function; print vt.textColor(vt.color.silver), ""; end function
	setNormalColor = function; print vt.normal, ""; end function
end if

//----------------------------------------------------------------------
//  respond: take a string, find the first pattern in gPats that
//    matches it, and return a randomly chosen response from the
//    corresponding list, with reflected text stuffed in where indicated.
//----------------------------------------------------------------------
respond = function(s)
	// Match against a lowercase copy, padded with spaces so that patterns
	// match whole words; captured text is taken from the original.
	padded = " " + s + " "
	lower = padded.lower
	
	// Remember anything that starts with "my", to bring up later.
	match = patMatch(" my (.*)", lower, padded)
	if match != null then gMemory.push reflect(cleanup(match[0]))
	
	for kv in gPats
		pat = kv[0]
		if pat[:4] != "(.*)" then pat = " " + pat
		if pat[-4:] != "(.*)" then pat = pat + " "
		match = patMatch(pat.lower, lower, padded)
		if match == null then continue
		choices = kv[1]
		
		if kv[0] == "(.*)" then
			// Nothing specific matched; maybe bring up something from earlier.
			if gMemory and rnd < 0.5 then
				choices = gMemoryResponses
				match = [gMemory.pull]
			else if s.split.len < 3 then
				// Echoing back a very short input sounds silly, so avoid that.
				choices = []
				for c in kv[1]
					if c.indexOf("%") == null then choices.push c
				end for
			end if
		end if
		
		// Choose a response at random, but not the same one as last time.
		idx = floor(rnd * choices.len)
		if gLastChoice.hasIndex(choices) and gLastChoice[choices] == idx then
			idx = (idx + 1) % choices.len
		end if
		gLastChoice[choices] = idx
		resp = choices[idx]
		
		// Stuff in reflected text where indicated.
		for i in match.indexes
			resp = resp.replace("%"+(i+1), reflect(cleanup(match[i])))
		end for
		resp = resp[0].upper + resp[1:]
		return resp
	end for
end function

//----------------------------------------------------------------------
// cleanup: trim spaces and trailing punctuation from captured text.
//----------------------------------------------------------------------
cleanup = function(s)
	while s and " ,.!?".indexOf(s[-1]) != null
		s = s[:-1]
	end while
	while s and s[0] == " "
		s = s[1:]
	end while
	return s
end function

//----------------------------------------------------------------------
// reflect: convert text from the user's point of view to the computer's,
//  using gPairReflections for two-word phrases and gReflections for
//  single words.  "You" becomes "I" at the start (where it's probably
//  the subject), and "me" elsewhere.
//----------------------------------------------------------------------
reflect = function(s)
	words = s.split
	result = []
	i = 0
	while i < words.len
		w = words[i].lower
		if i + 1 < words.len then
			pair = w + " " + words[i+1].lower
			if gPairReflections.hasIndex(pair) then
				result.push gPairReflections[pair]
				i += 2
				continue
			end if
		end if
		if w == "you" and i == 0 then
			result.push "I"
		else if gReflections.hasIndex(w) then
			result.push gReflections[w]
		else
			result.push words[i]
		end if
		i += 1
	end while
	return result.join
end function


//----------------------------------------------------------------------
// patMatch: Match a given pattern like "Hello (.*)!" against a string
//	 like "Hello world!", and return null if no match, or a list of
//	 the text that fits into the wildcards, like ["world"].  The whole
//	 string must match.  If source is given, the captured text is taken
//	 from the same positions in that string instead.
//----------------------------------------------------------------------
patMatch = function(pat, s, source=null, wildcard="(.*)")
	if source == null then source = s
	result = []
	pos = 0
	literals = pat.split(wildcard)
	for i in literals.indexes
		lit = literals[i]
		if s[pos:pos+lit.len] != lit then return null
		pos += lit.len
		if i == literals.len-1 then
			if pos == s.len then return result
			return null
		end if
		nextLit = literals[i+1]
		if i+1 == literals.len-1 then
			// the last literal must come at the very end
			nextpos = s.len - nextLit.len
			if nextpos < pos then return null
		else
			nextpos = s.indexOf(nextLit, pos-1)
			if nextpos == null then return null
		end if
		result.push source[pos:nextpos]
		pos = nextpos
	end for
end function

//----------------------------------------------------------------------
// gReflections and gPairReflections, translation tables used to convert
//    things you say into things the computer says back, e.g. "I am" -->
//    "you are".  ("Are" and "was" are only reflected next to I/you.)
//----------------------------------------------------------------------
gReflections = {
  "am"    : "are",
  "i"     : "you",
  "i'd"   : "you would",
  "i've"  : "you have",
  "i'll"  : "you will",
  "i'm"	: "you're",
  "my"    : "your",
  "mine"  : "yours",
  "myself": "yourself",
  "you've": "I have",
  "you'll": "I will",
  "you'd" : "I would",
  "your"  : "my",
  "you're": "I'm",
  "yours" : "mine",
  "yourself": "myself",
  "you"   : "me",
  "me"    : "you" }

gPairReflections = {
  "i am"    : "you are",
  "am i"    : "are you",
  "i was"   : "you were",
  "was i"   : "were you",
  "you are" : "I am",
  "are you" : "am I",
  "you were": "I was",
  "were you": "was I" }

// Things the user said starting with "my", to bring up again later;
// and the ways to bring them up.
gMemory = []
gMemoryResponses = [
  "Earlier you said your %1.",
  "Let's discuss further why your %1.",
  "Does that have anything to do with the fact that your %1?"]

// Index of the last response chosen from each response list.
gLastChoice = {}

//----------------------------------------------------------------------
// gPats, the main response table.  Each element of the list is a
//  two-element list; the first is an input pattern, and the second is a
//  list of possible responses, with group-macros labelled as
//  %1, %2, etc.
//----------------------------------------------------------------------
gPats = [
  ["quit",
  [ "Thank you for talking with me.",
  "Good-bye.",
  "Thank you, that will be $150.  Have a good day!"]],
  
["Hello(.*)",
  [ "Hello... I'm glad you could drop by today.",
  "Hi there... how are you today?",
  "Hello, how are you feeling today?"]],
  
["(.*) sorry (.*)",
  [ "There are many times when no apology is needed.",
  "What feelings do you have when you apologize?"]],
  
["(.*) mother(.*)",
  [ "Tell me more about your mother.",
  "What was your relationship with your mother like?",
  "How do you feel about your mother?",
  "How does this relate to your feelings today?",
  "Good family relations are important."]],
  
["(.*) father(.*)",
  [ "Tell me more about your father.",
  "How did your father make you feel?",
  "How do you feel about your father?",
  "Does your relationship with your father relate to your feelings today?",
  "Do you have trouble showing affection with your family?"]],
  
["(.*) child(.*)",
  [ "Did you have close friends as a child?",
  "What is your favorite childhood memory?",
  "Do you remember any dreams or nightmares from childhood?",
  "Did the other children sometimes tease you?",
  "How do you think your childhood experiences relate to your feelings today?"]],
  
["(.*) friend (.*)",
  [ "Tell me more about your friends.",
  "When you think of a friend, what comes to mind?",
  "Why don't you tell me about a childhood friend?"]],
  
["(.*) computer(.*)",
  [ "Are you really talking about me?",
  "Does it seem strange to talk to a computer?",
  "How do computers make you feel?",
  "Do you feel threatened by computers?"]],
  
["I need (.*)",
  [ "Why do you need %1?",
  "Would it really help you to get %1?",
  "Are you sure you need %1?"]],
  
["I can't (.*)",
  [ "How do you know you can't %1?",
  "Perhaps you could %1 if you tried.",
  "What would it take for you to %1?",
  "Not with THAT attitude.",
  "Are you sure you can't %1?",
  "What would you do if you could %1?",
  "Do you think others could %1?",
  "Have you tried to %1?"]],
  
["I am (.*)",
  [ "Did you come to me because you are %1?",
  "How long have you been %1?",
  "How do you feel about being %1?"]],
  
["I'm (.*)",
  [ "How does being %1 make you feel?",
  "Do you enjoy being %1?",
  "Why do you tell me you're %1?",
  "Why do you think you're %1?"]],
  
["I think (.*)",
  [ "Do you doubt %1?",
  "Do you really think so?",
  "But you're not sure %1?"]],
  
["I don't (.*)",
  [ "Don't you really %1?",
  "Why don't you %1?",
  "Do you want to %1?"]],
  
["I feel (.*)",
  [ "Good, tell me more about these feelings.",
  "Do you often feel %1?",
  "When do you usually feel %1?",
  "When you feel %1, what do you do?"]],
  
["I have (.*)",
  [ "Why do you tell me that you've %1?",
  "Have you really %1?",
  "Now that you have %1, what will you do next?"]],
  
["I would (.*)",
  [ "Could you explain why you would %1?",
  "Why would you %1?",
  "Who else knows that you would %1?"]],
  
["I want (.*)",
  [ "What would it mean to you if you got %1?",
  "Why do you want %1?",
  "What would you do if you got %1?",
  "If you got %1, then what would you do?"]],
  
["Why don't you (.*)",
  [ "Do you really think I don't %1?",
  "Perhaps eventually I will %1.",
  "Do you really want me to %1?"]],
  
["Why can't I (.*)",
  [ "Do you think you should be able to %1?",
  "If you could %1, what would you do?",
  "I don't know -- why can't you %1?",
  "Have you really tried?"]],
  
["Are you (.*)",
  [ "Why does it matter whether I am %1?",
  "Would you prefer it if I were not %1?",
  "Perhaps you believe I am %1.",
  "I may be %1 -- what do you think?"]],
  
["Because (.*)",
  [ "Is that the real reason?",
  "What other reasons come to mind?",
  "Does that reason apply to anything else?",
  "If %1, what else must be true?"]],
  
["Is it (.*)",
  [ "Do you think it is %1?",
  "Perhaps it's %1 -- what do you think?",
  "If it were %1, what would you do?",
  "It could well be that it is %1."]],
  
["It is (.*)",
  [ "You seem very certain.",
  "If I told you that it probably isn't %1, what would you feel?"]],
  
["Is there (.*)",
  [ "Do you think there is %1?",
  "It's likely that there is %1.",
  "Would you like there to be %1?"]],
  
["Can you (.*)",
  [ "What makes you think I can't %1?",
  "If I could %1, then what?",
  "Why do you ask if I can %1?"]],
  
["Can I (.*)",
  [ "Perhaps you don't want to %1.",
  "Do you want to be able to %1?",
  "If you could %1, would you?"]],
  
["Yes",
  [ "You seem quite sure.",
  "OK, but can you elaborate a bit?"]],
  
["No",
  [ "Why not?",
  "Are you saying no just to be negative?",
  "You are being a bit negative."]],
  
["You are (.*)",
  [ "Why do you think I am %1?",
  "Does it please you to think that I'm %1?",
  "Perhaps you would like me to be %1.",
  "Perhaps you're really talking about yourself?"]],
  
["You're (.*)",
  [ "Why do you say I am %1?",
  "Why do you think I am %1?",
  "Are we talking about you, or me?"]],
  
["You (.*)",
  [ "We should be discussing you, not me.",
  "Why do you say that about me?",
  "Why do you care whether I %1?"]],
  
["My (.*)",
  [ "I see, your %1.",
  "Why do you say that your %1?",
  "When your %1, how do you feel?"]],
  
["Why (.*)",
  [ "Why do you think that is?",
  "What do you think the reason might be?",
  "Why don't you tell me?" ]],
  
["What (.*)",
  [ "Why do you ask?",
  "How would an answer to that help you?",
  "What do you think?"]],
  
["How (.*)",
  [ "How do you suppose?",
  "Perhaps you can answer your own question.",
  "What is it you're really asking?"]],
  
["(.*)?",
  [ "Why do you ask that?",
  "Please consider whether you can answer your own question.",
  "Perhaps the answer lies within yourself?",
  "Why don't you tell me?"]],
  
["(.*)",
  [ "Please tell me more.",
  "Let's change focus a bit... Tell me about your family.",
  "Can you elaborate on that?",
  "Why do you say that %1?",
  "I see.",
  "Very interesting.",
  "%1.",
  "I see.  And what does that tell you?",
  "How does that make you feel?",
  "How do you feel when you say that?"]]  ]


runInteractive = function
	clear
	print "Therapist" + char(13) + "---------"
	print "Talk to the program by typing in plain English, using normal upper-"
	print "and lower-case letters and punctuation.  Enter ""quit"" when done."
	print "=" * 68
	print "Hello.  How are you feeling today?"
	
	while true
		setPromptColor
		s = input("> ")
		while s and (s[-1] == "!" or s[-1] == ".")
			s = s[:-1]
		end while
		setNormalColor
		print respond(s)
		if s.lower == "quit" then break
	end while
end function

if locals == globals then
	runInteractive
end if
