Animal = {}
Animal.name = "some animal"
Animal.speak = function(msg)
	print self.name + " says, """ + msg + """"
end function

Dog = new Animal
Dog.speak = function(msg)
	print "WOOF!"
	super.speak msg
end function


fido = new Dog
fido.speak "Hello!"
