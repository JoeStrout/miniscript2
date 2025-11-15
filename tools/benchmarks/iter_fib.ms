for i in range (1, 500000)
    n = 30 // fib(30)
    answer = 0

    if n < 2 then continue

    a = 0
    b = 1

    for j in range(2, n)
        c = a + b
        a = b
        b = c
    end for
    answer = b
end for

print "Result in r0:"
print answer