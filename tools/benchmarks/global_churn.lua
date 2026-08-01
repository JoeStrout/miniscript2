-- Globals reached by name from inside function calls.  See global_churn.ms.

-- Padding: a realistic population of globals, never read.
p00 = 0; p01 = 1; p02 = 2; p03 = 3; p04 = 4; p05 = 5; p06 = 6; p07 = 7
p08 = 8; p09 = 9; p10 = 10; p11 = 11; p12 = 12; p13 = 13; p14 = 14; p15 = 15
p16 = 16; p17 = 17; p18 = 18; p19 = 19; p20 = 20; p21 = 21; p22 = 22; p23 = 23
p24 = 24; p25 = 25; p26 = 26; p27 = 27; p28 = 28; p29 = 29; p30 = 30; p31 = 31

step = 7

function readLoop(n)
    local acc = 0
    local i = 0
    while i < n do
        acc = (acc + step) % 999983    -- "step" is a global read
        i = i + 1
    end
    return acc
end

keys = {}
for i = 0, 31 do
    keys[#keys + 1] = "dyn" .. i
end

function seed(keys)
    for _, k in ipairs(keys) do
        _G[k] = 0
    end
end

function churn(keys, passes)
    local p = 0
    while p < passes do
        for _, k in ipairs(keys) do
            _G[k] = _G[k] + 1
        end
        p = p + 1
    end
end

function total(keys)
    local t = 0
    for _, k in ipairs(keys) do
        t = t + _G[k]
    end
    return t
end

answer = readLoop(2000000)

seed(keys)
churn(keys, 5000)
answer = answer + total(keys)

print("Result in r0:")
print(answer)
