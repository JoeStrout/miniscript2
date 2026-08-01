-- Global variable access, the direct cost.  See global_loop.ms.
-- Lua is the closest comparison: its globals really are table lookups
-- through _ENV, which is the model notes/GLOBALS.md proposes.

gTotal = 0
gA = 1
gB = 2
gI = 0
gN = 10000000

while gI < gN do
    gTotal = (gTotal + gA + gB) % 999983
    gA = gB
    gB = gTotal
    gI = gI + 1
end

print("Result in r0:")
print(gTotal)
