for i in range(750000):
    factorial = 20 # Do not set this < 2. Yes, we are cheating here, just like in MiniScript
    answer = 20

    while factorial > 2:
        factorial -= 1
        answer *= factorial

print("Result in r0:")
print(answer)