for i = 1, 750000, 1 do
    factorial = 20 -- Do not set this < 2. Yes, we are cheating here, just like in MiniScript
    answer = 20

    while factorial > 2 do
        factorial = factorial - 1
        answer = answer * factorial
    end
end

print("Result in r0:")
print(answer)