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
#include "characterscene.h"
#include "tacticalintroscene.h"
#include "tacticalendscene.h"
#include "tacticalunitscorescene.h"
#include "helpscene.h"
#include "dialogscene.h"
#include "geoscene.h"
#include "geoendscene.h"
#include "basetradescene.h"
#include "buildbasescene.h"
#include "fastbattlescene.h"
#include "researchscene.h"
#include "settingscene.h"

#include "../engine/text.h"
#include "../engine/model.h"
#include "../engine/uirendering.h"
#include "../engine/particle.h"
#include "../engine/gpustatemanager.h"
#include "../engine/renderqueue.h"

#include "../grinliz/glmatrix.h"
#include "../grinliz/glutil.h"
#include "../grinliz/glperformance.h"
#include "../grinliz/glstringutil.h"
#include "../tinyxml/tinyxml.h"
#include "../version.h"

#include "ufosound.h"
#include "settings.h"

using namespace grinliz;

extern long memNewCount;

Game::Game( int width, int height, int rotation, const char* path ) :
	battleData( itemDefArr ),
	screenport( width, height, rotation ),
	markFrameTime( 0 ),
	frameCountsSinceMark( 0 ),
	framesPerSecond( 0 ),
	debugLevel( 0 ),
	suppressText( false ),
	previousTime( 0 ),
	isDragging( false )
{
	//GLRELASSERT( 0 );

	savePath = path;
	char c = savePath[savePath.size()-1];
	if ( c != '\\' && c != '/' ) {	
#ifdef WIN32
		savePath += "\\";
#else
		savePath += "/";
#endif
	}	
	
	Init();
	//Map* map = engine->GetMap();

	PushScene( INTRO_SCENE, 0 );
	PushPopScene();
}


// WARNING: strange map maker code
Game::Game( int width, int height, int rotation, const char* path, const TileSetDesc& base ) :
	battleData( itemDefArr ),
	screenport( width, height, rotation ),
	markFrameTime( 0 ),
	frameCountsSinceMark( 0 ),
	framesPerSecond( 0 ),
	debugLevel( 0 ),
	suppressText( false ),
	previousTime( 0 ),
	isDragging( false )
{
	GLASSERT( Engine::mapMakerMode == true );
	savePath = path;
	char c = savePath[savePath.size()-1];
	if ( c != '\\' && c != '/' ) {	
#ifdef WIN32
		savePath += '\\';
#else
		savePath += '/';
#endif
	}	
	
	Init();
	
	char buffer[128];
	SNPrintf( buffer, 128, "%4s_%2d_%4s_%02d", base.set, base.size, base.type, base.variation );

	mapmaker_xmlFile  = path;
	mapmaker_xmlFile += buffer;
	mapmaker_xmlFile += ".xml";

	GLString	texture  = buffer; 
				texture += "_TEX";
	GLString	dayMap   = buffer;
				dayMap  += "_DAY";
	GLString	nightMap  = buffer;
				nightMap += "_NGT";

	//engine->camera.SetPosWC( -25.f, 45.f, 30.f );	// standard test
	//engine->camera.SetYRotation( -60.f );

	PushScene( BATTLE_SCENE, 0 );
	PushPopScene();
	engine->GetMap()->SetSize( base.size, base.size );

	TiXmlDocument doc( mapmaker_xmlFile.c_str() );
	doc.LoadFile();
	if ( !doc.Error() )
		engine->GetMap()->Load( doc.FirstChildElement( "Map" ) );
}


void Game::Init()
{
	mapmaker_showPathing = 0;
	scenePopQueued = false;
//	sceneResetQueued = false;
	currentFrame = 0;
	surface.Set( Surface::RGBA16, 256, 256 );		// All the memory we will ever need (? or that is the intention)

	// Load the database.
	char buffer[260];
	int offset;
	int length;
	PlatformPathToResource( buffer, 260, &offset, &length );
	database = new gamedb::Reader();
#ifdef DEBUG	
	bool okay =
#endif
	database->Init( buffer, offset );
	GLASSERT( okay );

	SoundManager::Create( database );
	TextureManager::Create( database );
	ImageManager::Create( database );
	ModelResourceManager::Create();
	ParticleSystem::Create();
	SettingsManager::Create( savePath.c_str() );

	engine = new Engine( &screenport, database );

	LoadTextures();
	modelLoader = new ModelLoader();
	LoadModels();
	LoadItemResources();
	LoadAtoms();
	LoadPalettes();

	delete modelLoader;
	modelLoader = 0;

	Texture* textTexture = TextureManager::Instance()->GetTexture( "stdfont2" );
	GLASSERT( textTexture );
	UFOText::InitTexture( textTexture );
	UFOText::InitScreen( &screenport );

	faceSurface.Set( Surface::RGBA16, FaceGenerator::SIZE*MAX_TERRANS, FaceGenerator::SIZE );	// harwire sizes for face system
	oneFaceSurface.Set( Surface::RGBA16, FaceGenerator::SIZE, FaceGenerator::SIZE );
	memset( faceCache, 0, sizeof(FaceCache)*MAX_TERRANS );
	faceCacheSlot = 0;

	const gamedb::Item* node = database->Root()->Child( "textures" );
	GLASSERT( node );

	faceGen.chins.Load( node->Child( "faceChins" ));	
	faceGen.nChins = 17;
	faceGen.mouths.Load( node->Child( "faceMouths" ));
	faceGen.nMouths = 14;
	faceGen.noses.Load( node->Child( "faceNoses" ));
	faceGen.nNoses = 5;
	faceGen.hairs.Load( node->Child( "faceHairs" ));
	faceGen.nHairs = 17;
	faceGen.eyes.Load( node->Child( "faceEyes" ));
	faceGen.nEyes = 15;
	faceGen.glasses.Load( node->Child( "faceGlasses" ));
	faceGen.nGlasses = 5;
}


Game::~Game()
{
	if ( Engine::mapMakerMode ) {
		FILE* fp = fopen( mapmaker_xmlFile.c_str(), "w" );
		GLASSERT( fp );
		if ( fp ) {
			engine->GetMap()->Save( fp, 0 );
			fclose( fp );
		}
	}

	// Roll up to the main scene before saving.
	while( sceneStack.Size() > 1 ) {
		PopScene();
		PushPopScene();
	}

	sceneStack.Top()->scene->DeActivate();
	sceneStack.Top()->Free();
	sceneStack.Pop();

	float predicted, actual;
	WeaponItemDef::CurrentAccData( &predicted, &actual );
	GLOUTPUT(( "Game accuracy: predicted=%.2f actual=%.2f\n", predicted, actual ));

	delete engine;
	SettingsManager::Destroy();
	SoundManager::Destroy();
	ParticleSystem::Destroy();
	ModelResourceManager::Destroy();
	ImageManager::Destroy();
	TextureManager::Destroy();
	delete database;
}


bool Game::HasSaveFile( SavePathType type ) const
{
	bool result = false;

	FILE* fp = GameSavePath( type, SAVEPATH_READ );
	if ( fp ) {
		fseek( fp, 0, SEEK_END );
		long d = ftell( fp );
		if ( d > 100 ) {	// has to be something there: sanity check
			result = true;
		}
		fclose( fp );
	}
	return result;
}


void Game::DeleteSaveFile( SavePathType type )
{
	FILE* fp = GameSavePath( type, SAVEPATH_WRITE );
	if ( fp ) {
		fclose( fp );
	}
}


void Game::SceneNode::Free()
{
	sceneID = Game::NUM_SCENES;
	delete scene;	scene = 0;
	delete data;	data = 0;
	result = INT_MIN;
}


void Game::PushScene( int sceneID, SceneData* data )
{
	GLOUTPUT(( "PushScene %d\n", sceneID ));
	GLASSERT( sceneQueued.sceneID == NUM_SCENES );
	GLASSERT( sceneQueued.scene == 0 );

	sceneQueued.sceneID = sceneID;
	sceneQueued.data = data;
}


void Game::PopScene( int result )
{
	GLOUTPUT(( "PopScene result=%d\n", result ));
	GLASSERT( scenePopQueued == false );
	scenePopQueued = true;
	if ( result != INT_MAX )
		sceneStack.Top()->result = result;
}


void Game::PushPopScene() 
{
	if ( scenePopQueued || sceneQueued.sceneID != NUM_SCENES ) {
		TextureManager::Instance()->ContextShift();
	}

	if ( scenePopQueued )
	{
		sceneStack.Top()->scene->DeActivate();
		scenePopQueued = false;
		int result = sceneStack.Top()->result;
		int id     = sceneStack.Top()->sceneID;

		sceneStack.Top()->Free();
		sceneStack.Pop();

		if ( !sceneStack.Empty() ) {
			sceneStack.Top()->scene->Activate();
			if ( result != INT_MIN ) {
				sceneStack.Top()->scene->SceneResult( id, result );
			}
		}
	}

	if (    sceneQueued.sceneID == NUM_SCENES 
		 && sceneStack.Empty() ) 
	{
		// Unwind and full reset.
		delete engine;
		engine = new Engine( &screenport, database );
		DeleteSaveFile( SAVEPATH_GEO );
		DeleteSaveFile( SAVEPATH_TACTICAL );

		PushScene( INTRO_SCENE, 0 );
		PushPopScene();
	}
	else if ( sceneQueued.sceneID != NUM_SCENES ) 
	{
		GLASSERT( sceneQueued.sceneID < NUM_SCENES );

		if ( sceneStack.Size() ) {
			sceneStack.Top()->scene->DeActivate();
		}

		SceneNode* oldTop = 0;
		if ( !sceneStack.Empty() ) {
			oldTop = sceneStack.Top();
		}

		SceneNode* node = sceneStack.Push();
		CreateScene( sceneQueued, node );
		sceneQueued.data = 0;
		sceneQueued.Free();

		node->scene->Activate();

		if ( oldTop ) 
			oldTop->scene->ChildActivated( node->sceneID, node->scene, node->data );

		if (    node->scene->CanSave() 
			 && !Engine::mapMakerMode 
			 && sceneStack.Size() == 1 ) 
		{
			SavePathType savePath = node->scene->CanSave();
			FILE* fp = GameSavePath( savePath, SAVEPATH_READ );
			if ( fp ) {
				TiXmlDocument doc;
				doc.LoadFile( fp );
				//GLASSERT( !doc.Error() );
				if ( !doc.Error() ) {
					Load( doc );
				}
				fclose( fp );
			}
		}
	}
}


const Research* Game::GetResearch()
{
	for( SceneNode* node = sceneStack.BeginTop(); node; node = sceneStack.Next() ) {
		if ( node->sceneID == GEO_SCENE ) {
			return &((GeoScene*)node->scene)->GetResearch();
		}
	}
	return 0;
}


void Game::CreateScene( const SceneNode& in, SceneNode* node )
{
	Scene* scene = 0;
	switch ( in.sceneID ) {
		case BATTLE_SCENE:		battleData.Init(); scene = new BattleScene( this );									break;
		case CHARACTER_SCENE:	scene = new CharacterScene( this, (CharacterSceneData*)in.data );					break;
		case INTRO_SCENE:		scene = new TacticalIntroScene( this );												break;
		case END_SCENE:			scene = new TacticalEndScene( this );												break;
		case UNIT_SCORE_SCENE:	scene = new TacticalUnitScoreScene( this );											break;
		case HELP_SCENE:		scene = new HelpScene( this, (const HelpSceneData*)in.data );						break;
		case DIALOG_SCENE:		scene = new DialogScene( this, (const DialogSceneData*)in.data );					break;
		case GEO_SCENE:			scene = new GeoScene( this );														break;
		case GEO_END_SCENE:		scene = new GeoEndScene( this, (const GeoEndSceneData*)in.data );					break;
		case BASETRADE_SCENE:	scene = new BaseTradeScene( this, (BaseTradeSceneData*)in.data );					break;
		case BUILDBASE_SCENE:	scene = new BuildBaseScene( this, (BuildBaseSceneData*)in.data );					break;
		case FASTBATTLE_SCENE:	scene = new FastBattleScene( this, (BattleSceneData*)in.data );						break;
		case RESEARCH_SCENE:	scene = new ResearchScene( this, (ResearchSceneData*)in.data );						break;
		case SETTING_SCENE:		scene = new SettingScene( this );													break;
		default:
			GLASSERT( 0 );
			break;
	}
	node->scene = scene;
	node->sceneID = in.sceneID;
	node->data = in.data;
	node->result = INT_MIN;
}


const gamui::RenderAtom& Game::GetRenderAtom( int id )
{
	GLASSERT( id >= 0 && id < ATOM_COUNT );
	GLASSERT( renderAtoms[id].textureHandle );
	return renderAtoms[id];
}


const gamui::ButtonLook& Game::GetButtonLook( int id )
{
	GLASSERT( id >= 0 && id < LOOK_COUNT );
	return buttonLooks[id];
}


void Game::Load( const TiXmlDocument& doc )
{
	// Already pushed the BattleScene. Note that the
	// BOTTOM of the stack loads. (BattleScene or GeoScene).
	// A GeoScene will in turn load a BattleScene.
	const TiXmlElement* game = doc.RootElement();
	GLASSERT( StrEqual( game->Value(), "Game" ) );
	const TiXmlElement* scene = game->FirstChildElement();
	sceneStack.Top()->scene->Load( scene );
}


FILE* Game::GameSavePath( SavePathType type, SavePathMode mode ) const
{	
	grinliz::GLString str( savePath );
	if ( type == SAVEPATH_GEO )
		str += "geogame.xml";
	else if ( type == SAVEPATH_TACTICAL )
		str += "tacgame.xml";
	else
		GLASSERT( 0 );

	FILE* fp = fopen( str.c_str(), (mode == SAVEPATH_WRITE) ? "wb" : "rb" );
	return fp;
}


void Game::Save()
{
	// For loading, the BOTTOM loads and then loads higher scenes.
	// For saving, the GeoScene saves itself before pushing the tactical
	// scene, so save from the top back. (But still need to save if
	// we are in a character scene for example.)
	for( SceneNode* node=sceneStack.BeginTop(); node; node=sceneStack.Next() ) {
		if ( node->scene->CanSave() ) {
			FILE* fp = GameSavePath( node->scene->CanSave(), SAVEPATH_WRITE );
			GLASSERT( fp );
			if ( fp ) {
				XMLUtil::OpenElement( fp, 0, "Game" );
				XMLUtil::Attribute( fp, "version", VERSION );
				XMLUtil::Attribute( fp, "sceneID", node->sceneID );
				XMLUtil::SealElement( fp );

				node->scene->Save( fp, 1 );
	
				XMLUtil::CloseElement( fp, 0, "Game" );

				fclose( fp );
			}
			break;
		}
	}
}


void Game::DoTick( U32 _currentTime )
{
	{
		GRINLIZ_PERFTRACK

		currentTime = _currentTime;
		if ( previousTime == 0 ) {
			previousTime = currentTime-1;
		}
		U32 deltaTime = currentTime - previousTime;

		if ( markFrameTime == 0 ) {
			markFrameTime			= currentTime;
			frameCountsSinceMark	= 0;
			framesPerSecond			= 0.0f;
		}
		else {
			++frameCountsSinceMark;
			if ( currentTime - markFrameTime > 500 ) {
				framesPerSecond		= 1000.0f*(float)(frameCountsSinceMark) / ((float)(currentTime - markFrameTime));
				// actually K-tris/second
				markFrameTime		= currentTime;
				frameCountsSinceMark = 0;
			}
		}

		// Limit so we don't ever get big jumps:
		if ( deltaTime > 100 )
			deltaTime = 100;

		GPUShader::ResetState();
		GPUShader::Clear();

		Scene* scene = sceneStack.Top()->scene;
		scene->DoTick( currentTime, deltaTime );

		Rectangle2I clip2D, clip3D;
		int renderPass = scene->RenderPass( &clip3D, &clip2D );
		GLASSERT( renderPass );
	
		if ( renderPass & Scene::RENDER_3D ) {
			GRINLIZ_PERFTRACK_NAME( "Game::DoTick 3D" );
			//	r.Set( 100, 50, 300, 50+200*320/480 );
			//	r.Set( 100, 50, 300, 150 );
			screenport.SetPerspective(	//2.f, 
										//240.f, 
										//(EL_FOV*0.5f)*(screenport.UIWidth()/screenport.UIHeight())*320.0f/480.0f, 
										clip3D.IsValid() ? &clip3D : 0 );

			engine->Draw();
			if ( mapmaker_showPathing ) {
				engine->GetMap()->DrawPath( mapmaker_showPathing );
			}
			scene->Debug3D();
		
			const grinliz::Vector3F* eyeDir = engine->camera.EyeDir3();
			ParticleSystem* particleSystem = ParticleSystem::Instance();
			particleSystem->Update( deltaTime, currentTime );
			particleSystem->Draw( eyeDir, engine->GetMap() ? &engine->GetMap()->GetFogOfWar() : 0 );
		}

		{
			GRINLIZ_PERFTRACK_NAME( "Game::DoTick UI" );

			// UI Pass
			screenport.SetUI( clip2D.IsValid() ? &clip2D : 0 ); 
			if ( renderPass & Scene::RENDER_3D ) {
				scene->RenderGamui3D();
			}
			if ( renderPass & Scene::RENDER_2D ) {
				screenport.SetUI( clip2D.IsValid() ? &clip2D : 0 );
				scene->DrawHUD();
				scene->RenderGamui2D();
			}
		}
//		SoundManager::Instance()->PlayQueuedSounds();
	}

	const int Y = 305;
	#ifndef GRINLIZ_DEBUG_MEM
	const int memNewCount = 0;
	#endif
#if 1
	if ( !suppressText ) {
		if ( debugLevel >= 1 ) {
			UFOText::Draw(	0,  Y, "#%d %5.1ffps vbo=%d ps=%d", 
							VERSION, 
							framesPerSecond, 
							GPUShader::SupportsVBOs() ? 1 : 0,
							PointParticleShader::IsSupported() ? 1 : 0 );
		}
		if ( debugLevel >= 2 ) {
			UFOText::Draw(	0,  Y-15, "%4.1fK/f %3ddc/f", 
							(float)GPUShader::TrianglesDrawn()/1000.0f,
							GPUShader::DrawCalls() );
		}
		if ( debugLevel >= 3 ) {
			if ( !Engine::mapMakerMode )  {
				UFOText::Draw(  0, Y-30, "new=%d Tex(%d/%d) %dK/%dK mis=%d reuse=%d hit=%d",
								memNewCount,
								TextureManager::Instance()->NumTextures(),
								TextureManager::Instance()->NumGPUResources(),
								TextureManager::Instance()->CalcTextureMem()/1024,
								TextureManager::Instance()->CalcGPUMem()/1024,
								TextureManager::Instance()->CacheMiss(),
								TextureManager::Instance()->CacheReuse(),
								TextureManager::Instance()->CacheHit() );		
			}
		}
	}
#endif
	GPUShader::ResetTriCount();

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

#ifdef GRINLIZ_PROFILE
	const int SAMPLE = 8;
	if ( (currentFrame & (SAMPLE-1)) == 0 ) {
		Performance::SampleData();
	}
	for( int i=0; i<Performance::NumData(); ++i ) {
		const PerformanceData& data = Performance::GetData( i );

		UFOText::Draw( 60,  20+i*12, "%s", data.name );
		UFOText::Draw( 300, 20+i*12, "%.3f", data.normalTime );
		UFOText::Draw( 380, 20+i*12, "%d", data.functionCalls/SAMPLE );
	}
#endif

	previousTime = currentTime;
	++currentFrame;

	PushPopScene();
}


bool Game::PopSound( int* offset, int* size )
{
	return SoundManager::Instance()->PopSound( offset, size );
}


void Game::Tap( int action, int wx, int wy )
{
	// The tap is in window coordinate - need to convert to view.
	Vector2F window = { (float)wx, (float)wy };
	Vector2F view;
	screenport.WindowToView( window, &view );

	grinliz::Ray world;
	screenport.ViewToWorld( view, 0, &world );

#if 0
	{
		Vector2F ui;
		screenport.ViewToUI( view, &ui );
		if ( action != GAME_TAP_MOVE )
			GLOUTPUT(( "Tap: action=%d window(%.1f,%.1f) view(%.1f,%.1f) ui(%.1f,%.1f)\n", action, window.x, window.y, view.x, view.y, ui.x, ui.y ));
	}
#endif
	sceneStack.Top()->scene->Tap( action, view, world );
}


void Game::MouseMove( int x, int y )
{
	GLASSERT( Engine::mapMakerMode );
	((BattleScene*)sceneStack.Top()->scene)->MouseMove( x, y );
}



void Game::Zoom( int style, float distance )
{
	sceneStack.Top()->scene->Zoom( style, distance );
}


void Game::Rotate( float degrees )
{
	sceneStack.Top()->scene->Rotate( degrees );
}


void Game::CancelInput()
{
	isDragging = false;
}


void Game::HandleHotKeyMask( int mask )
{
	sceneStack.Top()->scene->HandleHotKeyMask( mask );
	if ( mask & GAME_HK_TOGGLE_DEBUG_TEXT ) {
		SetDebugLevel( GetDebugLevel() + 1 );
	}
}


void Game::DeviceLoss()
{
	TextureManager::Instance()->DeviceLoss();
	ModelResourceManager::Instance()->DeviceLoss();
	GPUShader::ResetState();
}


void Game::Resize( int width, int height, int rotation ) 
{
	screenport.Resize( width, height, rotation );
}


void Game::RotateSelection( int delta )
{
	((BattleScene*)sceneStack.Top()->scene)->RotateSelection( delta );
}

void Game::DeleteAtSelection()
{
	((BattleScene*)sceneStack.Top()->scene)->DeleteAtSelection();
}


void Game::DeltaCurrentMapItem( int d )
{
	((BattleScene*)sceneStack.Top()->scene)->DeltaCurrentMapItem(d);
}


void Game::SetLightMap( float r, float g, float b )
{
	((BattleScene*)sceneStack.Top()->scene)->SetLightMap( r, g, b );
}


int BattleData::CalcResult() const
{
	int nTerransAlive = Unit::Count( units+TERRAN_UNITS_START, MAX_TERRANS, Unit::STATUS_ALIVE );
	int nAliensAlive  = Unit::Count( units+ALIEN_UNITS_START, MAX_ALIENS, Unit::STATUS_ALIVE );

	int result = TIE;
	if ( nTerransAlive > 0 && nAliensAlive == 0 )
		result = VICTORY;
	else if ( nTerransAlive == 0 && nAliensAlive > 0 )
		result = DEFEAT;
	return result;
}


void BattleData::Save( FILE* fp, int depth )
{
	XMLUtil::OpenElement( fp, depth, "BattleData" );
	XMLUtil::Attribute( fp, "dayTime", dayTime );
	XMLUtil::Attribute( fp, "scenario", scenario );
	XMLUtil::SealElement( fp );

	storage.Save( fp, depth+1 );

	XMLUtil::OpenElement( fp, depth+1, "Units" );
	XMLUtil::SealElement( fp );
	for( int i=0; i<MAX_UNITS; ++i ) {
		units[i].Save( fp, depth+2 );
	}
	XMLUtil::CloseElement( fp, depth+1, "Units" );
	XMLUtil::CloseElement( fp, depth, "BattleData" );
}


void BattleData::Load( const TiXmlElement* doc )
{
	for( int i=0; i<MAX_UNITS; ++i )
		units[i].Free();
	storage.Clear();

	const TiXmlElement* ele = doc->FirstChildElement( "BattleData" );
	GLASSERT( ele );

	ele->QueryBoolAttribute( "dayTime", &dayTime );
	ele->QueryIntAttribute( "scenario", &scenario );

	storage.Load( ele );
	const TiXmlElement* unitsEle = ele->FirstChildElement( "Units" );
	GLASSERT( unitsEle );

	int team[3] = { TERRAN_UNITS_START, CIV_UNITS_START, ALIEN_UNITS_START };
	if ( unitsEle ) {
		for( const TiXmlElement* unitElement = unitsEle->FirstChildElement( "Unit" );
			 unitElement;
			 unitElement = unitElement->NextSiblingElement( "Unit" ) ) 
		{
			int t = 0;
			unitElement->QueryIntAttribute( "team", &t );
			Unit* unit = &units[team[t]];

			unit->Load( unitElement, storage.GetItemDefArr() );
			
			team[t]++;

			GLRELASSERT( team[0] <= TERRAN_UNITS_END );
			GLRELASSERT( team[1] <= CIV_UNITS_END );
			GLRELASSERT( team[2] <= ALIEN_UNITS_END );
		}
	}
}
