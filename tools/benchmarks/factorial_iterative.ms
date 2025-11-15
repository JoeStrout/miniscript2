for i in range(1,750000)
    factorial = 20 // Do not set this < 2. Yes, we are cheating here and still slow.
    answer = 20

    while factorial > 2
        factorial -= 1
        answer *= factorial
    end while
end for

print "Result in r0:"
print answer
return answer