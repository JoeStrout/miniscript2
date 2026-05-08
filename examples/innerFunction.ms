funcA = function
	funcB = function(label)
		print "Now in funcB with label: " + label
		x = results  // any access of `results` triggers the problem
		//results[label] = "OK"
		print "Returning from funcB" // (never appears)
	end function
	
	print "In funcA, calling funcB with 'foo':"
	funcB "foo"
	
	print "In funcA, calling funcB with 'bar':"
	funcB "bar"
	
	print "Done with funcA."
end function

if locals == globals then
	results = {}
	funcA
end if

	