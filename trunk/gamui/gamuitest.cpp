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

#include "glew.h"
#include "SDL.h"

#include "gamui.h"
#include <stdio.h>

#define TESTGLERR()	{	GLenum err = glGetError();				\
						if ( err != GL_NO_ERROR ) {				\
							printf( "GL ERR=0x%x\n", err );		\
							GAMUIASSERT( 0 );					\
						}										\
					}

using namespace gamui;

enum {
	RENDERSTATE_TEXT = 1,
	RENDERSTATE_TEXT_DISABLED,
	RENDERSTATE_NORMAL,
	RENDERSTATE_DISABLED
};

const int SCREEN_X = 600;
const int SCREEN_Y = 400;

typedef SDL_Surface* (SDLCALL * PFN_IMG_LOAD) (const char *file);
PFN_IMG_LOAD libIMG_Load;


class Renderer : public IGamuiRenderer
{
public:
	virtual void BeginRender()
	{
		TESTGLERR();
		
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho( 0, SCREEN_X, SCREEN_Y, 0, 1, -1 );

		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();				// model

		glDisable( GL_DEPTH_TEST );
		glDepthMask( GL_FALSE );
		glEnable( GL_BLEND );
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable( GL_TEXTURE_2D );

		glEnableClientState( GL_VERTEX_ARRAY );
		glEnableClientState( GL_TEXTURE_COORD_ARRAY );
		TESTGLERR();
	}

	virtual void EndRender() 
	{
		TESTGLERR();

	}

	virtual void BeginRenderState( const void* _renderState )
	{
		TESTGLERR();
		int renderState = (int)_renderState;

#if 0
		if ( renderState == RENDERSTATE_TEXT )
			glDisable( GL_BLEND );
		else
			glEnable( GL_BLEND );
#endif

		switch( renderState ) {
			case RENDERSTATE_TEXT:
			case RENDERSTATE_NORMAL:
				glColor4f( 1.f, 1.f, 1.f, 1.f );
				break;

			case RENDERSTATE_DISABLED:
			case RENDERSTATE_TEXT_DISABLED:
				glColor4f( 1.f, 1.f, 1.f, 0.5f );
				break;

			default:
				GAMUIASSERT( 0 );
				break;
		}
		TESTGLERR();
	}

	virtual void BeginTexture( const void* textureHandle )
	{
		TESTGLERR();
		glBindTexture( GL_TEXTURE_2D, (GLuint)textureHandle );
		TESTGLERR();
	}


	virtual void Render( const void* renderState, const void* textureHandle, int nIndex, const int16_t* index, int nVertex, const Gamui::Vertex* vertex )
	{
		TESTGLERR();
		glVertexPointer( 2, GL_FLOAT, sizeof(Gamui::Vertex), &vertex->x );
		glTexCoordPointer( 2, GL_FLOAT, sizeof(Gamui::Vertex), &vertex->tx );
		glDrawElements( GL_TRIANGLES, nIndex, GL_UNSIGNED_SHORT, index );
		TESTGLERR();
	}
};


class TextMetrics : public IGamuiText
{
public:
	virtual void GamuiGlyph( int c, GlyphMetrics* metric )
	{
		if ( c <= 32 || c >= 32+96 ) {
			c = 32;
		}
		c -= 32;
		int y = c / 16;
		int x = c - y*16;

		float tx0 = (float)x / 16.0f;
		float ty0 = (float)y / 8.0f;

		metric->advance = 10;
		metric->width = 16;
		metric->height = 16;
		metric->tx0 = tx0;
		metric->tx1 = tx0 + (1.f/16.f);

		metric->ty0 = ty0;
		metric->ty1 = ty0 + (1.f/8.f);
	}
};


int main( int argc, char **argv )
{    
	SDL_Surface *surface = 0;


	// SDL initialization steps.
    if ( SDL_Init( SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE | SDL_INIT_TIMER | SDL_INIT_AUDIO ) < 0 )
	{
	    fprintf( stderr, "SDL initialization failed: %s\n", SDL_GetError( ) );
		exit( 1 );
	}
	SDL_EnableKeyRepeat( 0, 0 );
	SDL_EnableUNICODE( 1 );

	const SDL_version* sversion = SDL_Linked_Version();

	void* handle = SDL_LoadObject( "SDL_image" );
	libIMG_Load = (PFN_IMG_LOAD)SDL_LoadFunction( handle, "IMG_Load" );

	SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
	SDL_GL_SetAttribute( SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute( SDL_GL_ALPHA_SIZE, 8);


	int	videoFlags  = SDL_OPENGL;      /* Enable OpenGL in SDL */
	videoFlags |= SDL_GL_DOUBLEBUFFER; /* Enable double buffering */

	surface = SDL_SetVideoMode( SCREEN_X, SCREEN_Y, 32, videoFlags );

	// Load text texture
	SDL_Surface* textSurface = SDL_LoadBMP( "stdfont2.bmp" );

	GLuint textTextureID;
	glGenTextures( 1, &textTextureID );
	glBindTexture( GL_TEXTURE_2D, textTextureID );

	glTexParameteri(	GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE );
	glTexParameteri(	GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameteri(	GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST );
	glTexImage2D( GL_TEXTURE_2D, 0,	GL_ALPHA, textSurface->w, textSurface->h, 0, GL_ALPHA, GL_UNSIGNED_BYTE, textSurface->pixels );
	SDL_FreeSurface( textSurface );


	// Load a bitmap
	SDL_Surface* imageSurface = libIMG_Load( "buttons.png" );

	GLuint imageTextureID;
	glGenTextures( 1, &imageTextureID );
	glBindTexture( GL_TEXTURE_2D, imageTextureID );

	glTexParameteri(	GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE );
	glTexParameteri(	GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameteri(	GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST );
	glTexImage2D( GL_TEXTURE_2D, 0,	GL_RGBA, imageSurface->w, imageSurface->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, imageSurface->pixels );
	SDL_FreeSurface( imageSurface );


	RenderAtom textAtom( (const void*)RENDERSTATE_TEXT, (const void*)textTextureID, 0, 0, 0, 0, 256, 128 );
	RenderAtom textAtomD = textAtom;
	textAtomD.renderState = (const void*) RENDERSTATE_TEXT_DISABLED;

	RenderAtom imageAtom( (const void*)RENDERSTATE_NORMAL, (const void*)imageTextureID, 0.5f, 0.5f, 228.f/256.f, 28.f/256.f, 100, 100 );

	RenderAtom decoAtom( (const void*)RENDERSTATE_NORMAL, (const void*)imageTextureID, 0, 0.25f, 0.25f, 0.f, 50, 50 );
	RenderAtom decoAtomD = decoAtom;
	decoAtomD.renderState = (const void*) RENDERSTATE_DISABLED;

	TextMetrics textMetrics;
	Renderer renderer;


	Gamui gamui( &renderer, textAtom, textAtomD, &textMetrics );

	TextLabel textLabel[2];
	textLabel[0].Init( &gamui );
	textLabel[1].Init( &gamui );

	textLabel[0].SetText( "Hello Gamui" );
	textLabel[1].SetText( "Very long text to test the string allocator." );
	textLabel[1].SetPos( 10, 20 );

	Image image0( &gamui, imageAtom, true );
	image0.SetPos( 50, 50 );

	TextBox block;
	block.Init( &gamui );
	block.SetPos( 50, 50 );
	block.SetSize( 100, 100 );
	block.SetText( "This is paragraph one.\n\nAnd number 2." );

	Image image1( &gamui, imageAtom, true );
	image1.SetPos( 50, 200 );
	image1.SetSize( 125, 125 );

	Image image2( &gamui, imageAtom, true );
	image2.SetPos( 200, 50 );
	image2.SetSize( 50, 50 );

	Image image2b( &gamui, imageAtom, true );
	image2b.SetPos( 270, 50 );
	image2b.SetSize( 50, 50 );

	Image image2c( &gamui, imageAtom, true );
	image2c.SetPos( 200, 120 );
	image2c.SetSize( 50, 50 );

	Image image2d( &gamui, imageAtom, true );
	image2d.SetPos( 270, 120 );
	image2d.SetSize( 50, 50 );

	Image image3( &gamui, imageAtom, true );
	image3.SetPos( 200, 200 );
	image3.SetSize( 125, 125 );
	image3.SetSlice( true );

	RenderAtom up( (const void*)RENDERSTATE_NORMAL, (const void*)imageTextureID, 0, 1, (52.f/256.f), (204.f/256.f), 50, 50 );
	RenderAtom upD = up;
	upD.renderState = (const void*) RENDERSTATE_DISABLED;

	RenderAtom down( (const void*)RENDERSTATE_NORMAL, (const void*)imageTextureID, 0, 0.75f, (52.f/256.f), (140.f/256.f), 50, 50 );
	RenderAtom downD( down, (const void*)RENDERSTATE_DISABLED );

	PushButton button0( &gamui, up, upD, down, downD, decoAtom, decoAtomD );
	button0.SetPos( 350, 50 );
	button0.SetSize( 150, 75 );
	button0.SetText( "Button" );

	PushButton button1( &gamui, up, upD, down, downD, decoAtom, decoAtomD );
	button1.SetPos( 350, 150 );
	button1.SetSize( 150, 75 );
	button1.SetText( "Button" );
	button1.SetEnabled( false );

	ToggleButton toggle( &gamui, up, upD, down, downD, decoAtom, decoAtomD );
	toggle.SetPos( 350, 250 );
	toggle.SetSize( 150, 50 );
	toggle.SetText( "Toggle" );
	toggle.SetText2( "Line 2" );

	ToggleButton toggle0( &gamui, up, upD, down, downD, decoAtom, decoAtomD );
	toggle0.SetPos( 350, 325 );
	toggle0.SetSize( 75, 50 );
	toggle0.SetText( "group" );

	ToggleButton toggle1( &gamui, up, upD, down, downD, decoAtom, decoAtomD );
	toggle1.SetPos( 430, 325 );
	toggle1.SetSize( 75, 50 );
	toggle1.SetText( "group" );

	ToggleButton toggle2( &gamui, up, upD, down, downD, decoAtom, decoAtomD );
	toggle2.SetPos( 510, 325 );
	toggle2.SetSize( 75, 50 );
	toggle2.SetText( "group" );

	toggle0.AddToToggleGroup( &toggle1 );
	toggle0.AddToToggleGroup( &toggle2 );

	RenderAtom tick0( (const void*)RENDERSTATE_NORMAL, (const void*)imageTextureID, 0, 0, 0, 0, 15, 30 );
	RenderAtom tick1=tick0, tick2=tick0;
	tick0.SetCoord( 190.f/256.f, 225.f/256.f, 205.f/256.f, 1 );
	tick2.SetCoord( 190.f/256.f, 180.f/256.f, 205.f/256.f, 210.f/256.f );
	tick1.SetCoord( 230.f/256.f, 225.f/256.f, 245.f/256.f, 1 );

	DigitalBar bar( &gamui, 10, tick0, tick1, tick2, 2 );
	bar.SetRange( 0.33f, 0.66f );
	//bar.SetRange( 0.5f, 1.0f );
	//bar.SetRange( 0.0f, 1.0f );
	bar.SetPos( 20, 350 );

	RenderAtom nullAtom;

	TiledImage<2, 2> tiled( &gamui );
	tiled.SetPos( 520, 20 );
	tiled.SetSize( 50, 50 );
	tiled.SetTile( 0, 0, tick1 );
	tiled.SetTile( 1, 0, nullAtom );
	tiled.SetTile( 0, 1, nullAtom );
	tiled.SetTile( 1, 1, tick0 );
	
	bool done = false;
	float range = 0.5f;
	while ( !done ) {
		SDL_Event event;
		if ( SDL_PollEvent( &event ) ) {
			switch( event.type ) {

				case SDL_KEYDOWN:
				{
					switch ( event.key.keysym.sym )
					{
						case SDLK_ESCAPE:
							done = true;
							break;
					}
				}
				break;

				case SDL_MOUSEBUTTONDOWN:
					gamui.TapDown( event.button.x, event.button.y );
					break;

				case SDL_MOUSEBUTTONUP:
					{
						const UIItem* item = gamui.TapUp( event.button.x, event.button.y );
						if ( item ) {
							range += 0.1f;
							if ( range > 1.0f )
								range = 0.0f;
							bar.SetRange( 0, range );
						}
					}
					break;

				case SDL_QUIT:
					done = true;
					break;
			}
		}

		glClearColor( 0, 0, 0, 1 );
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		float rotation = (float)((double)SDL_GetTicks() * 0.05 );
		image2b.SetRotationX( rotation );
		image2c.SetRotationY( rotation );
		image2d.SetRotationZ( rotation );
		gamui.Render();

		SDL_GL_SwapBuffers();
	}



	SDL_Quit();
	return 0;
}