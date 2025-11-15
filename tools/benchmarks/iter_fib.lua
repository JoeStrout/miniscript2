for i = 1, 500000, 1 do
    n = 30 -- fib(30)
    answer = 0

    if n >= 2 then
        a = 0
        b = 1

        for j = 2, n, 1 do
            c = a + b
            a = b
            b = c
        end
        answer = b
    end 
end
print("Result in r0:")
print(answer)