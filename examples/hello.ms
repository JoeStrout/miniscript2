greet = function(name="User")
  print "Hello, " + name + "!"
end function

greet2 = function(name="User2")
	print "Greetings and salutations, " + name + "!"
end function

print "(running hello.ms now)"
print "(locals == globals: " + (locals == globals) + ")"
