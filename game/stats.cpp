/*
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "stats.h"
#include "../grinliz/glutil.h"
#include "../grinliz/glrandom.h"
#include "gamelimits.h"
#include "../tinyxml/tinyxml.h"
#include "unit.h"
#include "../grinliz//glgeometry.h"

using namespace grinliz;


int Stats::GenStat( grinliz::Random* rand, int min, int max ) 
{
	int s = min + grinliz::LRintf( (float)(max-min) * rand->DiceUniform( 3, 6 ) );
	return Clamp( s, 1, TRAIT_MAX );
}


void Stats::CalcBaselines()
{
	int levSTR = Clamp( STR() + TRAIT_RANK_BONUS*Rank(), 1, TRAIT_MAX );
	int levDEX = Clamp( DEX() + TRAIT_RANK_BONUS*Rank(), 1, TRAIT_MAX );
	int levPSY = Clamp( PSY() + TRAIT_RANK_BONUS*Rank(), 1, TRAIT_MAX );

	totalHP =					levSTR;

	totalTU = Interpolate(		0.0f,						(float)MIN_TU,
								(float)TRAIT_MAX,			(float)MAX_TU,
								(float)(levDEX + levSTR)*0.5f );
	// be sure...
	if ( totalTU > (float)MAX_TU )
		totalTU = (float)MAX_TU;

	accuracy = Interpolate(		0.0f,						ACC_WORST_SHOT,
								(float)TRAIT_MAX,			ACC_BEST_SHOT,
								(float)levDEX );

	reaction = Interpolate(		0.0f,						REACTION_FAST,
								(float)TRAIT_MAX,			REACTION_SLOW,
								(float)(levDEX + levPSY)*0.5f );
}


void Stats::Save( TiXmlElement* doc ) const
{
	TiXmlElement* element = new TiXmlElement( "Stats" );
	element->SetAttribute( "STR", _STR );
	element->SetAttribute( "DEX", _DEX );
	element->SetAttribute( "PSY", _PSY );
	element->SetAttribute( "rank", rank );
	doc->LinkEndChild( element );
}


void Stats::Load( const TiXmlElement* parent )
{
	const TiXmlElement* ele = parent->FirstChildElement( "Stats" );
	GLASSERT( ele );
	if ( ele ) {
		ele->QueryValueAttribute( "STR", &_STR );
		ele->QueryValueAttribute( "DEX", &_DEX );
		ele->QueryValueAttribute( "PSY", &_PSY );
		ele->QueryValueAttribute( "level", &rank );
		ele->QueryValueAttribute( "rank", &rank );
		CalcBaselines();
	}
}



void BulletSpread::Generate( float uniformX, float uniformY, grinliz::Vector2F* result )
{
	const static float Y_RATIO = 0.7f;
	result->x = Deviation( uniformX );
	result->y = Deviation( uniformY ) * Y_RATIO;
}


void BulletSpread::Generate( int seed, grinliz::Vector2F* result )
{
	const static float MULT = 1.f / 65535.f;

	float x	= -1.0f + 2.0f * ((float)((U32)seed & 0xffff) * MULT);
	float y	= -1.0f + 2.0f * ((float)((U32)(seed>>16)) * MULT);

	Generate( x, y, result );
}


void BulletSpread::Generate( int seed, float radius, const grinliz::Vector3F& dir, const grinliz::Vector3F& target,  grinliz::Vector3F* targetPrime )
{
	Vector2F spread;
	Generate( seed, &spread );
	spread.x *= radius;
	spread.y *= radius;

	Vector3F normal = dir; 
	normal.Normalize();

	const static Vector3F UP = { 0, 1, 0 };
	Vector3F right;
	CrossProduct( normal, UP, &right );

	*targetPrime = target + spread.x*right + spread.y*UP;
}


float BulletSpread::ComputePercent( float radius, float distance, float width, float height )
{
	enum { X = 6, Y = 8 };
	static const char map[X*Y+1] =	"  xx  "
									" xxxx "
									"xxxxxx"
									"xxxxxx"
									"xxxxxx"
									"xxxxxx"
									" x  x "
									" x  x ";

	Vector2F center = { width*0.50f, height*0.65f };
	int hit = 0;

	for( int j=0; j<10; ++j ) {
		float y0 = -1.0f + (float)j*0.2f;

		for( int i=0; i<10; ++i ) {
			float x0 = -1.0f + (float)i*0.2f;

			Vector2F v;
			Generate( x0, y0, &v );

			Vector2F pos;
			pos.x = center.x + v.x*radius*distance;
			pos.y = center.y + v.y*radius*distance;

			if ( pos.x > 0 && pos.x < width && pos.y > 0 && pos.y < height ) {
				int mx = (int)( pos.x*float(X) / width );
				int my = (int)( pos.y*float(Y) / height );
				mx = Clamp( mx, 0, X-1);
				my = Clamp( my, 0, Y-1);
				if ( map[my*X+mx] == 'x' )
					++hit;
			}
		}
	}
	float result = (float)hit / 100.0f;
	return Clamp( result, 0.0f, 0.95f );
}
