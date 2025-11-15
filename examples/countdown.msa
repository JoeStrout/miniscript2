# Countdown loop from 5 to 0
# Demonstrates labels and conditional jumps

@main:
	LOAD r0, 5       # Start with 5

loop:                # Loop label
    LOAD r1, 1       # Load 1 for decrement
    SUB r0, r0, r1   # r0 = r0 - 1
    IFLT r1, r0      # if 1 < r0 then
    JUMP loop        # Jump back to loop

end:                 # End label
    RETURN           # Return final value