function rfib(n)
    if n < 1 then return 0 end
    if n == 1 then return 1 end
    return rfib(n-1) + rfib(n-2)
end

print("Result in r0:")
print(rfib(33))