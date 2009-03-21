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

#include "game.h"
#include "cgame.h"
#include "scene.h"
#include "battlescene.h"

#include "../engine/platformgl.h"
#include "../engine/text.h"
#include "../engine/model.h"
#include "../engine/uirendering.h"
#include "../engine/particle.h"

#include "../grinliz/glmatrix.h"
#include "../grinliz/glutil.h"

using namespace grinliz;

const char* const gModelNames[] = 
{
	"crate",
	
	// Characters
	"maleMarine",
	"femaleMarine",
	"alien0",
	"alien1",
	"alien2",
	"alien3",
	
	// mapmaker
	"selection",

	// Farmland tiles
	"wallWoodSh",
	"doorWoodOpen",
	"doorWoodCl",
	
	// UFO tiles
	"wallUFO",
	"diagUFO",
	"doorUFOOp",
	"doorUFOCl",
	"wallUFOInner",

	// decor
	"bed",
	"table",
	"tree",
	"wheat",

	0
};


struct TextureDef
{
	const char* name;
	U32 flags;
};

enum {
	ALPHA_TEST = 0x01,
};

const TextureDef gTextureDef[] = 
{
	{	"icons",		0	},
	{	"stdfont2",		0	},
	{	"woodDark",		0	},
	{	"woodDarkUFO",	0	},
	{	"woodPlank",	0	},
	{	"marine",		0	},
	{	"palette",		0	},
	{	"ufoOuter",		0	},
	{	"ufoInner",		0	},
	{	"tree",			ALPHA_TEST	},
	{	"wheat",		ALPHA_TEST	},
	{	"farmland",		0	},
	{	"particleQuad",	0	},
	{	"particleSparkle",	0	},
	{  0, 0 }
};


const char* gLightMapNames[ Game::NUM_LIGHT_MAPS ] = 
{
	"farmlandN",
};



Game::Game( int width, int height ) :
	engine( width, height, engineData ),
	rotation( 0 ),
	nTexture( 0 ),
	nModelResource( 0 ),
	markFrameTime( 0 ),
	frameCountsSinceMark( 0 ),
	framesPerSecond( 0 ),
	trianglesPerSecond( 0 ),
	trianglesSinceMark( 0 ),
	previousTime( 0 ),
	isDragging( false )
{
	surface.Set( 256, 256, 4 );		// All the memory we will ever need (? or that is the intention)

	LoadTextures();
	LoadModels();
	LoadMapResources();

	Texture* textTexture = GetTexture( "stdfont2" );
	GLASSERT( textTexture );
	UFOInitDrawText( textTexture->glID, engine.Width(), engine.Height(), rotation );

#ifdef MAPMAKER
#else
	// If we aren't the map maker, then we need to load a map.
	LoadMap( "farmland" );
#endif

	// Load the map.
	char buffer[512];
	for( int i=0; i<NUM_LIGHT_MAPS; ++i ) {
		PlatformPathToResource( gLightMapNames[i], "tex", buffer, 512 );
		FILE* fp = fopen( buffer, "rb" );
		GLASSERT( fp );
		lightMaps[i].LoadTexture( fp );
		fclose( fp );
	}

	engine.GetMap()->SetSize( 40, 40 );
	engine.GetMap()->SetTexture( GetTexture("farmland" ) );
	engine.GetMap()->SetLightMap( &lightMaps[0] );
	
	//engine.camera.SetPosWC( -19.4f, 62.0f, 57.2f );
	engine.camera.SetPosWC( -12.f, 45.f, 52.f );	// standard test
	//engine.camera.SetPosWC( -5.0f, engineData.cameraHeight, mz + 5.0f );

	particleSystem = new ParticleSystem();
	particleSystem->InitPoint( GetTexture( "particleSparkle" ) );
	particleSystem->InitQuad( GetTexture( "particleQuad" ) );

	scenes[BATTLE_SCENE] = new BattleScene( this );
	currentScene = scenes[BATTLE_SCENE];
}


Game::~Game()
{
	for( int i=0; i<NUM_SCENES; ++i ) {
		delete scenes[i];
	}

	delete particleSystem;
	FreeModels();
	FreeTextures();
}


void Game::LoadMapResources()
{
	Map* map = engine.GetMap();
	map->SetItemDef( 1, GetResource( "wallWoodSh" ) );
	map->SetItemDef( 2, GetResource( "doorWoodCl" ) );
	map->SetItemDef( 3, GetResource( "table" ) );
	map->SetItemDef( 4, GetResource( "bed" ) );
	map->SetItemDef( 5, GetResource( "diagUFO" ) );
	map->SetItemDef( 6, GetResource( "wallUFO" ) );
	map->SetItemDef( 7, GetResource( "doorUFOOp" ) );
	map->SetItemDef( 8, GetResource( "doorUFOCl" ) );
	map->SetItemDef( 9, GetResource( "wallUFOInner" ) );
	map->SetItemDef( 10, GetResource( "tree" ) );
	map->SetItemDef( 11, GetResource( "wheat" ) );
}


void Game::LoadTextures()
{
	memset( texture, 0, sizeof(Texture)*MAX_TEXTURES );

	U32 textureID = 0;
	FILE* fp = 0;
	char buffer[512];

	// Create the default texture "white"
	surface.Set( 2, 2, 2 );
	memset( surface.Pixels(), 255, 8 );
	textureID = surface.CreateTexture( false );
	texture[ nTexture++ ].Set( "white", textureID );

	// Load the textures from the array:
	for( int i=0; gTextureDef[i].name; ++i ) {
		PlatformPathToResource( gTextureDef[i].name, "tex", buffer, 512 );
		fp = fopen( buffer, "rb" );
		GLASSERT( fp );
		textureID = surface.LoadTexture( fp );
		bool alphaTest = (gTextureDef[i].flags & ALPHA_TEST ) ? true : false;
		texture[ nTexture++ ].Set( gTextureDef[i].name, textureID, alphaTest );
		fclose( fp );
	}
	GLASSERT( nTexture <= MAX_TEXTURES );
}


Surface* Game::GetLightMap( const char* name )
{
	for( int i=0; i<NUM_LIGHT_MAPS; ++i ) {
		if ( strcmp( name, gLightMapNames[i] ) == 0 ) {
			return &lightMaps[i];
		}
	}
	return 0;
}


void Game::SetScreenRotation( int value ) 
{
	rotation = ((unsigned)value)%4;

	UFOInitDrawText( 0, engine.Width(), engine.Height(), rotation );
	engine.camera.SetViewRotation( rotation );
}


ModelResource* Game::GetResource( const char* name )
{
	for( int i=0; gModelNames[i]; ++i ) {
		if ( strcmp( gModelNames[i], name ) == 0 ) {
			return &modelResource[i];
		}
	}
	GLASSERT( 0 );
	return 0;
}


Texture* Game::GetTexture( const char* name )
{
	for( int i=0; i<nTexture; ++i ) {
		if ( strcmp( texture[i].name, name ) == 0 ) {
			return &texture[i];
		}
	}
	GLASSERT( 0 );
	return 0;
}


void Game::LoadMap( const char* name )
{
	char buffer[512];

	PlatformPathToResource( name, "map", buffer, 512 );
	FILE* fp = fopen( buffer, "rb" );
	GLASSERT( fp );

	LoadMap( fp );
	fclose( fp );
}


void Game::LoadModels()
{
	ModelLoader* loader = new ModelLoader( texture, nTexture );
	memset( modelResource, 0, sizeof(ModelResource)*EL_MAX_MODEL_RESOURCES );

	FILE* fp = 0;
	char buffer[512];

	for( int i=0; gModelNames[i]; ++i ) {
		GLASSERT( i < EL_MAX_MODEL_RESOURCES );
		PlatformPathToResource( gModelNames[i], "mod", buffer, 512 );
		fp = fopen( buffer, "rb" );
		GLASSERT( fp );
		loader->Load( fp, &modelResource[i] );
		fclose( fp );
		nModelResource++;
	}
	delete loader;
}


void Game::FreeTextures()
{
	for( int i=0; i<nTexture; ++i ) {
		if ( texture[i].glID ) {
			glDeleteTextures( 1, (const GLuint*) &texture[i].glID );
			texture[i].name[0] = 0;
		}
	}
}


void Game::FreeModels()
{
	for( int i=0; i<nModelResource; ++i ) {
		for( U32 j=0; j<modelResource[i].header.nGroups; ++j ) {
			glDeleteBuffers( 1, (const GLuint*) &modelResource[i].atom[i].indexID );		
		}
		glDeleteBuffers( 1, (const GLuint*) &modelResource[i].atom[i].vertexID );
	}
}


void Game::DoTick( U32 currentTime )
{
	GLASSERT( currentTime > 0 );
	if ( previousTime == 0 ) {
		previousTime = currentTime-1;
	}
	U32 deltaTime = currentTime - previousTime;
	U32 previousTimeSec = previousTime / 1000;
	U32 currentTimeSec  = currentTime  / 1000;

	if ( markFrameTime == 0 ) {
		markFrameTime			= currentTime;
		frameCountsSinceMark	= 0;
		framesPerSecond			= 0.0f;
		trianglesPerSecond		= 0;
		trianglesSinceMark		= 0;
	}
	else {
		++frameCountsSinceMark;
		if ( currentTime - markFrameTime > 500 ) {
			framesPerSecond		= 1000.0f*(float)(frameCountsSinceMark) / ((float)(currentTime - markFrameTime));
			// actually K-tris/second
			trianglesPerSecond  = trianglesSinceMark / (currentTime - markFrameTime);
			markFrameTime		= currentTime;
			frameCountsSinceMark = 0;
			trianglesSinceMark = 0;
		}
	}

#ifdef EL_SHOW_MODELS
	for( int i=0; i<nModelResource; ++i ) {
		for ( unsigned k=0; k<modelResource[i].nGroups; ++ k ) {
			modelResource[i].atom[k].trisRendered = 0;
		}
	}
#endif

	if ( currentTimeSec != previousTimeSec ) {

		grinliz::Vector3F pos = { 10.0f, 1.0f, 28.0f };
		grinliz::Vector3F vel = { 0.0f, 1.0f, 0.0f };
		Color4F col = { 1.0f, -0.5f, 0.0f, 1.0f };
		Color4F colVel = { 0.0f, 0.0f, 0.0f, 0.0f };

		particleSystem->Emit(	ParticleSystem::POINT,
								0,		// type
								40,		// count
								ParticleSystem::PARTICLE_SPHERE,
								col,	colVel,
								pos,	0.1f,	
								vel,	0.1f,
								1200 );
	}
	grinliz::Vector3F pos = { 13.f, 0.0f, 28.0f };
	particleSystem->EmitFlame( deltaTime, pos );

	currentScene->DoTick( currentTime, deltaTime );

	glEnableClientState( GL_VERTEX_ARRAY );
	glEnableClientState( GL_NORMAL_ARRAY );
	glEnableClientState( GL_TEXTURE_COORD_ARRAY );

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable( GL_DEPTH_TEST );
	glEnable( GL_CULL_FACE );

	glMatrixMode( GL_MODELVIEW );
	glLoadIdentity();

	int triCount = 0;
	engine.Draw( &triCount );
	
	const grinliz::Vector3F* eyeDir = engine.camera.EyeDir3();
	particleSystem->Update( deltaTime );
	particleSystem->Draw( eyeDir );

	trianglesSinceMark += triCount;

	currentScene->DrawHUD();

	UFODrawText( 0,  0, "UFO Attack! %4.1ffps %5.1fK/f %4dK/s", 
				 framesPerSecond, 
				 (float)triCount/1000.0f,
				 trianglesPerSecond );

#ifdef EL_SHOW_MODELS
	int k=0;
	while ( k < nModelResource ) {
		int total = 0;
		for( unsigned i=0; i<modelResource[k].nGroups; ++i ) {
			total += modelResource[k].atom[i].trisRendered;
		}
		UFODrawText( 0, 12+12*k, "%16s %5d K", modelResource[k].name, total );
		++k;
	}
#endif

	glEnable( GL_DEPTH_TEST );
	previousTime = currentTime;
}


void Game::TransformScreen( int x0, int y0, int *x1, int *y1 )
{
	switch ( rotation ) {
		case 0:
			*x1 = x0;
			*y1 = y0;
			break;

		case 1:
			*x1 = y0;
			*y1 = engine.Width() - 1 - x0;
			break;

		case 2:
			*x1 = engine.Width() - 1 - x0;
			*y1 = y0;
			break;

		case 3:
			*x1 = engine.Height() - 1 - y0;
			*y1 = x0;
			break;

		default:
			GLASSERT( 0 );
			break;
	}
}


void Game::Tap( int tap, int x, int y )
{
	grinliz::Matrix4 mvpi;
	grinliz::Ray world;

	grinliz::Vector2I screen;
	TransformScreen( x, y, &screen.x, &screen.y );

	engine.CalcModelViewProjectionInverse( &mvpi );
	engine.RayFromScreen( x, y, mvpi, &world );

	currentScene->Tap( tap, screen, world );
}


void Game::Drag( int action, int x, int y )
{
	Vector2I screenRaw = { x, y };

	switch ( action ) 
	{
		case GAME_DRAG_START:
		{
			GLASSERT( !isDragging );
			isDragging = true;
			currentScene->Drag( action, screenRaw );
		}
		break;

		case GAME_DRAG_MOVE:
		{
			GLASSERT( isDragging );
			currentScene->Drag( action, screenRaw );
		}
		break;

		case GAME_DRAG_END:
		{
			GLASSERT( isDragging );
			currentScene->Drag( GAME_DRAG_MOVE, screenRaw );
			currentScene->Drag( GAME_DRAG_END, screenRaw );
			isDragging = false;
		}
		break;

		default:
			GLASSERT( 0 );
			break;
	}
}


void Game::Zoom( int action, int distance )
{
	currentScene->Zoom( action, distance );
}


void Game::CancelInput()
{
	isDragging = false;
}


#ifdef MAPMAKER

void Game::MouseMove( int x, int y )
{
	if ( currentScene == scenes[BATTLE_SCENE] ) {
		((BattleScene*)currentScene)->MouseMove( x, y );
	}
}

void Game::RotateSelection()
{
	if ( currentScene == scenes[BATTLE_SCENE] ) {
		((BattleScene*)currentScene)->RotateSelection();
	}
}

void Game::DeleteAtSelection()
{
	if ( currentScene == scenes[BATTLE_SCENE] ) {
		((BattleScene*)currentScene)->DeleteAtSelection();
	}
}


void Game::DeltaCurrentMapItem( int d )
{
	if ( currentScene == scenes[BATTLE_SCENE] ) {
		((BattleScene*)currentScene)->DeltaCurrentMapItem(d);
	}
}

#endif