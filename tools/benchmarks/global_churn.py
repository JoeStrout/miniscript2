#!/usr/bin/env python3
"""Globals reached by name from inside function calls.  See global_churn.ms."""

# Padding: a realistic population of module-level globals, never read.
p00 = 0; p01 = 1; p02 = 2; p03 = 3; p04 = 4; p05 = 5; p06 = 6; p07 = 7
p08 = 8; p09 = 9; p10 = 10; p11 = 11; p12 = 12; p13 = 13; p14 = 14; p15 = 15
p16 = 16; p17 = 17; p18 = 18; p19 = 19; p20 = 20; p21 = 21; p22 = 22; p23 = 23
p24 = 24; p25 = 25; p26 = 26; p27 = 27; p28 = 28; p29 = 29; p30 = 30; p31 = 31

step = 7

def readLoop(n):
    acc = 0
    i = 0
    while i < n:
        acc = (acc + step) % 999983    # "step" is a global read
        i = i + 1
    return acc

keys = ["dyn" + str(i) for i in range(32)]

def seed(keys):
    g = globals()
    for k in keys:
        g[k] = 0

def churn(keys, passes):
    g = globals()
    p = 0
    while p < passes:
        for k in keys:
            g[k] = g[k] + 1
        p = p + 1

def total(keys):
    g = globals()
    t = 0
    for k in keys:
        t = t + g[k]
    return t

answer = readLoop(2000000)

seed(keys)
churn(keys, 5000)
answer = answer + total(keys)

print("Result in r0:")
print(answer)
