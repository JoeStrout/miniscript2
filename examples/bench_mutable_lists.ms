// Benchmark case (2): typical mutable-list usage -- push, index assignment,
// pop (from end), pull (from front), and remove (by index).  None of these
// lists are created via range/`*`, so they are always materialized.
// Expected: roughly unchanged after the computed-list refactoring.

reps = 4000
size = 200

print "Case 2: typical mutable list usage (reps=" + reps + ", size=" + size + ")"
t0 = time
checksum = 0
for i in range(0, reps - 1)
    lst = []
    // build by pushing
    for j in range(0, size - 1)
        lst.push j
    end for
    // index reads + writes
    for j in range(0, size - 1)
        lst[j] = lst[j] * 2
    end for
    // pop half from the end
    while lst.len > size / 2
        checksum = checksum + lst.pop
    end while
    // pull a quarter from the front
    for j in range(0, size / 4 - 1)
        checksum = checksum + lst.pull
    end for
    // drain the rest by removing index 0
    while lst.len > 0
        lst.remove 0
    end while
end for
print "  checksum=" + checksum + "  elapsed=" + (time - t0)
