#!/usr/bin/env python3

for i in range(500000):
    n = 30 # fib(30)
    answer = 0

    if n < 2: continue

    a = 0
    b = 1

    for j in range(2, n+1):
        c = a + b
        a = b
        b = c

    answer = b

print("Result in r0:")
print(answer)