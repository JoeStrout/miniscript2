// Math Utilities.

// NOTE: Some of these functions are just wrappers for hidden intrinsic
// methods (methods that start with an underscore).  You should call
// those via mathUtil anyway, rather than calling them directly.  The
// hidden intrinsics might change, but we'll update this mathUtil module
// so your code doesn't break.

// e: a mathematical constant with some interesting properties.  See:
// https://en.wikipedia.org/wiki/E_(mathematical_constant)
e = 2.7182818284

// ln: natural logarithm (i.e., log base e)
ln = function(x)
	return log(x, 2.7182818284)
end function

// radToDeg: convert radians to degrees
radToDeg = function(radians)
	return radians * 180 / pi
end function

// degToRad: convert degrees to radians
degToRad = function(degrees)
	return degrees * pi / 180
end function

// moveTowards: shift a value towards another value, but no more than a 
// certain amount.  This version works on a number and returns a number.
moveTowards = function(num, targetNum, maxChange=1)
	if abs(targetNum - num) <= maxChange then return targetNum
	if targetNum > num then return num + maxChange
	return num - maxChange
end function

// moveTowardsXY: shift a map containing "x" and "y" values towards a 
// target map that contains the same, but moving no more than the given
// distance.  This is handy, for example, to make a sprite move towards
// another sprite or the mouse.  Note that this version changes the map
// passed in the first parameter, and returns `true` if any change was
// made, or `false` if the mover was already at the target.
moveTowardsXY = function(mover, target, maxDist=1)
	dx = target.x - mover.x
	dy = target.y - mover.y
	if dx == 0 and dy == 0 then return false  // already there
	dist = sqrt(dx^2 + dy^2)
	if dist < maxDist then
		mover.x = target.x
		mover.y = target.y
	else
		f = maxDist / dist
		mover.x = mover.x + dx * f
		mover.y = mover.y + dy * f
	end if
	return true   // moved, at least a little
end function

// distance: Calculate the distance between two points.  Each point may be
// either a map containing "x" and "y" (such as mouse or a Sprite), or it
// can be a 2-element [x,y] list.
distance = function(p1, p2)
	if p1 isa map then
		x1 = p1.x
		y1 = p1.y
	else
		x1 = p1[0]
		y1 = p1[1]
	end if
	if p2 isa map then
		x2 = p2.x
		y2 = p2.y
	else
		x2 = p2[0]
		y2 = p2[1]
	end if	
	return sqrt((x1-x2)^2 + (y1-y2)^2)
end function

// lerp: Interpolate between two values, that is, return a value that is
// some portion (t) of the way from a to b.
lerp = function(a, b, t)
	return a + (b-a) * t
end function

// lerp2d: Interpolate between two points, that is, return a point that is
// some portion (t) of the way from p1 to p2.  p1 and p2 may be either a map
// containing "x" and "y", or it may be an [x,y] list, but the result will
// always be in the form of an [x,y] list.
lerp2d = function(p1, p2, t)
	if p1 isa map then
		x1 = p1.x
		y1 = p1.y
	else
		x1 = p1[0]
		y1 = p1[1]
	end if
	if p2 isa map then
		x2 = p2.x
		y2 = p2.y
	else
		x2 = p2[0]
		y2 = p2[1]
	end if	
	return [x1 + (x2-x1) * t, y1 + (y2-y1) * t]
end function

if intrinsics.hasIndex("_proportionAlongLine") then
	
	// proportionAlongLine: calculate a value that indicates how far along the
	// line from endA to endB, is the nearest point on the line to p.  This will
	// be 0 if the p is closest to point endA, and 1 if it's closest to endB.
	// It will be something between 0 and 1 if p is between the two ends, and it
	// will be < 0 or > 1 if p is outside the range of endA to endB.
	//
	// Parameters:
	//  endA: one point on the line (map or list)
	//  endB: another point on the line (map or list)
	//  p: point of interest (map or list)
	// Returns: a number indicating how far p is from endA to endB.
	proportionAlongLine = @_proportionAlongLine
	
	// nearestPointOnLine: calculate the point on the infinite line through
	// endA and endB that is closest to the given point p.  Note that this
	// may be beyond the range of endA and endB.  See nearestPointOnLineSegment.
	//
	// Parameters:
	//  endA: one point on the line (map or list)
	//  endB: another point on the line (map or list)
	//  p: point of interest (map or list)
	// Returns: an [x,y] list of the nearest point on the line to p.
	nearestPointOnLine = @_nearestPointOnLine
	
	// nearestPointOnLineSegment: calculate the point on the line segment from 
	// endA to endB that is closest to the given point p.  This will never 
	// return a point outside the given line segment.
	//
	// Parameters:
	//  endA: one point on the line (map or list)
	//  endB: another point on the line (map or list)
	//  p: point of interest (map or list)
	// Returns: an [x,y] list of the nearest point on the line segment to p.
	nearestPointOnLineSegment = @_nearestPointOnLineSegment
	
	// distanceToLine: calculate the shortest distance between the given
	// point and an infinite line.
	// Parameters:
	//  endA: one point on the line (map or list)
	//  endB: another point on the line (map or list)
	//  p: point of interest (map or list)
	// Returns: distance from p to the line
	distanceToLine = function(endA, endB, p)
		ptOnLine = _nearestPointOnLine(endA, endB, p)
		return distance(p, ptOnLine)
	end function
	
	// distanceToLineSegment: calculate the shortest distance between 
	// the given point and a line segment between two points.
	// Parameters:
	//  endA: one point on the line (map or list)
	//  endB: another point on the line (map or list)
	//  p: point of interest (map or list)
	// Returns: distance from p to the line
	distanceToLineSegment = function(endA, endB, p)
		ptOnLine = _nearestPointOnLineSegment(endA, endB, p)
		return distance(p, ptOnLine)
	end function
	
	// lineIntersectProportion: look for an intersection between
	// line p1-p2 and line p3-p4.  Return the fraction of the way
	// from p1 to p2 where this intersection occurs.  If the two
	// lines are parallel, there is no intersection and this
	// function returns null.
	lineIntersectProportion = @_lineIntersectProportion
	
	// lineSegmentsIntersect: return whether the line segment p1-p2
	// intersects segment p3-p4.
	lineSegmentsIntersect = @_lineSegmentsIntersect
	
	// _lineLineIntersection: return the point at which line p1-p2
	// intersects line p3-p4.  If there is no intersection, this
	// function returns null.
	lineLineIntersection = @_lineLineIntersection
	
	// reflect: reflect a vector across a given normal.  This does
	// the classic "angle of incidence equals angle of reflection"
	// thing, where the angle is measured relative to the given
	// normal vector.
	// Parameters:
	//	vector: [x,y] list representing vector to be reflected
	//	normal: [x,y] list of normal vector (with length == 1)
	// Returns: new [x,y] list representing reflected vector.
	reflect = function(vector, normal)
		dotProd = vector[0]*normal[0] + vector[1]*normal[1]
		return [vector[0] - 2 * dotProd * normal[0],
				vector[1] - 2 * dotProd * normal[1] ]
	end function

	// bounceVelocity: Calculates the new velocity of something like
	// a ball, after colliding with a (possibly moving) line segment.
	// Parameters:
	//	ball: a map containing:
	//    x, y: position at point of contact
	//    vx, vy: velocity, in units per frame
	//	lineSeg: a map containing:
	//    x1, y1: position of first endpoint
	//    x2, y2: position of second endpoint
	//    prevX1, prevY1: previous position of first endpoint (optional)
	//    prevX2, prevY2: previous position of second endpoint (optional)
	// friction: determines strength of sideways friction effect.
	// Returns: a new map containing:
	//    vx, vy: new velocity after the bounce
	bounceVelocity = function(ball, lineSeg, friction=0.1)		
		// Compute velocity of each line segment endpoint
		if lineSeg.hasIndex("prevX1") then
			v1x = lineSeg.x1 - lineSeg.prevX1
			v1y = lineSeg.y1 - lineSeg.prevY1
		else
			v1x = 0
			v1y = 0
		end if
		if lineSeg.hasIndex("prevX2") then
			v2x = lineSeg.x2 - lineSeg.prevX2
			v2y = lineSeg.y2 - lineSeg.prevY2
		else
			v2x = 0
			v2y = 0
		end if
		
		// Compute contact point interpolation factor
		lambda = proportionAlongLine(
		[lineSeg.x1, lineSeg.y1], [lineSeg.x2, lineSeg.y2], [ball.x, ball.y])
		contactVX = lerp(v1x, v2x, lambda)
		contactVY = lerp(v1y, v2y, lambda)
		
		// Compute normal of the line segment
		nx = -(lineSeg.y2 - lineSeg.y1)
		ny = lineSeg.x2 - lineSeg.x1
		nlen = sqrt(nx*nx + ny*ny)
		nx /= nlen; ny /= nlen
		
		// Relative velocity in ball's moving frame
		relVX = ball.vx - contactVX
		relVY = ball.vy - contactVY
		
		// Reflect the relative velocity
		newRel = reflect([relVX, relVY], [nx, ny])
		
		// Friction: Take the component of the velocity in the direction
		// of the segment (i.e., perpendicular to the normal).
		perp = [ny, -nx]
		dot = relVX * perp[0] + relVY * perp[1]
		sideVec = [dot * perp[0], dot * perp[1]]		
		result = {
		  "vx": newRel[0] - sideVec[0] * friction,
		  "vy": newRel[1] - sideVec[1] * friction }
		return result
	end function
	
	// bounceOffSegment: Bounce a ball-like object -- which is attempting
	// to move from ball.x-ball.vx, ball.y-ball.vy to ball.x, ball.y -- off
	// of a line segment.  Segment contains x1,y1 and x2,y2 defining its
	// current position; optionally also prevX1, prevY1 and prevX2, prevY2,
	// defining its previous position (for a moving segment).  The fracBefore
	// parameter is optional, and if given, short-cuts figuring out what 
	// fraction of the ball's path comes before the impact.  This function
	// updates the x,y and vx,vy properties of ball, in place.
	bounceOffSegment = function(ball, segment, friction=0.1, fracBefore=null)
		prevBallX = ball.x - ball.vx
		prevBallY = ball.y - ball.vy
		prevBallPos = [prevBallX, prevBallY]
		if fracBefore == null then
			fracBefore = lineIntersectProportion(
			   prevBallPos, ball, 
			   [segment.x1, segment.y1], 
			   [segment.x2, segment.y2])
		end if
		ball.x = lerp(prevBallX, ball.x, fracBefore - 0.01)
		ball.y = lerp(prevBallY, ball.y, fracBefore - 0.01)
		newV = bounceVelocity(ball, segment, friction)
		ball.vx = newV.vx
		ball.vy = newV.vy
		// If you wanted a version of this method that updates the ball to
		// its new position after the bounce, instead of stopping at the hit
		// point, you would do this:
		// fracAfter = 1 - fracBefore
		// ball.x += ball.vx * fracAfter
		// ball.y += ball.vy * fracAfter
		// ...but this is a bad idea unless your world has only one segment to
		// bounce off of, because doing this will very likely cause the ball
		// to pass right through nearby segments, instead of bouncing off of
		// them.  Better to stop the ball for now at the contact point.
	end function
	
	// bounceOffStaticPoly:  Bounce a ball-like object -- which is attempting
	// to move from ball.x-ball.vx, ball.y-ball.vy to ball.x, ball.y -- off
	// of a stationary polygon with points defined by polyPts.
	// Returns true if bounced, false if not.
	bounceOffStaticPoly = function(ball, polyPts, friction=0.1)
		prevBallX = ball.x - ball.vx
		prevBallY = ball.y - ball.vy
		prevBallPos = [prevBallX, prevBallY]
		// Find the segment which *first* intersects the ball.
		seg = {}
		bestFrac = 1
		bestSeg = null
		for i in range(polyPts, polyPts.len-1)
			seg.x1 = polyPts[i][0]
			seg.y1 = polyPts[i][1]
			nexti = (i + 1) % polyPts.len
			seg.x2 = polyPts[nexti][0]
			seg.y2 = polyPts[nexti][1]
			segEnd1 = [seg.x1, seg.y1]
			segEnd2 = [seg.x2, seg.y2]
			if not lineSegmentsIntersect(prevBallPos, ball,
			  segEnd1, segEnd2) then continue
			fracBefore = lineIntersectProportion(
			  prevBallPos, ball, segEnd1, segEnd2)
			if fracBefore <= 0 or fracBefore > bestFrac then continue			
			bestFrac = fracBefore
			bestSeg = seg
			seg = {}
		end for
		if bestSeg == null then return false
		bounceOffSegment ball, bestSeg, friction, bestFrac
		return true
	end function
	
	// bounceOffMovingPoly: Bounce a ball-like object -- which is attempting
	// to move from ball.x-ball.vx, ball.y-ball.vy to ball.x, ball.y -- off
	// of a polygon with points defined by polyPts, also moving from a previous
	// position at prevPolyPts.
	// Returns true if bounced, false if not.
	bounceOffMovingPoly = function(ball, polyPts, prevPolyPts=null, friction=0.1)
		// Find the segment which *first* intersects the ball.
		seg = {}
		bestFrac = 1
		bestSeg = null
		for i in range(polyPts, polyPts.len-1)
			// For each segment of the moving poly, we're going to adjust
			// the ball's velocity to zero out the segment's velocity, and
			// do all our work in that adjusted frame... then adjust back
			// at the end.
			seg.x1 = polyPts[i][0]
			seg.y1 = polyPts[i][1]
			nexti = (i + 1) % polyPts.len
			seg.x2 = polyPts[nexti][0]
			seg.y2 = polyPts[nexti][1]
			segEnd1 = [seg.x1, seg.y1]
			segEnd2 = [seg.x2, seg.y2]
			segVx = ((seg.x1 - prevPolyPts[i][0]) + 
					 (seg.x2 - prevPolyPts[nexti][0])) / 2
			segVy = ((seg.y1 - prevPolyPts[i][1]) + 
					 (seg.y2 - prevPolyPts[nexti][1])) / 2
			aball = ball + {"vx":ball.vx - segVx, "vy":ball.vy - segVy}
			prevBallX = aball.x - aball.vx
			prevBallY = aball.y - aball.vy
			prevBallPos = [prevBallX, prevBallY]
			if not lineSegmentsIntersect(prevBallPos, aball,
			  segEnd1, segEnd2) then continue
			fracBefore = lineIntersectProportion(
			  prevBallPos, aball, segEnd1, segEnd2)
			if fracBefore <= 0 or fracBefore > bestFrac then continue
			bestFrac = fracBefore
			bestSeg = seg
			seg = {}
			if prevPolyPts then
				bestSeg.prevX1 = prevPolyPts[i][0] + segVx
				bestSeg.prevY1 = prevPolyPts[i][1] + segVy
				bestSeg.prevX2 = prevPolyPts[nexti][0] + segVx
				bestSeg.prevY2 = prevPolyPts[nexti][1] + segVy
				bestSeg.vx = segVx
				bestSeg.vy = segVy
			end if
		end for
		if bestSeg == null then return false
		bounceOffSegment aball, bestSeg, friction, bestFrac
		ball.x = aball.x; ball.y = aball.y
		ball.vx = aball.vx + bestSeg.vx
		ball.vy = aball.vy + bestSeg.vy
		return true
	end function
	
	// bounceOffPoly: Bounce a ball-like object -- which is attempting to
	// move from ball.x-ball.vx, ball.y-ball.vy to ball.x, ball.y -- off of 
	// a polygon with points defined by polyPts, possibly also moving from a 
	// previous position at prevPolyPts.
	// Returns true if bounced, false if not.
	bounceOffPoly = function(ball, polyPts, prevPolyPts=null, friction=0.1)
		if prevPolyPts == null then
			return bounceOffStaticPoly(ball, polyPts, friction)
		else
			return bounceOffMovingPoly(ball, polyPts, prevPolyPts, friction)
		end if
	end function
end if

if intrinsics.hasIndex("_polyPerimeter") then

	// polyPerimeter: Measure the total distance around the given polygon
	// (including from the last point back to the first).
	//
	// Parameters:
	//	polygon: list of [x,y] points defining a polygon
	// Returns: polygon perimeter.
	polyPerimeter = @_polyPerimeter
	
	// polyArea: Calculate the area within the given polygon.
	//
	// Parameters:
	//	polygon: list of [x,y] points defining a polygon
	// Returns: polygon area.
	polyArea = @_polyArea
	
	// pointInPoly: Test whether a given [x,y] point, or any of a list of
	// of [x,y] points, is contained within the given polygon.
	//
	// Parameters:
	//	point: either [x,y], or a list, e.g. [[x1,y1], [x2,y2], [x3,y3]]
	//	polygon: list of [x,y] points defining a polygon
	// Returns: true if any of the given points is within the polygon;
	//			false otherwise.
	pointInPoly = @_pointInPoly
	
	// offsetPoly: insets (shrinks) or outsets (grows) a polygon.  (Which
	// it does depends on whether the polygon is defined clockwise or
	// counter-clockwise.)  The given polygon is unchanged; instead a 
	// NEW list of points is returned with the result.
	//
	// Parameters:
	//	polygon: list of points defining a polygon
	//	delta: amount to inset/outset the polygon points
	// Returns: new list of points defining offset polygon
	offsetPoly = @_offsetPoly
	
	// shiftPoly: translates a polygon by adding some amount to the X
	// and Y values of each point. The given polygon is unchanged; instead
	// a NEW list of points is returned with the result.
	//
	// Parameters:
	//	polygon: list of points defining a polygon
	//	dx: amount to add to the X coordinate of each point
	//	dy: amount to add to the Y coordinate of each point
	// Returns: new list of points defining translated polygon
	shiftPoly = @_translatePoly
	
	// rotatePoly: rotates all points in the given list around 0,0.
	// The given polygon is unchanged; instead a NEW list of points
	// in returned with the result.
	//
	// Parameters:
	//	polygon: list of points defining a polygon
	//	degrees: amount of rotation to apply, in degrees clockwise
	// Returns: new list of points defining rotated polygon
	rotatePoly = @_rotatePoly
end if

// randNormal: return a normally-destributed (i.e. Gaussian) number
// with the given mean and standard deviation.
//  mean: average value of the returned numbers
//  stddev: standard deviation of the returned numbers
randNormal = function(mean=0, stddev=1)
    return mean + sqrt(-2 * log(rnd,2.7182818284)) * cos(2*pi*rnd) * stddev
end function

// randRange: return a uniformly distributed random number between the
// given minimum and maximum values (including min, but not including max).
randRange = function(min, max)
	return min + (max - min) * rnd
end function

// dice: return the sum of rolling some number of dice.
// For example, dice(3,6) simulates rolling three 6-sided dice.
dice = function(numberOfDice=1, sides=6)
	sum = 0
	for i in range(1, numberOfDice)
		sum = sum + ceil(rnd * sides)
	end for
	return sum
end function

// clamp: limits the given value to a specified range.
clamp = function(x, minval=0, maxval=1)
	if x < minval then return minval
	if x > maxval then return maxval
	return x
end function

// max: returns the greater of two values.
// (Note: if you have more than two, consider importing listUtil
// and using list.max instead.)
max = function(a, b)
	if b > a then return b else return a
end function

// min: returns the lesser of two values.
// (As above, also consider list.min from listUtil.)
min = function(a, b)
	if b < a then return b else return a
end function

// numToStr: converts a number to a string, with a specified precision
// (number of digits past the decimal place).  Trailing zeros will be
// added as needed to get the decimal point at the right place.
numToStr = function(n, precision=null)
	if precision == null then return str(n)
	if not n isa number or not precision isa number then
		print "numToStr error: arguments must be numbers."
		return
	end if
	if precision <= 0 or n == 1/0 or n == -1/0 or n != n then return str(round(n, precision))
	negative = n < 0; n = abs(n)
	digits = [floor(n)]
	for i in range(1, precision)
		d = floor(n * 10^i) % 10
		digits.push d
	end for
	if (n * 10^precision) % 1 >= 0.5 then
		for i in range(digits.len - 1)
			digits[i] += 1
			if digits[i] < 10 or i == 0 then break
			digits[i] = 0
		end for
	end if
	return "-" * negative + digits.pull + "." + digits.join("")
end function

runUnitTests = function
	print "Unit testing: mathUtil"
	
	errorCount = 0
	assertEqual = function(actual, expected, note)
		if actual != expected then
			print "Unit test failure (" + note + "): expected " + expected + ", got " + actual
			outer.errorCount = errorCount + 1
		end if
	end function
	
	if intrinsics.hasIndex("_polyPerimeter") then
		quadPoly = [[0,0], [100,0], [100,50], [0,50]]
		assertEqual polyPerimeter(quadPoly), 300, "polyPerimeter"
		assertEqual polyArea(quadPoly), 5000, "polyArea"
	end if
	
	if intrinsics.hasIndex("_proportionAlongLine") then
		endA = [100,50]
		endB = {"x":100, "y":150}
		assertEqual proportionAlongLine(endA, endB, [200,100]), 0.5, "proportionAlongLine"
		assertEqual nearestPointOnLine(endA, endB, [200,100]), [100,100], "nearestPointOnLine"
		assertEqual nearestPointOnLineSegment(endA, endB, [200,100]), [100,100], "nearestPointOnLineSegment"
		assertEqual nearestPointOnLine(endA, endB, [100,10]), [100,10], "nearestPointOnLine"
		assertEqual nearestPointOnLineSegment(endA, endB, [100,10]), [100,50], "nearestPointOnLineSegment"
		assertEqual distanceToLine(endA, endB, [200,80]), 100, "nearestPointOnLine"
		assertEqual distanceToLineSegment(endA, endB, [100,200]), 50, "nearestPointOnLineSegment"
	
		endA2 = {"x":80, "y":70}
		endB2 = [120, 70]
		assertEqual lineSegmentsIntersect(endA, endB, endA2, endB2), true, "lineSegmentsIntersect"
		assertEqual round(lineIntersectProportion(endA, endB, endA2, endB2),2), 0.20, "proportionAlongLine"
		assertEqual lineLineIntersection(endA, endB, endA2, endB2), [100,70], "lineLineIntersection"
	end if
	
	assertEqual moveTowards(100, 25, 10), 90, "moveTowards"
	mover = {"x":50, "y":25}
	target = {"x":60, "y":25}
	assertEqual moveTowardsXY(mover, target), true
	assertEqual mover.x, 51, "moveTowardsXY"
	assertEqual mover.y, 25, "moveTowardsXY"
	target = {"x":51, "y":30}
	assertEqual moveTowardsXY(mover, target, 10), true
	assertEqual mover.x, 51, "moveTowardsXY"
	assertEqual mover.y, 30, "moveTowardsXY"
	assertEqual moveTowardsXY(mover, target, 10), false
	
	assertEqual numToStr(pi, 2), "3.14", "numToStr"
	assertEqual numToStr(pi, 4), "3.1416", "numToStr"
	assertEqual numToStr(pi, 12), "3.141592653590", "numToStr"
	assertEqual numToStr(1.23, 3), "1.230", "numToStr"
	assertEqual numToStr(1.23), "1.23", "numToStr"
	assertEqual numToStr(12345.67, -2), "12300", "numToStr"
	assertEqual numToStr(2, 3), "2.000", "numToStr"
	assertEqual numToStr(2/3, 12), "0.666666666667", "numToStr"
	assertEqual numToStr(41.9999, 2), "42.00", "numToStr"
	assertEqual numToStr(42 - 1E-12, 5), "42.00000", "numToStr"
	assertEqual numToStr(-pi, 4), "-3.1416", "numToStr"
	
	if errorCount == 0 then
		print "All tests passed.  Woot!"
	else
		print errorCount + " error" + "s" * (errorCount!=1) + " found."
	end if
end function

if globals == locals then runUnitTests
