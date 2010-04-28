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

#ifndef UFOATTACK_SURFACE_INCLUDED
#define UFOATTACK_SURFACE_INCLUDED

#include "../grinliz/gltypes.h"
#include "../grinliz/gldebug.h"
#include "../grinliz/glrectangle.h"
#include "../grinliz/glvector.h"

#include "../shared/glmap.h"
#include "../engine/ufoutil.h"
#include "../shared/gamedbreader.h"

#include "enginelimits.h"
#include <stdio.h>

// FIXME: this still operates in texture coordinates.
// Flip to image coordinates, and change the texture loading code.

class Surface
{
public:
	// WARNING: duplicated in Texture
	enum  {			// channels	bytes
		RGBA16,		// 4444		2
		RGB16,		// 565		2
		ALPHA,		// 8		1
	};

	struct RGBA {
		U8 r, g, b, a;
	};

	static U16 CalcColorRGB16( RGBA rgba )
	{
		U32 c =   ((rgba.r>>3) << 11)
				| ((rgba.g>>2) << 5)
				| ((rgba.b>>3) );
		return (U16)c;
	}

	static void CalcRGB16( U16 c, RGBA* rgb ) {
		U32 r = (c & 0xF800) >> 11;		// 5, 0-31
		U32 g = (c & 0x07E0) >> 5;		// 6, 0-61
		U32 b = (c & 0x001F);			// 5, 0-31

		// 0-15 is the range.
		// 0  -> 0
		// 15 -> 255
		rgb->r = (r<<3)|(r>>2);
		rgb->g = (g<<2)|(g>>4);
		rgb->b = (b<<3)|(b>>2);
		rgb->a = 255;
	}

	static U16 CalcColorRGBA16( RGBA rgba )
	{
		U32 c =   ( (rgba.r>>4) << 12 )
			    | ( (rgba.g>>4) << 8 )
				| ( (rgba.b>>4) << 4 )
				| ( (rgba.a>>4) << 0 );
		return (U16)c;
	}

	static void CalcRGBA16( U16 color, RGBA* rgb ) {
		U32 r = (color>>12);
		U32 g = (color>>8)&0x0f;
		U32 b = (color>>4)&0x0f;
		U32 a = color&0x0f;

		// 0-15 is the range.
		// 0  -> 0
		// 15 -> 255
		rgb->r = (r<<4)|r;
		rgb->g = (g<<4)|g;
		rgb->b = (b<<4)|b;
		rgb->a = (a<<4)|a;
	}

	Surface();
	~Surface();

	int			Width()	const			{ return w; }
	int			Height() const			{ return h; }
	U8*			Pixels()				{ return pixels; }
	const U8*	Pixels() const			{ return pixels; }
	int			BytesPerPixel() const	{ return (format==ALPHA) ? 1 : 2; }
	int			BytesInImage() const	{ GLASSERT( w*h*BytesPerPixel() <= allocated ); return w*h*BytesPerPixel(); }
	bool		Alpha() const			{ return (format!=RGB16) ? true : false; }
	int			Format() const			{ return format; }

	// Surfaces are flipped (as are images) for OpenGL. This "unflips" back to map==pixel coordinates.
	U16	ImagePixel16( int x, int y ) const	{ 
		GLASSERT( x >=0 && x < Width() );
		GLASSERT( y >=0 && y < Height() );
		GLASSERT( BytesPerPixel() == 2 );
		return *((const U16*)pixels + (h-1-y)*w + x);
	}
	void SetImagePixel16( int x, int y, U16 c ) {
		GLASSERT( x >=0 && x < Width() );
		GLASSERT( y >=0 && y < Height() );
		GLASSERT( BytesPerPixel() == 2 );
		*((U16*)pixels + (h-1-y)*w + x) = c;
	}

	U16 Pixel16( int x, int y ) const {
		GLASSERT( x >=0 && x < Width() );
		GLASSERT( y >=0 && y < Height() );
		GLASSERT( BytesPerPixel() == 2 );
		return *((const U16*)pixels + (h-1-y)*w + x);
	}

	void SetPixel16( int x, int y, U16 c ) {
		GLASSERT( x >=0 && x < Width() );
		GLASSERT( y >=0 && y < Height() );
		GLASSERT( BytesPerPixel() == 2 );
		*((U16*)pixels + y*w + x) = c;
	}

	// Set the format and allocate memory.
	void Set( int format, int w, int h );

	void Clear( int c );
	void Blit(	const grinliz::Vector2I& target, 
				const Surface* src, 
				const grinliz::Rectangle2I& srcRect );

	void Blit(	const grinliz::Rectangle2I& target,
				const Surface* src,
				const Matrix2I& xformTargetToSrc );

	static int QueryFormat( const char* formatString );

	// -- Metadata about the surface --
	void SetName( const char* n );
	const char* Name() const			{ return name; }
	
private:
	Surface( const Surface& );
	void operator=( const Surface& );

	int format;
	int w;
	int h;
	int allocated;
	char name[EL_FILE_STRING_LEN];
	U8* pixels;
};


class ImageManager
{
public:
	static ImageManager* Instance()	{ GLASSERT( instance ); return instance; }
	
	const Surface* GetImage( const char* name );
	
	Surface* AddLockedSurface();
	void Unlock();

	static void Create();
	static void Destroy();
private:
	ImageManager()		{}
	~ImageManager()		{}

	enum {
		MAX_IMAGES = 30		// increase as needed
	};

	static ImageManager* instance;
	CArray< Surface, MAX_IMAGES > arr;		// textures
	CStringMap<	Surface* > map;
};


#endif // UFOATTACK_SURFACE_INCLUDED