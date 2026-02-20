// Recursive Fibonnaci function

fib = function(n)
	if n < 2 then return n
	return fib(n-1) + fib(n-2)
end function

print fib(6)
