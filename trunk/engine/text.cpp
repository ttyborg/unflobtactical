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

#include "text.h"
#include "platformgl.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "../grinliz/glutil.h"

const int ADVANCE = 10;
const int ADVANCE_SMALL = 8;
const float SMALL_SCALE = 0.75f;
const int GLYPH_CX = 16;
const int GLYPH_CY = 8;
static int GLYPH_WIDTH = 256 / GLYPH_CX;
static int GLYPH_HEIGHT = 128 / GLYPH_CY;


Screenport UFOText::screenport( 320, 480, 1 );
U32 UFOText::textureID = 0;

void UFOText::InitScreen( const Screenport& sp )
{
	screenport = sp;
}


void UFOText::InitTexture( U32 textTextureID )
{
	GLASSERT( textTextureID );
	textureID = textTextureID;
}


void UFOText::Begin()
{
	GLASSERT( textureID );

	glDisable( GL_DEPTH_TEST );
	glDepthMask( GL_FALSE );
	glEnable( GL_BLEND );
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisableClientState( GL_NORMAL_ARRAY );

	glColor4f( 1.f, 1.f, 1.f, 1.f );

	screenport.PushUI();
	glBindTexture( GL_TEXTURE_2D, textureID );
}


void UFOText::End()
{
	glEnableClientState( GL_NORMAL_ARRAY );
	screenport.PopUI();

	glDisable( GL_BLEND );
	glEnable( GL_DEPTH_TEST );
	glDepthMask( GL_TRUE );
}


void UFOText::TextOut( const char* str, int x, int y )
{
	float v[8];
	float t[8];
	bool smallText = false;

	while( *str )
	{
		if ( *str == '.' ) {
			smallText = true;
			++str;
			continue;
		}
		if ( smallText && !( *str >= '0' && *str <= '9' ) ) {
			smallText = false;
		}

		int c = *str - 32;
		if ( c >= 1 && c < 128-32 )
		{
			float tx0 = ( 1.0f / float( GLYPH_CX ) ) * ( c % GLYPH_CX );
			float tx1 = tx0 + ( 1.0f / float( GLYPH_CX ) );

			float ty0 = 1.0f - ( 1.0f / float( GLYPH_CY ) ) * (float)( c / GLYPH_CX + 1 );
			float ty1 = ty0 + ( 1.0f / float( GLYPH_CY ) );

			t[0] = tx0;						t[1] = ty0;
			t[2] = tx1;						t[3] = ty0;
			t[4] = tx1;						t[5] = ty1;
			t[6] = tx0;						t[7] = ty1;

			if ( !smallText ) {
				v[0] = (float)x;				v[1] = (float)y;
				v[2] = (float)(x+GLYPH_WIDTH);	v[3] = (float)y;
				v[4] = (float)(x+GLYPH_WIDTH);	v[5] = (float)(y+GLYPH_HEIGHT);
				v[6] = (float)x;				v[7] = (float)(y+GLYPH_HEIGHT);
				x += ADVANCE;
			}
			else {
				float y0 = (float)(y+GLYPH_HEIGHT) - (float)GLYPH_HEIGHT * SMALL_SCALE;
				float x1 = (float)x + (float)GLYPH_WIDTH*SMALL_SCALE;

				v[0] = (float)x;				v[1] = y0;
				v[2] = x1;						v[3] = y0;
				v[4] = x1;						v[5] = (float)(y+GLYPH_HEIGHT);
				v[6] = (float)x;				v[7] = (float)(y+GLYPH_HEIGHT);
				x += ADVANCE_SMALL;
			}

			glVertexPointer( 2, GL_FLOAT, 0, v );
			glTexCoordPointer( 2, GL_FLOAT, 0, t );

			glDrawArrays( GL_TRIANGLE_FAN, 0, 4 );
		}
		else if ( *str == ' ' ) {
			x += ADVANCE;
		}
		++str;
	}
}


void UFOText::GlyphSize( const char* str, int* width, int* height )
{
	*width = 0;
	*height = 0;

	if ( str && *str ) {
		*height = GLYPH_HEIGHT;
	}
	bool small = false;
	for( const char* p = str; p && *p; ++p ) {
		if ( *p == '.' ) {
			small = true;
		}
		else if ( small ) {
			if ( *p >= '0' && *p <= '9' ) {
				*width += ADVANCE_SMALL;
			}
			else {
				*width += ADVANCE;
				small = false;
			}
		}
		else {
			*width += ADVANCE;
		}
	}
}


void UFOText::Draw( int x, int y, const char* format, ... )
{
	Begin();

    va_list     va;
    char		buffer[1024];

    //
    //  format and output the message..
    //
    va_start( va, format );
    vsprintf( buffer, format, va );
    va_end( va );

    TextOut( buffer, x, y );
	End();
}


void UFOText::Stream( int x, int y, const char* format, ... )
{
    va_list     va;
    char		buffer[1024];

    //
    //  format and output the message..
    //
    va_start( va, format );
    vsprintf( buffer, format, va );
    va_end( va );

    TextOut( buffer, x, y );
}
