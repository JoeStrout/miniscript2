// Benchmark: sort a large list of numbers, comparing integer vs float values.
// Each rep sorts a fresh copy of the unsorted data to avoid already-sorted bias.

runBenchmark = function(n = 100000, reps = 5)
	data = [0] * n

	timeIt = function(label, key, op)
		print "in timeIt with label: " + label
		t0 = time
		rep = 0
		while rep < reps
			op
			rep += 1
		end while
		elapsed = round(time - t0, 3)
		print label + ": " + elapsed + " sec"
		results[key] = elapsed
	end function

	sortOp = function; copy = data[:]; copy.sort; end function

	// Integer version
	i = 0
	while i < n
		data[i] = floor(rnd * 1000000)
		i += 1
	end while
	timeIt "listSort.int   (n=" + n + ", reps=" + reps + ")", "listSort.int(" + n + ")", @sortOp
	print "1st timeIt done"

	// Float version (no floor)
	i = 0
	while i < n
		data[i] = rnd * 1000000
		i += 1
	end while
	print "calling 2nd timeIt"
	timeIt "listSort.float (n=" + n + ", reps=" + reps + ")", "listSort.float(" + n + ")", @sortOp
	print "2nd timeIt done"
end function

if locals == globals then
	results = {}
	runBenchmark
end if
