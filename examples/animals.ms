Animal = {}
Animal.name = "Animal"
Animal.contents = null
Animal.speak = function(msg)
	print self.name + " says, """ + msg + """"
end function
Animal.describe = function
	print "You see a " + self.name + "."
	self.listContents "The " + self.name + " contains:"
end function

Animal.listContents = function(header, emptyMsg)
	print "self: " + self
	if not self.contents then return emptyMsg
	print header
	if self.contents isa string then
		print self.contents
	else
		print self.join(self.contents, ", ")
	end if
end  function

Dog = new Animal
Dog.name = "Dog"
Dog.speak = function(msg)
	print "WOOF!"
	super.speak msg
end function


fido = new Dog
fido.speak "Hello!"
fido.describe
