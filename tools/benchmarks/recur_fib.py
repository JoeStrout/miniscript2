#!/usr/bin/env python3
"""Recursive Fibonacci sequence."""

def rfib(n):
    if n < 1:
        return 0
    if n == 1:
        return 1
    return rfib(n-1) + rfib(n-2)

print("Result in r0:")
print(rfib(33))