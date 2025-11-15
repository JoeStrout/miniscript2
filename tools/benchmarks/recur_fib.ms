// Recursive Fibonacci sequence.

rfib = function(n)
    if n < 2 then return n // No negatives please
    return rfib(n-1) + rfib(n-2)
end function

print "Result in r0:"
print rfib(33)