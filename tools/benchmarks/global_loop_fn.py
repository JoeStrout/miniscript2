#!/usr/bin/env python3
"""Control for global_loop.py: the identical loop, using function locals."""

def run(gN):
    gTotal = 0
    gA = 1
    gB = 2
    gI = 0
    while gI < gN:
        gTotal = (gTotal + gA + gB) % 999983
        gA = gB
        gB = gTotal
        gI = gI + 1
    return gTotal

print("Result in r0:")
print(run(10000000))
