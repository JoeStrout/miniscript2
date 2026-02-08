// Test if statements

x = 5

// Single-line if
if x > 3 then print "x > 3"

// Single-line if/else
if x > 10 then print "big" else print "small"

// Block if
if x == 5 then
    print "x is five"
end if

// Block if/else
if x < 0 then
    print "negative"
else
    print "non-negative"
end if

// Block if/else if/else
if x < 0 then
    print "negative"
else if x == 0 then
    print "zero"
else if x < 10 then
    print "small positive"
else
    print "large positive"
end if
