#!/usr/bin/env python3
"""Global variable access, the direct cost.  See global_loop.ms."""

gTotal = 0
gA = 1
gB = 2
gI = 0
gN = 10000000

while gI < gN:
    gTotal = (gTotal + gA + gB) % 999983
    gA = gB
    gB = gTotal
    gI = gI + 1

print("Result in r0:")
print(gTotal)
