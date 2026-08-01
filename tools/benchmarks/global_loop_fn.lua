-- Control for global_loop.lua: the identical loop, using function locals.

local function run(gN)
    local gTotal = 0
    local gA = 1
    local gB = 2
    local gI = 0
    while gI < gN do
        gTotal = (gTotal + gA + gB) % 999983
        gA = gB
        gB = gTotal
        gI = gI + 1
    end
    return gTotal
end

print("Result in r0:")
print(run(10000000))
