#include "Fixedgeom.h"
using namespace grinliz;

void DumpRectangle( const Rectangle3X& r ) {
	GLOUTPUT(( "(%.2f,%.2f,%.2f)-(%.2f,%.2f,%.2f)", 
			(float)r.min.x, (float)r.min.y, (float)r.min.z,
			(float)r.max.x, (float)r.max.y, (float)r.max.z ));
}


void DumpRectangle( const Rectangle2X& r ) {
	GLOUTPUT(( "(%.2f,%.2f)-(%.2f,%.2f)", 
			(float)r.min.x, (float)r.min.y,
			(float)r.max.x, (float)r.max.y ));
}


void PlaneX::Convert( const grinliz::Plane& plane )
{
	n.x = plane.n.x;
	n.y = plane.n.y;
	n.z = plane.n.z;
	d = plane.d;
}



Fixed PlanePointDistanceSquared( const PlaneX& plane, const Vector3X& point )
{
	// http://mathworld.wolfram.com/Point-PlaneDistance.html
	Fixed num = plane.n.x*point.x + plane.n.y*point.y + plane.n.z*point.z + plane.d;
	Fixed denom = plane.n.x*plane.n.x + plane.n.y*plane.n.y + plane.n.z*plane.n.z;
	Fixed d2 = num*num/denom;
	return d2;
}


int ComparePlaneSphereX( const PlaneX& plane, const SphereX& sphere )
{
	Fixed d2 = PlanePointDistanceSquared( plane, sphere.origin );
	Fixed r2 = sphere.radius*sphere.radius;

	if ( d2 <= 0 ) {
		if ( d2 > r2 ) {
			return grinliz::POSITIVE;
		}
	}
	else if ( d2 < -r2 ) {
		return grinliz::NEGATIVE;
	}
	return grinliz::INTERSECT;
}


int IntersectRaySphereX(	const SphereX& sphere,
							const Vector3X& p,
							const Vector3X& dir,
							Fixed* t )
{
	Vector3X raySphere = sphere.origin - p;
	Fixed raySphereLen2 = DotProduct( raySphere, raySphere );
	Fixed sphereR2 = sphere.radius*sphere.radius;

	if (raySphereLen2 < sphereR2) 
	{	
		// Origin is inside the sphere.
		return grinliz::INSIDE;
	} 
	else 
	{
		// Clever idea: what is the rays closest approach to the sphere?
		// see: http://www.devmaster.net/wiki/Ray-sphere_intersection

		Fixed closest = DotProduct(raySphere, dir);
		if (closest < 0) {
			// Then we are pointing away from the sphere (and we know we aren't inside.)
			return grinliz::REJECT;
		}
		Fixed halfCordLen = (sphereR2 - raySphereLen2) / DotProduct(dir, dir) + (closest*closest);
		if ( halfCordLen > 0 ) {
			*t = closest - halfCordLen.Sqrt();
			return grinliz::INTERSECT;
		}
	}
	return grinliz::REJECT;
}


int ComparePlaneAABBX( const PlaneX& plane, const Rectangle3X& aabb )
{
	// Only need to check the most positive and most negative point,
	// or minimum and maximum depending on point of view. The octant
	// of the plane normal implies a vertex to check.

	Vector3X posPoint, negPoint;

	if ( plane.n.x > 0 )	{
		posPoint.x = aabb.max.x;
		negPoint.x = aabb.min.x;
	} else {
		posPoint.x = aabb.min.x;
		negPoint.x = aabb.max.x;
	}

	if ( plane.n.y > 0 )	{
		posPoint.y = aabb.max.y;
		negPoint.y = aabb.min.y;
	} else {
		posPoint.y = aabb.min.y;
		negPoint.y = aabb.max.y;
	}

	if ( plane.n.z > 0 ) {
		posPoint.z = aabb.max.z;
		negPoint.z = aabb.min.z;
	} else {
		posPoint.z = aabb.min.z;
		negPoint.z = aabb.max.z;
	}		

	// RTR 735
	// f(P) = N*P + d

	// If the most negative point is still on the positive
	// side, it is a positive, and vice versa.
	Fixed fp = DotProduct( plane.n, negPoint ) + plane.d;
	if ( fp > 0 )
	{
		return grinliz::POSITIVE;
	}

	fp = DotProduct( plane.n, posPoint ) + plane.d;
	if ( fp < 0 )
	{
		return grinliz::NEGATIVE;
	}
	return grinliz::INTERSECT;
}


int IntersectRayAABBX( const Vector3X& origin, 
						const Vector3X& dir,
						const Rectangle3X& aabb,
						Vector3X* intersect,
						Fixed* t )
{
	enum
	{
		RIGHT,
		LEFT,
		MIDDLE
	};

	bool inside = true;
	int quadrant[3];
	int i;
	int whichPlane;
	Fixed maxT[3];
	Fixed candidatePlane[3];

	const Fixed *pOrigin = &origin.x;
	const Fixed *pBoxMin = &aabb.min.x;
	const Fixed *pBoxMax = &aabb.max.x;
	const Fixed *pDir    = &dir.x;
	Fixed *pIntersect = &intersect->x;

	// Find candidate planes
	for (i=0; i<3; ++i)
	{
		if( pOrigin[i] < pBoxMin[i] ) 
		{
			quadrant[i] = LEFT;
			candidatePlane[i] = pBoxMin[i];
			inside = false;
		}
		else if ( pOrigin[i] > pBoxMax[i] ) 
		{
			quadrant[i] = RIGHT;
			candidatePlane[i] = pBoxMax[i];
			inside = false;
		}
		else	
		{
			quadrant[i] = MIDDLE;
		}
	}

	// Ray origin inside bounding box
	if( inside )	
	{
		*intersect = origin;
		*t = 0;
		return grinliz::INSIDE;
	}


	// Calculate T distances to candidate planes
	for (i = 0; i < 3; i++)
	{
		if (   quadrant[i] != MIDDLE 
			&& ( pDir[i].x > Fixed::EPSILON || pDir[i].x < -Fixed::EPSILON ) )
		{
			maxT[i] = ( candidatePlane[i]-pOrigin[i] ) / pDir[i];
		}
		else
		{
			maxT[i] = -1;
		}
	}

	// Get largest of the maxT's for final choice of intersection
	whichPlane = 0;
	for (i = 1; i < 3; i++)
		if (maxT[whichPlane] < maxT[i])
			whichPlane = i;

	// Check final candidate actually inside box
	if ( maxT[whichPlane] < 0 )
		return grinliz::REJECT;

	for (i = 0; i < 3; i++)
	{
		if (whichPlane != i ) 
		{
			pIntersect[i] = pOrigin[i] + maxT[whichPlane]*pDir[i];
			if (pIntersect[i] < pBoxMin[i] || pIntersect[i] > pBoxMax[i])
				return grinliz::REJECT;
		} 
		else 
		{
			pIntersect[i] = candidatePlane[i];
		}
	}
	*t = maxT[whichPlane];
	return grinliz::INTERSECT;
}	
