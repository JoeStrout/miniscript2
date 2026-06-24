// Benchmark case (1): repeated creation of large lists via `range` and `*`.
// This is the pattern computed lists are meant to accelerate: we build big
// lists over and over but only lightly consume them (length + a few reads).
// Expected: substantially faster once range/`*` produce computed lists.

reps = 4000
size = 5000

print "Case 1: repeated large-list creation (reps=" + reps + ", size=" + size + ")"
t0 = time
total = 0
for i in range(0, reps - 1)
    a = range(0, size)        // would allocate size+1 elements each time
    b = [0] * size            // would allocate size elements each time
    total = total + a.len + b.len + a[size / 2] + b[0]
end for
print "  total=" + total + "  elapsed=" + (time - t0)
