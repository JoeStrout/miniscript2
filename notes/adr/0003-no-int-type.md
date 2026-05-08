Title: No separate `int` numeric type
Status: Accepted

Context:
With the new NaN-boxed Value type in MS2, it was easy enough to include `int` as an internal type separate from `double` (even though both reported to the MiniScript user as `number`).  It was thought that this might bring some performance advantages.  But in May 2026, we actually tested this, by running a set of benchmarks with and without the separate `int` type.  Results:

 | cpp/miniscript2_int | cpp/miniscript2 | cs/miniscript2_int | cs/miniscript2 | miniscript (1.6.2)
|---|---|---|---|---|---|
rCollatz(15000) | 0.457 | 0.405 | 3.389 | 3.422 | 15.415
dotProduct.float(100000) | 0.205 | 0.193 | 1.99 | 1.924 | 4.829
dotProduct.int(100000) | 0.203 | 0.2 | 1.949 | 1.951 | 4.828
rfib(30) | 0.374 | 0.341 | 3.273 | 3.253 | 14.104
intModulo(3000000) | 0.309 | 0.451 | 3.562 | 3.571 | 12.663
listSort.int(100000) | 0.086 | 0.06 | 0.391 | 0.403 | 0.315
listSort.float(100000) | 0.079 | 0.061 | 0.383 | 0.395 | 0.319
listSum.int(1000000) | 0.226 | 0.236 | 3.005 | 2.96 | 1.057
listSum.float(1000000) | 0.235 | 0.233 | 2.997 | 3.118 | 1.046
loopMath | 0.169 | 0.124 | 1.684 | 1.689 | 5.823
sieve(1000000) | 0.192 | 0.172 | 1.382 | 1.379 | 10.222
|---|---|---|---|---|---|
Average | 0.230 | 0.225 | 2.182 | 2.188 | 6.420

Decision: Remove the `int` type, and store all numeric values internally as `double`.


Alternatives Considered:
- Keep the `int` type internally.

Consequences:
- Simpler code (removed all "if is_int(v)" paths)
- Less opportunity for unintended behavior differences between e.g. 1 and 1.0
- Bitwise functions, etc., should still work reliably since a double can store any integer up to 2^53 without loss of precision
- More certain compatibility with MS1 behavior

