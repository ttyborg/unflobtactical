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

#include "battlescene.h"
#include "characterscene.h"
#include "game.h"
#include "cgame.h"

#include "../engine/uirendering.h"
#include "../engine/platformgl.h"
#include "../engine/particle.h"
#include "../engine/text.h"

#include "battlestream.h"
#include "ai.h"
#include "tacticalendscene.h"

#include "../grinliz/glfixed.h"
#include "../micropather/micropather.h"
#include "../grinliz/glstringutil.h"
#include "../grinliz/glgeometry.h"

#include "../tinyxml/tinyxml.h"
#include "ufosound.h"

using namespace grinliz;
using namespace gamui;

//#define REACTION_FIRE_EVENT_ONLY

TacticalEndSceneData gTacticalData;	// cheating. Just a block of memory to pass to tactical.
									// can't be on stack, don't want to fight headers to
									// put in class.

BattleScene::BattleScene( Game* game ) : Scene( game ), m_targets( units )
{
	subTurnIndex = subTurnCount = 0;
	rotationUIOn = nextUIOn = true;

	engine  = game->engine;
	visibility.Init( this, units, engine->GetMap() );
	uiMode = UIM_NORMAL;
	nearPathState.Clear();

//	memset( hpBarsFadeTime, 0, sizeof( int )*MAX_UNITS );
	engine->GetMap()->SetPathBlocker( this );

	aiArr[ALIEN_TEAM]		= new WarriorAI( ALIEN_TEAM, engine->GetSpaceTree() );
	aiArr[TERRAN_TEAM]		= 0;
	// FIXME: use warrior AI for now...
	aiArr[CIV_TEAM]			= new WarriorAI( CIV_TEAM, engine->GetSpaceTree() );

#ifdef MAPMAKER
	currentMapItem = 1;
#endif

	// On screen menu.
	const Screenport& port = engine->GetScreenport();

	const ButtonLook& green = game->GetButtonLook( Game::GREEN_BUTTON );
	const ButtonLook& blue = game->GetButtonLook( Game::BLUE_BUTTON );
	const ButtonLook& red = game->GetButtonLook( Game::RED_BUTTON );

	alienImage.Init( &gamui3D, UIRenderer::CalcDecoAtom( DECO_ALIEN ) );
	alienImage.SetPos( float(port.UIWidth()-50), 0 );
	alienImage.SetSize( 50, 50 );

	nameRankUI.Init( &gamui3D );

	gamui::RenderAtom nullAtom;
	for( int i=0; i<MAX_UNITS; ++i ) {
		unitImage0[i].Init( &engine->GetMap()->overlay0, nullAtom );
		unitImage0[i].SetVisible( false );

		unitImage1[i].Init( &engine->GetMap()->overlay1, nullAtom );
		unitImage1[i].SetVisible( false );
	}
	selectionImage.Init( &engine->GetMap()->overlay1, UIRenderer::CalcIconAtom( ICON_STAND_HIGHLIGHT ) );
	selectionImage.SetForeground( true );	// don't want to overlap with unitImage1
	selectionImage.SetSize( 1, 1 );

	for( int i=0; i<MAX_ALIENS; ++i ) {
		targetArrow[i].Init( &gamui2D, UIRenderer::CalcIconAtom( ICON_TARGET_POINTER ) );
		targetArrow[i].SetVisible( false );
	}

	const float SIZE = 50.0f;
	{
		exitButton.Init( &gamui2D, blue );
		exitButton.SetDeco( UIRenderer::CalcDecoAtom( DECO_LAUNCH, true ), UIRenderer::CalcDecoAtom( DECO_LAUNCH, false ) );
		exitButton.SetSize( SIZE, SIZE );

		helpButton.Init( &gamui2D, green );
		helpButton.SetDeco( UIRenderer::CalcDecoAtom( DECO_HELP, true ), UIRenderer::CalcDecoAtom( DECO_HELP, false ) );
		helpButton.SetSize( SIZE, SIZE );

		nextTurnButton.Init( &gamui2D, green );
		nextTurnButton.SetDeco( UIRenderer::CalcDecoAtom( DECO_END_TURN, true ), UIRenderer::CalcDecoAtom( DECO_END_TURN, false ) );
		nextTurnButton.SetSize( SIZE, SIZE );

		targetButton.Init( &gamui2D, 0, red );
		targetButton.SetDeco( UIRenderer::CalcDecoAtom( DECO_AIMED, true ), UIRenderer::CalcDecoAtom( DECO_AIMED, false ) );
		targetButton.SetSize( SIZE, SIZE );

		invButton.Init( &gamui2D, green );
		invButton.SetDeco( UIRenderer::CalcDecoAtom( DECO_CHARACTER, true ), UIRenderer::CalcDecoAtom( DECO_CHARACTER, false ) );
		invButton.SetSize( SIZE, SIZE );

		invButton.Init( &gamui2D, green );
		invButton.SetDeco( UIRenderer::CalcDecoAtom( DECO_CHARACTER, true ), UIRenderer::CalcDecoAtom( DECO_CHARACTER, false ) );
		invButton.SetSize( SIZE, SIZE );

		static const int controlDecoID[CONTROL_BUTTON_COUNT] = { DECO_ROTATE_CCW, DECO_ROTATE_CW, DECO_PREV, DECO_NEXT };
		for( int i=0; i<CONTROL_BUTTON_COUNT; ++i ) {
			controlButton[i].Init( (i==0) ? &gamui2D : &gamui3D, green );
			controlButton[i].SetDeco( UIRenderer::CalcDecoAtom( controlDecoID[i], true ), UIRenderer::CalcDecoAtom( controlDecoID[i], false ) );
			controlButton[i].SetSize( SIZE, SIZE );
		}

		UIItem* items[6] = { &exitButton, &helpButton, &nextTurnButton, &targetButton, &invButton, &controlButton[0] };
		Gamui::Layout( items, 6, 1, 6, 0, 0, SIZE, (float)port.UIHeight(), 0 );

		controlButton[1].SetPos( SIZE, port.UIHeight()-SIZE );
		controlButton[2].SetPos( port.UIWidth()-SIZE*2.f, port.UIHeight()-SIZE );
		controlButton[3].SetPos( port.UIWidth()-SIZE*1.f, port.UIHeight()-SIZE );

		RenderAtom menuImageAtom( UIRenderer::RENDERSTATE_NORMAL, TextureManager::Instance()->GetTexture( "commandBarV" ), 0, 0, 1, 1, 50, 320 );
		menuImage.Init( &gamui2D, menuImageAtom );
	}

	
	for( int i=0; i<3; ++i ) {
		fireButton[i].Init( &gamui3D, red );
		fireButton[i].SetSize( 120.f, 60.f );
		fireButton[i].SetDeco( UIRenderer::CalcIconAtom( ICON_GREEN_WALK_MARK, true ), UIRenderer::CalcIconAtom( ICON_GREEN_WALK_MARK, false ) );
		fireButton[i].SetVisible( false );
		fireButton[i].SetText( " " );
		fireButton[i].SetText2( " " );
		fireButton[i].SetDecoLayout( gamui::Button::RIGHT, 25, 0 );
		fireButton[i].SetTextLayout( gamui::Button::LEFT, 20, 0 );
	}	

	{
		static const float W = 0.15f;
		static const float H = 0.15f;
		static const float S = 0.02f;

		gamui::RenderAtom tick0Atom = UIRenderer::CalcPaletteAtom( UIRenderer::PALETTE_BRIGHT_GREEN, W, H );
		tick0Atom.renderState = (const void*)UIRenderer::RENDERSTATE_OPAQUE;
		gamui::RenderAtom tick1Atom = UIRenderer::CalcPaletteAtom( UIRenderer::PALETTE_RED, W, H );
		tick1Atom.renderState = (const void*)UIRenderer::RENDERSTATE_OPAQUE;
		gamui::RenderAtom tick2Atom = UIRenderer::CalcPaletteAtom( UIRenderer::PALETTE_DARK_GREY, W, H );
		tick1Atom.renderState = (const void*)UIRenderer::RENDERSTATE_OPAQUE;

		for( int i=0; i<MAX_UNITS; ++i ) {
			hpBars[i].Init( &engine->GetMap()->overlay1, 5, tick0Atom, tick1Atom, tick2Atom, S );
			hpBars[i].SetVisible( false );
		}
	}
	engine->EnableMap( true );

#ifdef MAPMAKER
	{
		const ModelResource* res = ModelResourceManager::Instance()->GetModelResource( "selection" );
		mapSelection = engine->AllocModel( res );
		mapSelection->SetPos( 0.5f, 0.0f, 0.5f );
		preview = 0;
	}
#endif

	currentTeamTurn = ALIEN_TEAM;
	NextTurn();
}


BattleScene::~BattleScene()
{
	engine->GetMap()->SetPathBlocker( 0 );
	ParticleSystem::Instance()->Clear();

#if defined( MAPMAKER )
	if ( mapSelection ) 
		engine->FreeModel( mapSelection );
	if ( preview )
		engine->FreeModel( preview );
	//sqlite3_close( mapDatabase );
#endif

	for( int i=0; i<3; ++i )
		delete aiArr[i];
}


void BattleScene::Activate()
{
	engine->EnableMap( true );
}


void BattleScene::DeActivate()
{
//	engine->SetClip( 0 );
}


void BattleScene::InitUnits()
{
	int Z = engine->GetMap()->Height();
	Random random(1098305);
	random.Rand();
	random.Rand();

	Item gun0( game, "PST" ),
		 gun1( game, "RAY-1" ),
		 ar3( game, "AR-3P" ),
		 plasmaRifle( game, "PLS-2" ),
		 medkit( game, "Med" ),
		 armor( game, "ARM-1" ),
		 fuel( game, "Gel" ),
		 grenade( game, "RPG", 4 ),
		 autoClip( game, "AClip", 15 ),
		 cell( game, "Cell", 12 ),
		 tachyon( game, "Tach", 4 ),
		 clip( game, "Clip", 8 );

	for( int i=0; i<4; ++i ) {
		Vector2I pos = { (i*2)+10, Z-10 };
		Unit* unit = &units[TERRAN_UNITS_START+i];

		random.Rand();
		unit->Init( engine, game, Unit::STATUS_ALIVE, TERRAN_TEAM, 0, random.Rand() );

		Inventory* inventory = unit->GetInventory();

		if ( i == 0 ) {
			inventory->AddItem( gun0 );
		}
		else if ( (i & 1) == 0 ) {
			inventory->AddItem( ar3 );
			inventory->AddItem( gun0 );

			inventory->AddItem( autoClip );
			inventory->AddItem( grenade );
		}
		else {
			inventory->AddItem( plasmaRifle );

			inventory->AddItem( cell );
			inventory->AddItem( tachyon );
		}

		inventory->AddItem( Item( clip ));
		inventory->AddItem( Item( clip ));
		inventory->AddItem( medkit );
		inventory->AddItem( armor );

		unit->UpdateInventory();
		unit->SetMapPos( pos.x, pos.y );
		unit->SetYRotation( (float)(((i+2)%8)*45) );
	}
	
	const Vector2I alienPos[4] = { 
		{ 16, 21 }, {12, 16 }, { 30, 25 }, { 29, 30 }
	};
	for( int i=0; i<4; ++i ) {
		Unit* unit = &units[ALIEN_UNITS_START+i];

		unit->Init( engine, game, ALIEN_TEAM, Unit::STATUS_ALIVE, i&3, random.Rand() );
		Inventory* inventory = unit->GetInventory();
		if ( (i&1) == 0 ) {
			inventory->AddItem( gun1 );
			inventory->AddItem( cell );
			inventory->AddItem( tachyon );
		}
		else if ( i==1 ) {
			inventory->AddItem( plasmaRifle );
		}
		else {
			inventory->AddItem( plasmaRifle );
			inventory->AddItem( cell );
			inventory->AddItem( tachyon );
		}
		unit->UpdateInventory();
		unit->SetMapPos( alienPos[i] );
	}

	Storage* extraAmmo = engine->GetMap()->LockStorage( 12, 20 );
	if ( !extraAmmo )
		extraAmmo = new Storage();
	extraAmmo->AddItem( cell );
	extraAmmo->AddItem( cell );
	extraAmmo->AddItem( tachyon );
	engine->GetMap()->ReleaseStorage( 12, 20, extraAmmo, game->GetItemDefArr() );


	/*
	for( int i=0; i<4; ++i ) {
		Vector2I pos = { (i*2)+10, Z-12 };
		units[CIV_UNITS_START+i].Init( engine, game, CIV_TEAM, 0, random.Rand() );
		units[CIV_UNITS_START+i].SetMapPos( pos.x, pos.y );
	}
	*/
}


void BattleScene::NextTurn()
{
	currentTeamTurn = (currentTeamTurn==ALIEN_TEAM) ? TERRAN_TEAM : ALIEN_TEAM;	// FIXME: go to 3 state

	switch ( currentTeamTurn ) {
		case TERRAN_TEAM:
			GLOUTPUT(( "New Turn: Terran\n" ));
			for( int i=TERRAN_UNITS_START; i<TERRAN_UNITS_END; ++i )
				units[i].NewTurn();
			break;

		case ALIEN_TEAM:
			GLOUTPUT(( "New Turn: Alien\n" ));
			for( int i=ALIEN_UNITS_START; i<ALIEN_UNITS_END; ++i ) {
				units[i].NewTurn();

			}
			break;

		case CIV_TEAM:
			GLOUTPUT(( "New Turn: Civ\n" ));
			for( int i=CIV_UNITS_START; i<CIV_UNITS_END; ++i )
				units[i].NewTurn();
			break;

		default:
			GLASSERT( 0 );
			break;
	}

	// Allow the map to change (fire and smoke)
	Rectangle2I change;
	change.SetInvalid();
	engine->GetMap()->DoSubTurn( &change );
	visibility.InvalidateAll( change );

	// Since the map has changed:
	ProcessDoors();
	CalcTeamTargets();
	targetEvents.Clear();

	if ( aiArr[currentTeamTurn] ) {
		aiArr[currentTeamTurn]->StartTurn( units, m_targets );
	}
	else {
		if ( engine->GetMap()->GetLanderModel() )
			OrderNextPrev();
	}

}


void BattleScene::OrderNextPrev()
{
	const Model* lander = engine->GetMap()->GetLanderModel();
	GLASSERT( lander );

	Matrix2I mat, inv;
	mat.SetXZRotation( (int)lander->GetRotation() );
	mat.Invert( &inv );

	// Tricky problem. First sort into ordered by (x,z) buckets. Then
	// group "clumps" while keeping the bucket order.

	int	a_subTurnOrder[MAX_TERRANS];

	subTurnCount=0;
	for( int i=TERRAN_UNITS_START; i<TERRAN_UNITS_END; ++i ) {
		if ( units[i].IsAlive() ) {
			a_subTurnOrder[subTurnCount++] = i;
		}
	}
	if ( subTurnCount == 0 )
		return;

	// Yes, a bubble sort. But there are only 8 (possibly in the future
	// 16) soldiers to sort.
	//
	Vector2I v = { -1, MAP_SIZE };
	for( int j=0; j<(subTurnCount-1); ++j ) {
		Vector2I pos0 = inv * units[a_subTurnOrder[j]].Pos();
		int posScore0 = DotProduct( pos0, v );

		for( int i=j+1; i<subTurnCount; ++i ) {
			Vector2I posI = inv * units[a_subTurnOrder[i]].Pos();
			int posScoreI = DotProduct( posI, v );

			if ( posScoreI > posScore0 ) {
				Swap( &a_subTurnOrder[j], &a_subTurnOrder[i] );
			}
		}
	}


	Rectangle2I clump;
	const int FIRST_OUTSET = 2;
	const int ADD_OUTSET = 2;
	int count = 0;

	for( int j=0; j<subTurnCount; ++j ) {
		if ( a_subTurnOrder[j] >= 0 ) {
			subTurnOrder[count++] = a_subTurnOrder[j];

			clump.min = clump.max = units[ a_subTurnOrder[j] ].Pos();
			clump.Outset( FIRST_OUTSET );

			for( int i=j+1; i<subTurnCount; ++i ) {
				if ( a_subTurnOrder[i] >= 0 ) {
					if ( clump.Contains( units[ a_subTurnOrder[i] ].Pos() ) ) {
						subTurnOrder[count++] = a_subTurnOrder[i];

						Rectangle2I subClump;
						subClump.min = subClump.max = units[ a_subTurnOrder[i] ].Pos();
						a_subTurnOrder[i] = -1;

						subClump.Outset( ADD_OUTSET );
						clump.DoUnion( subClump );
					}
				}
			}
		}
	}
	GLASSERT( count == subTurnCount );
	subTurnIndex = subTurnCount;
}


void BattleScene::Save( TiXmlElement* doc )
{
	TiXmlElement* battleElement = new TiXmlElement( "BattleScene" );
	doc->LinkEndChild( battleElement );
	battleElement->SetAttribute( "currentTeamTurn", currentTeamTurn );
	battleElement->SetAttribute( "dayTime", engine->GetMap()->DayTime() ? 1 : 0 );

	TiXmlElement* mapElement = new TiXmlElement( "Map" );
	doc->LinkEndChild( mapElement );
	engine->GetMap()->Save( mapElement );

	TiXmlElement* unitsElement = new TiXmlElement( "Units" );
	doc->LinkEndChild( unitsElement );
	
	for( int i=0; i<MAX_UNITS; ++i ) {
		units[i].Save( unitsElement );
	}
}


void BattleScene::Load( const TiXmlElement* gameElement )
{
	// FIXME: Save/Load AI? Memory state is lost.

	selection.Clear();

	const TiXmlElement* battleElement = gameElement->FirstChildElement( "BattleScene" );
	if ( battleElement ) {
		battleElement->QueryIntAttribute( "currentTeamTurn", &currentTeamTurn );
		int daytime = 1;
		battleElement->QueryIntAttribute( "dayTime", &daytime );
		engine->GetMap()->SetDayTime( daytime ? true : false );
	}

	engine->GetMap()->Load( gameElement->FirstChildElement( "Map"), game->GetItemDefArr() );
	
	int team[3] = { TERRAN_UNITS_START, CIV_UNITS_START, ALIEN_UNITS_START };

	if ( gameElement->FirstChildElement( "Units" ) ) {
		for( const TiXmlElement* unitElement = gameElement->FirstChildElement( "Units" )->FirstChildElement( "Unit" );
			 unitElement;
			 unitElement = unitElement->NextSiblingElement( "Unit" ) ) 
		{
			int t = 0;
			unitElement->QueryIntAttribute( "team", &t );
			Unit* unit = &units[team[t]];
			unit->Load( unitElement, engine, game );
			
			team[t]++;

			GLASSERT( team[0] <= TERRAN_UNITS_END );
			GLASSERT( team[1] <= CIV_UNITS_END );
			GLASSERT( team[2] <= ALIEN_UNITS_END );
		}
	}
	

	engine->GetMap()->QueryAllDoors( &doors );

	ProcessDoors();
	CalcTeamTargets();
	targetEvents.Clear();

	if ( aiArr[currentTeamTurn] ) {
		aiArr[currentTeamTurn]->StartTurn( units, m_targets );
	}
	if ( engine->GetMap()->GetLanderModel() )
		OrderNextPrev();
}


void BattleScene::SetFogOfWar()
{
	if ( visibility.FogCheckAndClear() ) {
		grinliz::BitArray<Map::SIZE, Map::SIZE, 1>* fow = engine->GetMap()->LockFogOfWar();
		for( int j=0; j<MAP_SIZE; ++j ) {
			for( int i=0; i<MAP_SIZE; ++i ) {
				if ( visibility.TeamCanSee( TERRAN_TEAM, i, j ) )
					fow->Set( i, j );
				else
					fow->Clear( i, j );
			}
		}
		engine->GetMap()->ReleaseFogOfWar();
	}
}


void BattleScene::TestHitTesting()
{
	/*
	GRINLIZ_PERFTRACK
	for ( int i=0; i<MAX_UNITS; ++i ) {
		if ( units[i].Status() == Unit::STATUS_ALIVE ) {
			int cx = (int)units[i].GetModel()->X();
			int cz = (int)units[i].GetModel()->Z();
			const int DELTA = 10;

			Ray ray;
			ray.origin.Set( (float)cx+0.5f, 1.0f, (float)cz+0.5f );

			for( int x=cx-DELTA; x<cx+DELTA; ++x ) {
				for( int z=cz-DELTA; z<cz+DELTA; ++z ) {
					if ( x>=0 && z>=0 && x<engine->GetMap()->Width() && z<engine->GetMap()->Height() ) {

						Vector3F dest = {(float)x, 1.0f, (float)z};

						ray.direction = dest - ray.origin;
						ray.length = 1.0f;

						Vector3F intersection;
						engine->IntersectModel( ray, TEST_TRI, 0, 0, &intersection );
					}
				}
			}
		}
	}
	*/
}


int BattleScene::CenterRectIntersection(	const Vector2I& r,
											const Rectangle2I& rect,
											Vector2I* out )
{
	Vector2F center = { (float)(rect.min.x+rect.max.x)*0.5f, (float)(rect.min.y+rect.max.y)*0.5f };
	Vector2F rf = { (float)r.x, (float)r.y };
	Vector2F outf;

	for( int e=0; e<4; ++e ) {
		Vector2I p0, p1;
		rect.Edge( e, &p1, &p0 );
		Vector2F p0f = { (float)p0.x, (float)p0.y };
		Vector2F p1f = { (float)p1.x, (float)p1.y };

		float t0, t1;
		int result = IntersectLineLine( center, rf, 
										p0f, p1f, 
										&outf, &t0, &t1 );
		if (    result == grinliz::INTERSECT
			 && t0 >= 0 && t0 <= 1 && t1 >= 0 && t1 <= 1 ) 
		{
			out->Set( LRintf( outf.x ), LRintf( outf.y ) );
			return grinliz::INTERSECT;
		}
	}
	GLASSERT( 0 );
	return grinliz::REJECT;
}


int BattleScene::RenderPass( grinliz::Rectangle2I* clip3D, grinliz::Rectangle2I* clip2D )
{
#if defined( MAPMAKER )
	clip3D->SetInvalid();
	clip2D->SetInvalid();
	return RENDER_3D | RENDER_2D; 
#else
	//clip3D->Set( 100, 0, 400, 150 );
	//clip3D->Set( 100, 0, 100+240, 160 );
	//clip3D->SetInvalid();
	//clip3D->Set( 100, 0, 400, 250 );

	Vector2I size;
	size.x = LRintf( menuImage.Width() );
	size.y = LRintf( menuImage.Height() );
	const Screenport& port = engine->GetScreenport();

	clip3D->Set( size.x, 0, port.UIWidth()-1, port.UIHeight()-1 );
	clip2D->Set(0, 0, port.UIWidth()-1, port.UIHeight()-1 );
	return RENDER_3D | RENDER_2D | RENDER_2D_FLIPPED; 
#endif
}


void BattleScene::SetUnitOverlays()
{
	gamui::RenderAtom targetAtom0 = UIRenderer::CalcIconAtom( ICON_TARGET_STAND );
	gamui::RenderAtom targetAtom1 = targetAtom0;
	targetAtom0.renderState = (const void*)UIRenderer::RENDERSTATE_OPAQUE;

	gamui::RenderAtom greenAtom0 = UIRenderer::CalcIconAtom( ICON_GREEN_STAND_MARK );
	gamui::RenderAtom yellowAtom0 = UIRenderer::CalcIconAtom( ICON_YELLOW_STAND_MARK );
	gamui::RenderAtom orangeAtom0 = UIRenderer::CalcIconAtom( ICON_ORANGE_STAND_MARK );

	gamui::RenderAtom greenAtom1 = UIRenderer::CalcIconAtom( ICON_GREEN_STAND_MARK_OUTLINE );
	greenAtom1.renderState = (const void*)UIRenderer::RENDERSTATE_OPAQUE;
	gamui::RenderAtom yellowAtom1 = UIRenderer::CalcIconAtom( ICON_YELLOW_STAND_MARK_OUTLINE );
	yellowAtom1.renderState = (const void*)UIRenderer::RENDERSTATE_OPAQUE;
	gamui::RenderAtom orangeAtom1 = UIRenderer::CalcIconAtom( ICON_ORANGE_STAND_MARK_OUTLINE );
	orangeAtom1.renderState = (const void*)UIRenderer::RENDERSTATE_OPAQUE;

	const Unit* unitMoving = 0;
	if ( !actionStack.Empty() ) {
		if ( actionStack.Top().actionID == ACTION_MOVE ) {
			unitMoving = actionStack.Top().unit;
		}
	}
	
	static const float HP_DX = 0.10f;
	static const float HP_DY = 0.95f;

	for( int i=ALIEN_UNITS_START; i<ALIEN_UNITS_END; ++i ) {
		// layer 0 target arrow
		// layer 1 target arrow

		targetArrow[i-ALIEN_UNITS_START].SetVisible( false );
		unitImage0[i].SetVisible( false );
		unitImage1[i].SetVisible( false );
		hpBars[i].SetVisible( false );

		if ( unitMoving != &units[i] && units[i].IsAlive() && m_targets.TeamCanSee( TERRAN_TEAM, i ) ) {
			Vector3F p;
			units[i].CalcPos( &p );

			// Is the unit on screen? If so, put in a simple foot decal. Else
			// put in an "alien that way" decal.
			Vector2I r;	
			engine->GetScreenport().WorldToUI( p, &r );
			Rectangle2I uiBounds;
			
			engine->GetScreenport().UIBoundsClipped3D( &uiBounds );

			if ( uiBounds.Contains( r ) ) {
				unitImage0[i].SetVisible( true );
				unitImage0[i].SetAtom( targetAtom0 );
				unitImage0[i].SetSize( 1.2f, 1.2f );
				unitImage0[i].SetCenterPos( p.x, p.z );

				unitImage1[i].SetVisible( true );
				unitImage1[i].SetAtom( targetAtom1 );
				unitImage1[i].SetSize( 1.2f, 1.2f );
				unitImage1[i].SetCenterPos( p.x, p.z );

				hpBars[i].SetVisible( true );
				hpBars[i].SetPos( p.x + HP_DX - 0.5f, p.z + HP_DY - 0.5f );
				hpBars[i].SetRange( (float)units[i].HP()*0.01f, (float)units[i].GetStats().TotalHP()*0.01f );
			}
			else {
				targetArrow[i-ALIEN_UNITS_START].SetVisible( true );

				Vector2I center = { (uiBounds.min.x + uiBounds.max.x)/2,
									(uiBounds.min.y + uiBounds.max.y)/2 };
				Rectangle2I inset = uiBounds;
				const int EPS = 10;
				inset.Outset( -EPS );
				Vector2I intersection = { 0, 0 };
				CenterRectIntersection( r, inset, &intersection );

				targetArrow[i-ALIEN_UNITS_START].SetCenterPos( (float)intersection.x, (float)uiBounds.max.y-1-intersection.y );
				float angle = atan2( (float)(center.y-r.y), (float)(r.x-center.x) );
				angle = ToDegree( angle ) + 90.0f;	

				targetArrow[i-ALIEN_UNITS_START].SetRotationZ( angle );
				targetArrow[i-ALIEN_UNITS_START].SetVisible( true );
				targetArrow[i-ALIEN_UNITS_START].SetSize( 50, 50 );
			}
		}
	}

	for( int i=TERRAN_UNITS_START; i<TERRAN_UNITS_END; ++i ) {
		unitImage0[i].SetVisible( false );
		unitImage1[i].SetVisible( false );

		if ( unitMoving != &units[i] && units[i].IsAlive() ) {
			int remain = units[i].CalcWeaponTURemaining();
			Vector2I pos = units[i].Pos();

			if ( remain >= Unit::AUTO_SHOT ) {
				unitImage0[i].SetAtom( greenAtom0 );
				unitImage1[i].SetAtom( greenAtom1 );
			}
			else if ( remain == Unit::SNAP_SHOT ) {
				unitImage0[i].SetAtom( yellowAtom0 );
				unitImage1[i].SetAtom( yellowAtom1 );
			}
			else {
				unitImage0[i].SetAtom( orangeAtom0 );
				unitImage1[i].SetAtom( orangeAtom1 );
			}

			unitImage0[i].SetVisible( true );
			unitImage0[i].SetPos( (float)pos.x, (float)pos.y );
			unitImage0[i].SetSize( 1, 1 );

			unitImage1[i].SetVisible( true );
			unitImage1[i].SetPos( (float)pos.x, (float)pos.y );
			unitImage1[i].SetSize( 1, 1 );

			hpBars[i].SetVisible( true );
			hpBars[i].SetPos( (float)pos.x + HP_DX, (float)pos.y + HP_DY );
			hpBars[i].SetVisible( true );
			hpBars[i].SetRange( (float)units[i].HP()*0.01f, (float)units[i].GetStats().TotalHP()*0.01f );
		}
	}

	if ( SelectedSoldierUnit() && unitMoving != SelectedSoldierUnit() ) {
		Vector2I pos = SelectedSoldierUnit()->Pos();
		selectionImage.SetPos( (float)pos.x, (float)pos.y );
		selectionImage.SetVisible( true );
	}
	else {
		selectionImage.SetVisible( false );
	}
}


void BattleScene::DoTick( U32 currentTime, U32 deltaTime )
{
	TestHitTesting();
	engine->GetMap()->EmitParticles( deltaTime );

#if 0
		// Debug unit targets.
		for( int i=ALIEN_UNITS_START; i<ALIEN_UNITS_END; ++i ) {
			if ( targets.terran.alienTargets.IsSet( unitID-TERRAN_UNITS_START, i-ALIEN_UNITS_START ) ) {
				Vector3F p;
				units[i].CalcPos( &p );
				game->particleSystem->EmitDecal( ParticleSystem::DECAL_UNIT_SIGHT,
												 ParticleSystem::DECAL_BOTH,
												 p, ALPHA, 0 );	
			}
		}
		// Debug team targets.
		for( int i=ALIEN_UNITS_START; i<ALIEN_UNITS_END; ++i ) {
			if ( targets.terran.teamAlienTargets.IsSet( i-ALIEN_UNITS_START ) ) {
				Vector3F p;
				units[i].CalcPos( &p );
				game->particleSystem->EmitDecal( ParticleSystem::DECAL_TEAM_SIGHT,
												 ParticleSystem::DECAL_BOTH,
												 p, ALPHA, 0 );	
			}
		}
#endif


	int result = ProcessAction( deltaTime );
	SetFogOfWar();	// fast if nothing changed.	

	if ( result & STEP_COMPLETE ) {
		ProcessDoors();			// Not fast. Only calc when needed.
		CalcTeamTargets();
		StopForNewTeamTarget();
		DoReactionFire();
		targetEvents.Clear();	// All done! They don't get to carry on beyond the moment.
	}

	SetUnitOverlays();

	if ( result && EndCondition( &gTacticalData ) ) {
		game->PushScene( Game::END_SCENE, &gTacticalData );
	}
	else if	( currentTeamTurn == TERRAN_TEAM ) {
		if ( selection.soldierUnit && !selection.soldierUnit->IsAlive() ) {
			SetSelection( 0 );
		}
		if ( actionStack.Empty() && selection.soldierUnit ) {
			ShowNearPath( selection.soldierUnit );
		}
		if ( actionStack.Empty() ) {
			ShowNearPath( selection.soldierUnit );	// fast if it does nothing.
		}
		// Render the target (if it is on-screen)
		if ( HasTarget() ) {
			DrawFireWidget();
		}
		else {
			fireButton[0].SetVisible( false );
			fireButton[1].SetVisible( false );
			fireButton[2].SetVisible( false );
		}
	}
	else {
		if ( actionStack.Empty() ) {
			bool done = ProcessAI();
			if ( done )
				NextTurn();
		}
	}
}


void BattleScene::Debug3D()
{
	/*
	int start[2] = { TERRAN_UNITS_START, ALIEN_UNITS_START };
	int end[2]   = { TERRAN_UNITS_END, ALIEN_UNITS_END };

	for( int k=0; k<2; ++k ) {
		if ( units[start[k]].IsAlive() ) {
			Vector3F origin;
			units[start[k]].GetModel()->CalcTrigger( &origin );
			const Model* ignore[] = { units[start[k]].GetModel(), units[start[k]].GetWeaponModel(), 0 };

			for( int i=start[k]+1; i<end[k]; ++i ) {
				if ( !units[i].IsAlive() )
					continue;

				Vector3F target;
				units[i].GetModel()->CalcTarget( &target );

				Ray ray;
				ray.direction = target - origin;
				ray.origin = origin;

				Vector3F intersect;
				engine->IntersectModel( ray, TEST_TRI, 0, 0, ignore, &intersect );

				glDisableClientState( GL_NORMAL_ARRAY );
				glDisableClientState( GL_VERTEX_ARRAY );
				glDisableClientState( GL_TEXTURE_COORD_ARRAY );
				glDisable( GL_TEXTURE_2D );

				glLineWidth( 2.0f );

				glColor4f( 0, 1, 0, 1 );
				glBegin( GL_LINES );
				glVertex3f( origin.x, origin.y, origin.z );
				glVertex3f( intersect.x, intersect.y, intersect.z );
				glEnd();

				glColor4f( 1, 0, 0, 1 );
				glBegin( GL_LINES );
				glVertex3f( intersect.x, intersect.y, intersect.z );
				glVertex3f( target.x, target.y, target.z );
				glEnd();

				glEnable( GL_TEXTURE_2D );
				glEnableClientState( GL_NORMAL_ARRAY );
				glEnableClientState( GL_VERTEX_ARRAY );
				glEnableClientState( GL_TEXTURE_COORD_ARRAY );

			}
		}
	}
	*/
}


bool BattleScene::EndCondition( TacticalEndSceneData* data )
{
	memset( data, 0, sizeof( *data ) );
	data->units = units;

	for( int i=TERRAN_UNITS_START; i<TERRAN_UNITS_END; ++i ) {
		if ( units[i].InUse() )
			++data->nTerrans;
		if ( units[i].IsAlive() )
			++data->nTerransAlive;
	}
	for( int i=ALIEN_UNITS_START; i<ALIEN_UNITS_END; ++i ) {
		if ( units[i].InUse() )
			++data->nAliens;
		if ( units[i].IsAlive() )
			++data->nAliensAlive;
	}
	if ( data->nAliensAlive == 0 ||data->nTerransAlive == 0 )
		return true;
	return false;
}


void BattleScene::DrawHPBars()
{
	/*
	// A bit of code so the hp bar isn't up *all* the time. If selected, then show them all.
	// Else just show for 5 seconds after hp changes.
	//
	const int FADE_TIME = 5000;

	for( int i=0; i<MAX_UNITS; ++i ) {
		hpBarsFadeTime[i] = Max( hpBarsFadeTime[i] - (int)game->DeltaTime(), 0 );
		hpBars[i].SetVisible( false );

		if (    units[i].IsAlive() 
			 && units[i].GetModel() 
			 && ( units[i].Team() == TERRAN_TEAM || m_targets.TeamCanSee( TERRAN_TEAM, i ) ) ) 
		{

			const Screenport& port = engine->GetScreenport();

			const Vector3F& pos = units[i].GetModel()->Pos();
			Vector2I ui;
			port.WorldToUI( pos, &ui );
			Rectangle2I uiBounds;
			port.UIBoundsClipped3D( &uiBounds );

			// onscreen
			float fhp = (float)units[i].HP()/100.0f;

			if ( hpBars[i].GetRange0() != fhp ) {
				hpBars[i].SetRange( fhp, (float)units[i].GetStats().TotalHP()/100.0f );
				hpBarsFadeTime[i] = FADE_TIME;
			}
			hpBars[i].SetPos( (float)ui.x - hpBars[i].Width()*0.5f, 
							  (float)(port.UIHeight()-1-ui.y) + hpBars[i].Height()*1.5f ); 

			if ( ( &units[i] == SelectedSoldierUnit() ) || hpBarsFadeTime[i] > 0 ) {
				hpBars[i].SetVisible( true );
			}
		}
	}
	*/
}


void BattleScene::DrawFireWidget()
{
	GLASSERT( HasTarget() );
	GLASSERT( SelectedSoldierUnit() );

	Unit* unit = SelectedSoldierUnit();
	
	Vector3F target;
	if ( selection.targetPos.x >= 0 ) {
		target.Set( (float)selection.targetPos.x+0.5f, 1.0f, (float)selection.targetPos.y+0.5f );
	}
	else {
		selection.targetUnit->GetModel()->CalcTarget( &target );
	}

	Vector3F distVector = target - SelectedSoldierModel()->Pos();
	float distToTarget = distVector.Length();

	Inventory* inventory = unit->GetInventory();
	const WeaponItemDef* wid = unit->GetWeaponDef();

	float snapTU=0, autoTU=0, altTU=0;
	if ( wid ) 
		unit->AllFireTimeUnits( &snapTU, &autoTU, &altTU );

	for( int i=0; i<3; ++i ) {
		if ( wid && unit->CanFire( (WeaponMode)i ) )
		{
			float fraction, anyFraction, dptu, tu;

			unit->FireStatistics( (WeaponMode)i, distToTarget, &fraction, &anyFraction, &tu, &dptu );
			int nRounds = inventory->CalcClipRoundsTotal( wid->GetClipItemDef( (WeaponMode)i) );

			// Never show 100% in the UI:
			if ( fraction > 0.90f )
				fraction = 0.90f;
			if ( anyFraction > 0.90f )
				anyFraction = 0.90f;

			char buffer0[32];
			char buffer1[32];
			SNPrintf( buffer0, 32, "%s %d%%", wid->fireDesc[i], (int)LRintf( anyFraction*100.0f ) );
			SNPrintf( buffer1, 32, "%d/%d", wid->RoundsNeeded( (WeaponMode)i ), nRounds );

			fireButton[i].SetEnabled( true );
			fireButton[i].SetText( buffer0 );
			fireButton[i].SetText2( buffer1 );

			// Reflect the TU left.
			float tuAfter = unit->TU() - tu;
			int tuIndicator = ICON_ORANGE_WALK_MARK;

			if ( tuAfter >= autoTU ) {
				tuIndicator = ICON_GREEN_WALK_MARK;
			}
			else if ( tuAfter >= snapTU ) {
				tuIndicator = ICON_YELLOW_WALK_MARK;
			}
			else if ( tuAfter < snapTU ) {
				tuIndicator = ICON_ORANGE_WALK_MARK;
			}
			fireButton[i].SetDeco( UIRenderer::CalcIconAtom( tuIndicator, true ), UIRenderer::CalcIconAtom( tuIndicator, false ) );
		}				 
		else {			 
			fireButton[i].SetEnabled( false );
			fireButton[i].SetText( "[none]" );
			fireButton[i].SetText2( "" );
			RenderAtom nullAtom;
			fireButton[i].SetDeco( nullAtom, nullAtom );
		}
	}

	Vector2F r;
	engine->GetScreenport().WorldToScreen( target, &r );

	int uiX, uiY;
	const Screenport& port = engine->GetScreenport();
	port.ViewToUI( (int)r.x, (int)r.y, &uiX, &uiY );
	uiY = port.UIHeight()-1-uiY;

	const int DX = 10;

	// Make sure it fits on the screen.
	float w = fireButton[0].Width();
	float h = fireButton[0].Height()*3.f + (float)FIRE_BUTTON_SPACING*2.0f;
	float x = (float)(uiX+DX);
	float y = (float)(uiY) - h*0.5f;

	if ( x < 0 ) {
		x = 0;
	}
	else if ( x+w >= port.UIWidth() ) {
		x = port.UIWidth() - w;
	}
	if ( y < 0 ) {
		y = 0;
	}
	else if ( y+h >= port.UIHeight() ) {
		y = port.UIHeight() - h;
	}
	fireButton[0].SetPos( x, y );
	fireButton[1].SetPos( x, y+fireButton[0].Height()+(float)FIRE_BUTTON_SPACING );
	fireButton[2].SetPos( x, y+fireButton[0].Height()*2.0f+(float)FIRE_BUTTON_SPACING*2.0f );
	fireButton[0].SetVisible( true );
	fireButton[1].SetVisible( true );
	fireButton[2].SetVisible( true );
}


/*
void BattleScene::SetFireWidget()
{
	GLASSERT( SelectedSoldierUnit() );
	GLASSERT( SelectedSoldierModel() );

	Unit* unit = SelectedSoldierUnit();
	Item* item = unit->GetWeapon();

	Vector3F target;
	if ( selection.targetPos.x >= 0 ) {
		target.Set( (float)selection.targetPos.x+0.5f, 1.0f, (float)selection.targetPos.y+0.5f );
	}
	else {
		target = selection.targetUnit->GetModel()->Pos();
	}
	Vector3F distVector = target - SelectedSoldierModel()->Pos();
	float distToTarget = distVector.Length();

	Inventory* inventory = unit->GetInventory();
	const WeaponItemDef* wid = item->IsWeapon();
	float snapTU, autoTU, altTU;

	unit->AllFireTimeUnits( &snapTU, &autoTU, &altTU );

	for( int i=0; i<3; ++i ) {
		float tu = 0;

		if (    wid 
			 && unit->CanFire( (WeaponMode)i ) )
		{
			float fraction, anyFraction, dptu;

			unit->FireStatistics( (WeaponMode)i, distToTarget, &fraction, &anyFraction, &tu, &dptu );
			int nRounds = inventory->CalcClipRoundsTotal( wid->GetClipItemDef( (WeaponMode)i) );

			// Never show 100% in the UI:
			if ( fraction > 0.95f )
				fraction = 0.95f;
			if ( anyFraction > 0.98f )
				anyFraction = 0.98f;

			char buffer0[32];
			char buffer1[32];
			SNPrintf( buffer0, 32, "%s %d%%", wid->fireDesc[i], (int)LRintf( anyFraction*100.0f ) );
			SNPrintf( buffer1, 32, "%d/%d", wid->RoundsNeeded( (WeaponMode)i ), nRounds );

			fireButton[i].SetEnabled( true );
			fireButton[i].SetText( buffer0 );
			fireButton[i].SetText2( buffer1 );
		}				 
		else {			 
			fireButton[i].SetEnabled( false );
			fireButton[i].SetText( "" );
			fireButton[i].SetText2( "" );
		}
		
		// Reflect the TU left.
		float tuAfter = unit->TU() - tu;
		int tuIndicator = ICON_ORANGE_WALK_MARK;
		if ( tu != 0 && tuAfter >= autoTU ) {
			tuIndicator = ICON_GREEN_WALK_MARK;
		}
		else if ( tu != 0 && tuAfter >= snapTU ) {
			tuIndicator = ICON_YELLOW_WALK_MARK;
		}
		else if ( tu != 0 && tuAfter < snapTU ) {
			tuIndicator = ICON_ORANGE_WALK_MARK;
		}
		fireButton[i].SetDeco( UIRenderer::CalcIconAtom( tuIndicator, true ), UIRenderer::CalcIconAtom( tuIndicator, false ) );
	}
}
*/


void BattleScene::TestCoordinates()
{
	//const Screenport& port = engine->GetScreenport();
	Rectangle2I uiBounds;
	engine->GetScreenport().UIBoundsClipped3D( &uiBounds );
	Rectangle2I inset = uiBounds;
	inset.Outset( 0 );

	Matrix4 mvpi;
	engine->GetScreenport().ViewProjectionInverse3D( &mvpi );

	for( int i=0; i<4; ++i ) {
		Vector3F pos;
		Vector2I corner;
		uiBounds.Corner( i, &corner );
		if ( engine->RayFromScreenToYPlane( corner.x, corner.y, mvpi, 0, &pos ) ) {
			Color4F c = { 0, 1, 1, 1 };
			Color4F cv = { 0, 0, 0, 0 };
			ParticleSystem::Instance()->EmitOnePoint( c, cv, pos, 0 );
		}
	}
}


void BattleScene::PushScrollOnScreen( const Vector3F& pos )
{
	/*
		Centering is straightforward but not a great experience. Really want to scroll
		in to the nearest point. Do this by computing the desired point, offset it back,
		and add the difference.
	*/
	const Screenport& port = engine->GetScreenport();

	Vector2I r;
	port.WorldToUI( pos, &r );

	Rectangle2I uiBounds;
	port.UIBoundsClipped3D( &uiBounds );
	Rectangle2I inset = uiBounds;
	inset.Outset( -20 );

	if ( !inset.Contains( r ) ) 
	{

		Vector3F c;
		engine->CameraLookingAt( &c );

		Vector3F delta = pos - c;
		float len = delta.Length();
		delta.Normalize();

		Action action;
		action.Init( ACTION_CAMERA_BOUNDS, 0 );
		action.type.cameraBounds.target = pos;
		action.type.cameraBounds.normal = delta;
		action.type.cameraBounds.speed = (float)MAP_SIZE / 3.0f;
		actionStack.Push( action );
	}
}


void BattleScene::SetSelection( Unit* unit ) 
{
	if ( !unit ) {
		selection.soldierUnit = 0;
		selection.targetUnit = 0;
	}
	else {
		GLASSERT( unit->IsAlive() );

		if ( unit->Team() == TERRAN_TEAM ) {
			selection.soldierUnit = unit;
			selection.targetUnit = 0;
		}
		else if ( unit->Team() == ALIEN_TEAM ) {
			GLASSERT( SelectedSoldier() );
			selection.targetUnit = unit;
			selection.targetPos.Set( -1, -1 );

			GLASSERT( uiMode == UIM_NORMAL );
			uiMode = UIM_FIRE_MENU;
		}
		else {
			GLASSERT( 0 );
		}
	}
}


void BattleScene::PushRotateAction( Unit* src, const Vector3F& dst3F, bool quantize )
{
	GLASSERT( src->GetModel() );

	Vector2I dst = { (int)dst3F.x, (int)dst3F.z };

	float rot = src->AngleBetween( dst, quantize );
	if ( src->GetModel()->GetRotation() != rot ) {
		Action action;
		action.Init( ACTION_ROTATE, src );
		action.type.rotate.rotation = rot;
		actionStack.Push( action );
	}
}


bool BattleScene::PushShootAction( Unit* unit, const grinliz::Vector3F& target, 
								   WeaponMode mode,
								   float useError,
								   bool clearMoveIfShoot )
{
	GLASSERT( unit );

	if ( !unit->IsAlive() )
		return false;
	
	Item* weapon = unit->GetWeapon();
	if ( !weapon )
		return false;
	const WeaponItemDef* wid = weapon->IsWeapon();

	Vector3F normal, right, up, p;
	unit->CalcPos( &p );
	normal = target - p;
	float length = normal.Length();
	normal.Normalize();

	up.Set( 0.0f, 1.0f, 0.0f );
	CrossProduct( normal, up, &right );
	CrossProduct( normal, right, &up );

	if ( unit->CanFire( mode ) )
	{
		int nShots = wid->RoundsNeeded( mode );
		unit->UseTU( unit->FireTimeUnits( mode ) );

		for( int i=0; i<nShots; ++i ) {
			Vector3F t = target;
			if ( useError ) {
				float d = length * random.Uniform() * unit->GetStats().Accuracy() * useError;
				Matrix4 m;
				m.SetAxisAngle( normal, (float)random.Rand( 360 ) );
				t = target + (m * right)*d;
			}

			if ( clearMoveIfShoot && !actionStack.Empty() && actionStack.Top().actionID == ACTION_MOVE ) {
				actionStack.Clear();
			}

			Action action;
			action.Init( ACTION_SHOOT, unit );
			action.type.shoot.target = t;
			action.type.shoot.mode = mode;
			actionStack.Push( action );
			unit->GetInventory()->UseClipRound( wid->GetClipItemDef( mode ) );
		}
		PushRotateAction( unit, target, false );
		return true;
	}
	return false;
}


/*
bool BattleScene::LineOfSight( const Unit* shooter, const Unit* target )
{
	GLASSERT( shooter->GetModel() );
	GLASSERT( target->GetModel() );
	GLASSERT( shooter != target );

	Vector3F p0, p1, intersection;
	shooter->GetModel()->CalcTrigger( &p0 );
	target->GetModel()->CalcTarget( &p1 );
	// FIXME: handle 'no weapon' case
	const Model* ignore[5] = {	shooter->GetModel(), 
								shooter->GetWeaponModel(), 
								target->GetModel(), 
								target->GetWeaponModel(), 
								0 };

	Ray ray = { p0, p1-p0 };
	Model* m = engine->IntersectModel( ray, TEST_TRI, 0, 0, ignore, &intersection );
	if ( m == target->GetModel() )
		return true;
	return false;
}
*/


void BattleScene::DoReactionFire()
{
	int antiTeam = ALIEN_TEAM;
	if ( currentTeamTurn == ALIEN_TEAM )
		antiTeam = TERRAN_TEAM;

	bool react = false;
	if ( actionStack.Empty() ) {
		react = true;
	}
	else { 
		const Action& action = actionStack.Top();
		if (    action.actionID == ACTION_MOVE
			&& action.unit
			&& action.unit->Team() == currentTeamTurn
			&& action.type.move.pathFraction == 0 )
		{
			react = true;		
		}
	}
	if ( react ) {
		int i=0;
		while( i < targetEvents.Size() ) {
			TargetEvent t = targetEvents[i];

			// Reaction fire occurs on the *antiTeam*. It's a little
			// strange to get the ol' head wrapped around.
			if (    t.team == 0				// individual
				 && t.gain == 1 
				 && units[t.viewerID].Team() == antiTeam
				 && units[t.targetID].Team() == currentTeamTurn ) 
			{
				// Reaction fire
				Unit* targetUnit = &units[t.targetID];
				Unit* srcUnit = &units[t.viewerID];

				if (    targetUnit->IsAlive() 
					 && targetUnit->GetModel()
					 && srcUnit->GetWeapon() ) {

					bool rangeOK = true;
					// Filter out explosive weapons...
					if ( srcUnit->GetWeapon()->IsWeapon()->IsExplosive( kModeSnap ) ) {
						Vector2I d = srcUnit->Pos() - targetUnit->Pos();
						if ( d.LengthSquared() < EXPLOSIVE_RANGE * EXPLOSIVE_RANGE )
							rangeOK = false;
					}
					
					if ( rangeOK ) {
						// Do we really react? Are we that lucky? Well, are you, punk?
						float r = random.Uniform();
						float reaction = srcUnit->GetStats().Reaction();

						// Reaction is impacted by rotation.
						// Multiple ways to do the math. Go with the normal of the src facing to
						// the normal of the target.

						Vector2I targetMapPos = targetUnit->Pos();
						Vector2I srcMapPos = srcUnit->Pos();

						Vector2F normalToTarget = { (float)(targetMapPos.x - srcMapPos.x), (float)(targetMapPos.y - srcMapPos.y) };
						normalToTarget.Normalize();

						Vector2F facing;
						facing.x = sinf( ToRadian( srcUnit->GetModel()->GetRotation() ) );
						facing.y = cosf( ToRadian( srcUnit->GetModel()->GetRotation() ) );

						float mod = DotProduct( facing, normalToTarget ) * 0.5f + 0.5f;  
						reaction *= mod;				// linear with angle.
						float error = 2.0f - mod;		// doubles with rotation
						
						GLOUTPUT(( "reaction fire possible. (if %.2f < %.2f)\n", r, reaction ));

						if ( r <= reaction ) {
							Vector3F target;
							targetUnit->GetModel()->CalcTarget( &target );
							
							int shot = PushShootAction( srcUnit, target, kModeAuto, error, true );	// auto
							if ( !shot )
								PushShootAction( srcUnit, target, kModeSnap, error, true );	// snap
						}
				}
				}
				targetEvents.SwapRemove( i );
			}
			else {
				++i;
			}
		}
	}
}


bool BattleScene::ProcessAI()
{
	GLASSERT( actionStack.Empty() );
	bool allDone = true;
	int count = 0;

	while ( actionStack.Empty() ) {
		allDone = true;
		for( int i=ALIEN_UNITS_START; i<ALIEN_UNITS_END; ++i ) {
			if ( actionStack.Empty() && units[i].IsAlive() && !units[i].IsUserDone() ) {

				++count;
				int flags = (count&1) ? AI::AI_WANDER : 0;	// half wander (starting with 1st)
				flags |= ( units[i].AI() == Unit::AI_GUARD ) ? AI::AI_GUARD : 0;
				AI::AIAction aiAction;

				bool done = aiArr[ALIEN_TEAM]->Think( &units[i], units, m_targets, flags, engine->GetMap(), &aiAction );

				switch ( aiAction.actionID ) {
					case AI::ACTION_SHOOT:
						{
							bool shot = PushShootAction( &units[i], aiAction.shoot.target, aiAction.shoot.mode, 1.0f, false );
							GLASSERT( shot );
							if ( !shot )
								done = true;
						}
						break;

					case AI::ACTION_MOVE:
						{
							Action action;
							action.Init( ACTION_MOVE, &units[i] );
							action.type.move.path = aiAction.move.path;
							actionStack.Push( action );
						}
						break;

					case AI::ACTION_SWAP_WEAPON:
						{
//							Inventory* inv = units[i].GetInventory();
//							inv->SwapWeapons();
						}
						break;

					case AI::ACTION_PICK_UP:
						{
							const AI::PickUpAIAction& pick = aiAction.pickUp;

							Vector2I pos = units[i].Pos();
							Storage* storage = engine->GetMap()->LockStorage( pos.x, pos.y );
							GLASSERT( storage );
							Inventory* inventory = units[i].GetInventory();

							if ( storage ) {
								for( int k=0; k<pick.MAX_ITEMS; ++k ) {
									if ( pick.itemDefArr[k] ) {
										while ( storage->GetCount( pick.itemDefArr[k] ) ) {

											Item item;
											storage->RemoveItem( pick.itemDefArr[k], &item );
											if ( inventory->AddItem( item ) < 0 ) {
												// Couldn't add to inventory. Return to storage.
												storage->AddItem( item );
												// and don't try adding this item again...
												break;
											}
										}
									}
								}
								engine->GetMap()->ReleaseStorage( pos.x, pos.y, storage, game->GetItemDefArr() );
							}
						}
						break;

					case AI::ACTION_NONE:
						break;

					default:
						GLASSERT( 0 );
						break;
				}
				if ( done ) {
					units[i].SetUserDone();
				}
				if ( !units[i].IsUserDone() ) {
					allDone = false;
				}
			}
		}
		if ( allDone )
			break;
	}
	return allDone;
}


void BattleScene::ProcessDoors()
{
	bool doorChange = false;
	Rectangle2I invalid;
	invalid.SetInvalid();

	// Where are all the units? Then go through and set
	// each door. Setting a door to its current value
	// does nothing.
	BitArray< MAP_SIZE, MAP_SIZE, 1 > map;
	for( int i=0; i<MAX_UNITS; ++i ) {
		if ( units[i].IsAlive() ) {

			Vector2I pos;
			units[i].CalcMapPos( &pos, 0 );
			map.Set( pos.x, pos.y, 0 );
		}
	}

	for( int i=0; i<doors.Size(); ++i ) {
		const Vector2I& v = doors[i];
		if (    map.IsSet( v.x, v.y )
			 || map.IsSet( v.x+1, v.y )
			 || map.IsSet( v.x-1, v.y )
			 || map.IsSet( v.x, v.y+1 )
			 || map.IsSet( v.x, v.y-1 ) )
		{
			doorChange |= engine->GetMap()->OpenDoor( v.x, v.y, true );
			invalid.DoUnion( v );
		}
		else {
			doorChange |= engine->GetMap()->OpenDoor( v.x, v.y, false );
			invalid.DoUnion( v );
		}
	}
	if ( doorChange ) {
		visibility.InvalidateAll();
	}
}


void BattleScene::StopForNewTeamTarget()
{
	int antiTeam = ALIEN_TEAM;
	if ( currentTeamTurn == ALIEN_TEAM )
		antiTeam = TERRAN_TEAM;

	if ( actionStack.Size() == 1 ) {
		const Action& action = actionStack.Top();
		if (    action.actionID == ACTION_MOVE
			&& action.unit
			&& action.unit->Team() == currentTeamTurn
			&& action.type.move.pathFraction == 0 )
		{
			// THEN check for interuption.
			// Clear out the current team events, look for "new team"
			// Player: only pauses on "new team"
			// AI: pauses on "new team" OR "new target"
			int i=0;
			bool newTeam = false;
			while( i < targetEvents.Size() ) {
				TargetEvent t = targetEvents[i];
				if (	( aiArr[currentTeamTurn] || t.team == 1 )	// AI always pause. Players pause on new team.
					 && t.gain == 1 
					 && t.viewerID == currentTeamTurn )
				{
					// New sighting!
					GLOUTPUT(( "Sighting: Team %d sighted target %d on team %d.\n", t.viewerID, t.targetID, units[t.targetID].Team() ));
					newTeam = true;
					targetEvents.SwapRemove( i );
				}
				else {
					++i;
				}
			}
			if ( newTeam ) {
				actionStack.Clear();
			}
		}
	}
}


int BattleScene::ProcessAction( U32 deltaTime )
{
	int result = 0;

	if ( !actionStack.Empty() )
	{
		Action* action = &actionStack.Top();

		Unit* unit = 0;
		Model* model = 0;

		if ( action->unit ) {
			if ( !action->unit->IsAlive() || !action->unit->GetModel() ) {
				GLASSERT( 0 );	// may be okay, but untested.
				actionStack.Pop();
				return true;
			}
			unit = action->unit;
			model = action->unit->GetModel();
		}

		/*
		Vector2I originalPos = { 0, 0 };
		float originalRot = 0.0f;
		float originalTU = 0.0f;
		if ( unit ) {
			unit->CalcMapPos( &originalPos, &originalRot );
			originalTU = unit->GetStats().TU();
		}
		*/

		switch ( action->actionID ) {
			case ACTION_MOVE: 
				{
					// Move the unit. Be careful to never move more than one step (Travel() does not).
					// Used to do intermedia rotation, but it was annoying. Once vision was switched
					// to 360 it did nothing. So rotation is free now.
					//
					const float SPEED = 4.5f;
					float x, z, r;

					MoveAction* move = &action->type.move;
						
					move->path.GetPos( action->type.move.pathStep, move->pathFraction, &x, &z, &r );
					// Face in the direction of walking.
					model->SetRotation( r );

					float travel = Travel( deltaTime, SPEED );

					while(    (move->pathStep < move->path.pathLen-1 )
						   && travel > 0.0f ) 
					{
						move->path.Travel( &travel, &move->pathStep, &move->pathFraction );
						if ( move->pathFraction == 0.0f ) {
							// crossed a path boundary.
							GLASSERT( unit->TU() >= 1.0 );	// one move is one TU
							
							Vector2<S16> v0 = move->path.GetPathAt( move->pathStep-1 );
							Vector2<S16> v1 = move->path.GetPathAt( move->pathStep );
							int d = abs( v0.x-v1.x ) + abs( v0.y-v1.y );

							if ( d == 1 )
								unit->UseTU( 1.0f );
							else if ( d == 2 )
								unit->UseTU( 1.41f );
							else { GLASSERT( 0 ); }

							visibility.InvalidateUnit( unit-units );
							result |= STEP_COMPLETE;
							break;
						}
						move->path.GetPos( move->pathStep, move->pathFraction, &x, &z, &r );

						Vector3F v = { x+0.5f, 0.0f, z+0.5f };
						unit->SetPos( v, model->GetRotation() );
					}
					if ( move->pathStep == move->path.pathLen-1 ) {
						actionStack.Pop();
						visibility.InvalidateUnit( unit-units );
						result |= STEP_COMPLETE | UNIT_ACTION_COMPLETE;
					}
				}
				break;

			case ACTION_ROTATE:
				{
					const float ROTSPEED = 400.0f;
					float travel = Travel( deltaTime, ROTSPEED );

					float delta, bias;
					MinDeltaDegrees( model->GetRotation(), action->type.rotate.rotation, &delta, &bias );

					if ( delta <= travel ) {
						unit->SetYRotation( action->type.rotate.rotation );
						actionStack.Pop();
						result |= UNIT_ACTION_COMPLETE;
					}
					else {
						unit->SetYRotation( model->GetRotation() + bias*travel );
					}
				}
				break;

			case ACTION_SHOOT:
				{
					int r = ProcessActionShoot( action, unit, model );
					result |= r;
				}
				break;

			case ACTION_HIT:
				{
					int r = ProcessActionHit( action );
					result |= r;
				}
				break;

			case ACTION_DELAY:
				{
					if ( deltaTime >= action->type.delay.delay ) {
						actionStack.Pop();
						result |= OTHER_ACTION_COMPLETE;
					}
					else {
						action->type.delay.delay -= deltaTime;
					}
				}
				break;

			case ACTION_CAMERA:
				{
					action->type.camera.timeLeft -= deltaTime;
					if ( action->type.camera.timeLeft > 0 ) {
						Vector3F v;
						for( int i=0; i<3; ++i ) {
							v.X(i) = Interpolate(	(float)action->type.camera.time, 
													action->type.camera.start.X(i),
													0.0f,							
													action->type.camera.end.X(i),
													(float)action->type.camera.timeLeft );
						}
						engine->camera.SetPosWC( v );
					}
					else {
						actionStack.Pop();
						result |= OTHER_ACTION_COMPLETE;
					}
				}
				break;

			case ACTION_CAMERA_BOUNDS:
				{
					float t = Travel( deltaTime, action->type.cameraBounds.speed );
					
					// Don't let it over-shoot!
					Vector3F lookingAt;
					engine->CameraLookingAt( &lookingAt );
					Vector3F atToTarget = action->type.cameraBounds.target - lookingAt;
					if ( atToTarget.Length() <= t ) {
						actionStack.Pop();
					}
					else {
						Vector3F d = action->type.cameraBounds.normal * t;
						engine->camera.DeltaPosWC( d.x, d.y, d.z );

						const Screenport& port = engine->GetScreenport();
						Vector2I r;
						port.WorldToUI( action->type.cameraBounds.target, &r );

						Rectangle2I uiBounds;
						port.UIBoundsClipped3D( &uiBounds );
						Rectangle2I inset = uiBounds;
						inset.Outset( -20 );

						if ( inset.Contains( r ) ) {
							actionStack.Pop();
						}
					}
				}
				break;

			default:
				GLASSERT( 0 );
				break;
		}
		/*
		if ( unit ) {
			Vector2I newPos;
			float newRot;
			unit->CalcMapPos( &newPos, &newRot );

			// If we changed map position, update UI feedback.
			if ( newPos != originalPos || newRot != originalRot ) {
				CalcTeamTargets();
				nearPathState = NEAR_PATH_INVALID;
			}
		}*/
	}
	return result;
}


int BattleScene::ProcessActionShoot( Action* action, Unit* unit, Model* model )
{
	DamageDesc damageDesc;
	bool impact = false;
	Model* modelHit = 0;
	Vector3F intersection;
	U32 delayTime = 0;
	const WeaponItemDef* weaponDef = 0;
	Ray ray;

	int result = 0;

	if ( unit && model && unit->IsAlive() ) {
		Vector3F p0, p1;
		Vector3F beam0 = { 0, 0, 0 }, beam1 = { 0, 0, 0 };

		const Item* weaponItem = unit->GetWeapon();
		GLASSERT( weaponItem );
		weaponDef = weaponItem->GetItemDef()->IsWeapon();
		GLASSERT( weaponDef );

		model->CalcTrigger( &p0 );
		p1 = action->type.shoot.target;

		ray.origin = p0;
		ray.direction = p1-p0;

		// What can we hit?
		// model
		//		unit, alive (does damage)
		//		unit, dead (does nothing)
		//		model, world (does damage)
		//		gun, does nothing
		// ground / bounds

		// Don't hit the shooter:
		const Model* ignore[] = { unit->GetModel(), unit->GetWeaponModel(), 0 };
		Model* m = engine->IntersectModel( ray, TEST_TRI, 0, 0, ignore, &intersection );

		if ( m && intersection.y < 0.0f ) {
			// hit ground before the unit (intesection is with the part under ground)
			// The world bounds will pick this up later.
			m = 0;
		}

		weaponDef->DamageBase( action->type.shoot.mode, &damageDesc );

		if ( m ) {
			impact = true;
			GLASSERT( intersection.x >= 0 && intersection.x <= (float)MAP_SIZE );
			GLASSERT( intersection.z >= 0 && intersection.z <= (float)MAP_SIZE );
			GLASSERT( intersection.y >= 0 && intersection.y <= 10.0f );
			beam0 = p0;
			beam1 = intersection;
			GLASSERT( m->AABB().Contains( intersection ) );
			modelHit = m;
		}
		else {		
			Vector3F in, out;
			int inResult, outResult;
			Rectangle3F worldBounds;
			worldBounds.Set( 0, 0, 0, 
							(float)engine->GetMap()->Width(), 
							8.0f,
							(float)engine->GetMap()->Height() );

			int result = IntersectRayAllAABB( ray.origin, ray.direction, worldBounds, 
											  &inResult, &in, &outResult, &out );

			GLASSERT( result == grinliz::INTERSECT );
			if ( result == grinliz::INTERSECT ) {
				beam0 = p0;
				beam1 = out;
				intersection = out;
				GLASSERT( intersection.x >= 0 && intersection.x <= (float)MAP_SIZE );
				GLASSERT( intersection.z >= 0 && intersection.z <= (float)MAP_SIZE );
				GLASSERT( intersection.y >= 0 && intersection.y <= 10.0f );

				if ( out.y < 0.01 ) {
					// hit the ground
					impact = true;

				}
			}
		}

		if ( beam0 != beam1 ) {
			weaponDef->RenderWeapon( action->type.shoot.mode,
									 ParticleSystem::Instance(),
									 beam0, beam1, 
									 impact, 
									 game->CurrentTime(), 
									 &delayTime );
			SoundManager::Instance()->QueueSound( weaponDef->weapon[weaponDef->Index(action->type.shoot.mode)].sound );
		}
	}
	actionStack.Pop();
	result |= UNIT_ACTION_COMPLETE;

	if ( impact ) {
		GLASSERT( weaponDef );
		GLASSERT( intersection.x >= 0 && intersection.x <= (float)MAP_SIZE );
		GLASSERT( intersection.z >= 0 && intersection.z <= (float)MAP_SIZE );
		GLASSERT( intersection.y >= 0 && intersection.y <= 10.0f );

		Action h;
		h.Init( ACTION_HIT, unit );
		h.type.hit.damageDesc = damageDesc;
		h.type.hit.explosive = weaponDef->IsExplosive( action->type.shoot.mode );
		h.type.hit.p = intersection;
		
		h.type.hit.n = ray.direction;
		h.type.hit.n.Normalize();

		h.type.hit.m = modelHit;
		actionStack.Push( h );
	}

	if ( delayTime ) {
		Action a;
		a.Init( ACTION_DELAY, 0 );
		a.type.delay.delay = delayTime;
		actionStack.Push( a );
	}

	if ( impact && modelHit ) {
		int x = LRintf( intersection.x );
		int y = LRintf( intersection.z );
		if ( engine->GetMap()->GetFogOfWar().IsSet( x, y, 0 ) ) {
			PushScrollOnScreen( intersection );
		}
	}

	return result;
}


int BattleScene::ProcessActionHit( Action* action )
{
	Rectangle2I destroyed;
	destroyed.SetInvalid();
	int result = 0;

	if ( !action->type.hit.explosive ) {
		SoundManager::Instance()->QueueSound( "hit" );

		// Apply direct hit damage
		Model* m = action->type.hit.m;
		Unit* hitUnit = 0;
		Unit* hitWeapon = 0;

		if ( m ) {
			hitUnit = UnitFromModel( m, false );
			hitWeapon = UnitFromModel( m, true );
		}

		if ( hitUnit ) {
			if ( hitUnit->IsAlive() ) {
				hitUnit->DoDamage( action->type.hit.damageDesc, engine->GetMap() );
				if ( !hitUnit->IsAlive() ) {
					selection.ClearTarget();			
					visibility.InvalidateUnit( hitUnit - units );
					if ( action->unit )
						action->unit->CreditKill();
				}
				GLOUTPUT(( "Hit Unit 0x%x hp=%d/%d\n", (unsigned)hitUnit, (int)hitUnit->HP(), (int)hitUnit->GetStats().TotalHP() ));
			}
		}
		else if ( hitWeapon ) {
			Inventory* inv = hitWeapon->GetInventory();
			inv->RemoveItem( Inventory::WEAPON_SLOT );
			hitWeapon->UpdateInventory();
			GLOUTPUT(( "Shot the weapon out of Unit 0x%x hand.\n", (unsigned)hitWeapon ));
		}
		else if ( m && m->IsFlagSet( Model::MODEL_OWNED_BY_MAP ) ) {
			Rectangle2I bounds;
			engine->GetMap()->MapBoundsOfModel( m, &bounds );

			// Hit world object.
			engine->GetMap()->DoDamage( m, action->type.hit.damageDesc, &destroyed );
		}
	}
	else {
		// Explosion
		SoundManager::Instance()->QueueSound( "explosion" );

		const int MAX_RAD = 2;
		const int MAX_RAD_2 = MAX_RAD*MAX_RAD;
		Rectangle2I mapBounds;
		engine->GetMap()->CalcBounds( &mapBounds );

		// There is a small offset to move the explosion back towards the shooter.
		// If it hits a wall (common) this will move it to the previous square.
		// Also means a model hit may be a "near miss"...but explosions are messy.
		const int x0 = (int)(action->type.hit.p.x - 0.2f*action->type.hit.n.x);
		const int y0 = (int)(action->type.hit.p.z - 0.2f*action->type.hit.n.z);
		// generates smoke:
		destroyed.Set( x0-MAX_RAD, y0-MAX_RAD, x0+MAX_RAD, y0+MAX_RAD );

		for( int rad=0; rad<=MAX_RAD; ++rad ) {
			DamageDesc dd = action->type.hit.damageDesc;
			dd.Scale( (float)(1+MAX_RAD-rad) / (float)(1+MAX_RAD) );

			for( int y=y0-rad; y<=y0+rad; ++y ) {
				for( int x=x0-rad; x<=x0+rad; ++x ) {
					if ( x==(x0-rad) || x==(x0+rad) || y==(y0-rad) || y==(y0+rad) ) {

						Vector2I x0y0 = { x0, y0 };
						if ( !mapBounds.Contains( x0y0 ) )	// keep explosions in-bounds
							continue;

						// Tried to do this with the pather, but the issue with 
						// walls being on the inside and outside of squares got 
						// ugly. But the other option - ray casting - also nasty.
						// Go one further than could be walked.
						int radius2 = (x-x0)*(x-x0) + (y-y0)*(y-y0);
						if ( radius2 > MAX_RAD_2 )
							continue;
						
						// can the tile to be damaged be reached by the explosion?
						// visibility is checked up to the tile before this one, else
						// it is possible to miss because "you can't see yourself"
						bool canSee = true;
						if ( rad > 0 ) {
							LineWalk walk( x0, y0, x, y );
							// Actually 'less than' so that damage goes through walls a little.
							while( walk.CurrentStep() < walk.NumSteps() ) {
								Vector2I p = walk.P();
								Vector2I q = walk.Q();

								// Was using CanSee, but that has the problem that
								// some walls are inner and some walls are outer.
								// *sigh* Use the pather.
								if ( !engine->GetMap()->CanSee( p, q ) ) {
									canSee = false;
									break;
								}
								walk.Step();
							}
							// Go with the sight check to see if the explosion can
							// reach this tile.
							if ( !canSee )
								continue;
						}

						Unit* unit = GetUnitFromTile( x, y );
						if ( unit && unit->IsAlive() ) {
							unit->DoDamage( dd, engine->GetMap() );
							if ( !unit->IsAlive() ) {
								visibility.InvalidateUnit( unit - units );
								if ( unit == SelectedSoldierUnit() ) {
									selection.ClearTarget();			
								}
								if ( action->unit )
									action->unit->CreditKill();
							}
						}
						bool hitAnything = false;
						engine->GetMap()->DoDamage( x, y, dd, &destroyed );

						// Where to add smoke?
						// - if we hit anything
						// - change of smoke anyway
						if ( hitAnything || random.Bit() ) {
							int turns = 4 + random.Rand( 4 );
							engine->GetMap()->AddSmoke( x, y, turns );
						}
					}
				}
			}
		}
	}
	visibility.InvalidateAll( destroyed ); 
	actionStack.Pop();
	result |= UNIT_ACTION_COMPLETE;
	return true;
}


void BattleScene::HandleHotKeyMask( int mask )
{
	if ( mask & GAME_HK_NEXT_UNIT ) {
		HandleNextUnit( 1 );		
	}
	if ( mask & GAME_HK_PREV_UNIT ) {
		HandleNextUnit( -1 );		
	}
	if ( mask & GAME_HK_ROTATE_CCW ) {
		HandleRotation( 45.f );
	}
	if ( mask & GAME_HK_ROTATE_CW ) {
		HandleRotation( -45.f );
	}
	if ( mask & GAME_HK_TOGGLE_ROTATION_UI ) {
		rotationUIOn = !rotationUIOn;
	}
	if ( mask & GAME_HK_TOGGLE_NEXT_UI ) {
		nextUIOn = !nextUIOn;
	}
}


void BattleScene::HandleNextUnit( int bias )
{
	subTurnIndex += bias;
	if ( subTurnIndex < 0 )
		subTurnIndex = subTurnCount;
	if ( subTurnIndex > subTurnCount )
		subTurnIndex = 0;

	if ( subTurnIndex == subTurnCount ) {
		SetSelection( 0 );
	}
	else {
		int index = subTurnOrder[ subTurnIndex ];
		GLASSERT( index >= TERRAN_UNITS_START && index < TERRAN_UNITS_END );

		if ( units[index].IsAlive() ) {
			SetSelection( &units[index] );
			PushScrollOnScreen( units[index].GetModel()->Pos() );
		}
		else {
			SetSelection( 0 );
		}
	}
	//SoundManager::Instance()->QueueSound( "blip" );
}


void BattleScene::HandleRotation( float bias )
{
	if ( actionStack.Empty() && SelectedSoldierUnit() ) {
		Unit* unit = SelectedSoldierUnit();

		float r = unit->GetModel()->GetRotation();
		r += bias;
		r = NormalizeAngleDegrees( r );
		r = 45.f * float( (int)((r+20.0f) / 45.f) );

		Action action;
		action.Init( ACTION_ROTATE, unit );
		action.type.rotate.rotation = r;
		actionStack.Push( action );

	}
}


bool BattleScene::HandleIconTap( int vX, int vY )
{
	int screenX, screenY;
	engine->GetScreenport().ViewToUI( vX, vY, &screenX, &screenY );
	float guiX = (float)screenX;
	float guiY = (float)(engine->GetScreenport().UIHeight()-1-screenY);

	const UIItem* tapped = gamui2D.TapDown( guiX, guiY );
	gamui2D.TapUp( guiX, guiY );
	if ( !tapped ) {
		tapped = gamui3D.TapDown( guiX, guiY );
		gamui3D.TapUp( guiX, guiY );
	}

	if ( uiMode == UIM_FIRE_MENU ) {
		bool okay = false;
		WeaponMode mode = kModeSnap;

		if ( tapped == fireButton+0 )		{ okay = true; mode = kModeSnap; }
		else if ( tapped == fireButton+1 )	{ okay = true; mode = kModeAuto; }
		else if ( tapped == fireButton+2 )	{ okay = true; mode = kModeAlt;  }
			
		if ( okay ) { 
			// shooting creates a turn action then a shoot action.
			GLASSERT( selection.soldierUnit >= 0 );
			GLASSERT( selection.targetUnit >= 0 );
			//SoundManager::Instance()->QueueSound( "blip" );

			Item* weapon = selection.soldierUnit->GetWeapon();
			GLASSERT( weapon );
			const WeaponItemDef* wid = weapon->GetItemDef()->IsWeapon();
			GLASSERT( wid );

			Vector3F target;
			if ( selection.targetPos.x >= 0 ) {
				target.Set( (float)selection.targetPos.x + 0.5f, 1.0f, (float)selection.targetPos.y + 0.5f );
			}
			else {
				selection.targetUnit->GetModel()->CalcTarget( &target );
			}
			PushShootAction( selection.soldierUnit, target, mode, 1.0f, false );
			selection.targetUnit = 0;
		}
	}
	else if ( uiMode == UIM_TARGET_TILE ) {
		if ( tapped == &targetButton ) {
			uiMode = UIM_NORMAL;
		}
	}
	else {
		if ( tapped == &targetButton ) {
			//SoundManager::Instance()->QueueSound( "blip" );
				uiMode = UIM_TARGET_TILE;
		}
		else if ( tapped == controlButton + ROTATE_CCW_BUTTON ) {
			HandleRotation( 45.f );
		}
		else if ( tapped == controlButton + ROTATE_CCW_BUTTON ) {
			HandleRotation( -45.f );
		}
		else if ( tapped == &invButton ) {
			if ( actionStack.Empty() && SelectedSoldierUnit() ) {
				//SoundManager::Instance()->QueueSound( "blip" );
				CharacterSceneInput* input = new CharacterSceneInput();
				input->unit = SelectedSoldierUnit();
				input->canChangeArmor = false;
				game->PushScene( Game::CHARACTER_SCENE, input );
			}
		}
		else if ( tapped == &nextTurnButton ) {
			SetSelection( 0 );
			engine->GetMap()->ClearNearPath();
			if ( EndCondition( &gTacticalData ) ) {
				game->PushScene( Game::END_SCENE, &gTacticalData );
				//SoundManager::Instance()->QueueSound( "blip" );
			}
			else {
				NextTurn();
			}
		}
		else if ( tapped == controlButton + NEXT_BUTTON ) {
			HandleNextUnit( 1 );
		}
		else if ( tapped == controlButton + PREV_BUTTON ) {
			HandleNextUnit( -1 );
		}
		else if ( tapped == &helpButton ) {
			game->PushScene( Game::HELP_SCENE, 0 );
		}
	}
	return tapped != 0;
}


void BattleScene::Tap(	int tap, 
						const grinliz::Vector2I& screen,
						const grinliz::Ray& world )
{
	/*
	{
		// Test projections.
		Vector3F pos;
		IntersectRayPlane( world.origin, world.direction, 1, 0.0f, &pos );
		pos.y = 0.01f;
		Color4F c = { 1, 0, 0, 1 };
		Color4F cv = { 0, 0, 0, 0 };
		ParticleSystem::Instance()->EmitOnePoint( c, cv, pos, 1500 );
	}
	*/
	/*{
		// Test Sound.
		int size=0;
		const gamedb::Reader* reader = game->GetDatabase();
		const gamedb::Item* data = reader->Root()->Child( "data" );
		const gamedb::Item* item = data->Child( "testlaser44" );

		const void* snd = reader->AccessData( item, "binary", &size );
		GLASSERT( snd );
		PlayWAVSound( snd, size );
	}*/

	if ( tap > 1 )
		return;
	if ( !actionStack.Empty() )
		return;
	if ( currentTeamTurn != TERRAN_TEAM )
		return;

	/* Modes:
		- if target is up, it needs to be selected or cleared.
		- if targetMode is on, wait for tile selection or targetMode off
		- else normal mode

	*/

	if ( uiMode == UIM_NORMAL ) {
		bool iconSelected = HandleIconTap( screen.x, screen.y );
		if ( iconSelected )
			return;
	}
	else if ( uiMode == UIM_TARGET_TILE ) {
		bool iconSelected = HandleIconTap( screen.x, screen.y );
		// If the mode was turned off, return. Else the selection is handled below.
		if ( iconSelected )
			return;
	}
	else if ( uiMode == UIM_FIRE_MENU ) {
		HandleIconTap( screen.x, screen.y );
		// Whether or not something was selected, drop back to normal mode.
		uiMode = UIM_NORMAL;
		selection.targetPos.Set( -1, -1 );
		selection.targetUnit = 0;
		return;
	}

#ifdef MAPMAKER
	const Vector3F& pos = mapSelection->Pos();
	int rotation = (int) (mapSelection->GetRotation() / 90.0f );

	int ix = (int)pos.x;
	int iz = (int)pos.z;
	if (    ix >= 0 && ix < engine->GetMap()->Width()
	  	 && iz >= 0 && iz < engine->GetMap()->Height()
		 && *engine->GetMap()->GetItemDefName( currentMapItem ) )
	{
		engine->GetMap()->AddItem( ix, iz, rotation, currentMapItem, -1, 0 );
	}
#endif	

	// Get the map intersection. May be used by TARGET_TILE or NORMAL
	Vector3F intersect;
	Map* map = engine->GetMap();

	Vector2I tilePos = { 0, 0 };
	bool hasTilePos = false;

	int result = IntersectRayPlane( world.origin, world.direction, 1, 0.0f, &intersect );
	if ( result == grinliz::INTERSECT && intersect.x >= 0 && intersect.x < Map::SIZE && intersect.z >= 0 && intersect.z < Map::SIZE ) {
		tilePos.Set( (int)intersect.x, (int) intersect.z );
		hasTilePos = true;
	}

	if ( uiMode == UIM_TARGET_TILE ) {
		if ( hasTilePos ) {
			selection.targetUnit = 0;
			selection.targetPos.Set( tilePos.x, tilePos.y );
			uiMode = UIM_FIRE_MENU;
		}
		return;
	}

	GLASSERT( uiMode == UIM_NORMAL );

	// We didn't tap a button.
	// What got tapped? First look to see if a SELECTABLE model was tapped. If not, 
	// look for a selectable model from the tile.

	// If there is a selected model, then we can tap a target model.
	bool canSelectAlien = SelectedSoldier();			// a soldier is selected

	for( int i=TERRAN_UNITS_START; i<TERRAN_UNITS_END; ++i ) {
		if ( units[i].GetModel() ) units[i].GetModel()->ClearFlag( Model::MODEL_SELECTABLE );

		if ( units[i].IsAlive() ) {
			GLASSERT( units[i].GetModel() );
			units[i].GetModel()->SetFlag( Model::MODEL_SELECTABLE );
		}
	}
	for( int i=ALIEN_UNITS_START; i<ALIEN_UNITS_END; ++i ) {
		if ( units[i].GetModel() ) units[i].GetModel()->ClearFlag( Model::MODEL_SELECTABLE );

		if ( canSelectAlien && units[i].IsAlive() ) {
			GLASSERT( units[i].GetModel() );
			units[i].GetModel()->SetFlag( Model::MODEL_SELECTABLE );
		}
	}

	Model* tappedModel = engine->IntersectModel( world, TEST_HIT_AABB, Model::MODEL_SELECTABLE, 0, 0, 0 );
	const Unit* tappedUnit = UnitFromModel( tappedModel );

	if ( tappedModel && tappedUnit && tappedUnit->Team() == ALIEN_TEAM ) {
		SetSelection( UnitFromModel( tappedModel ) );		// sets either the Alien or the Unit
		map->ClearNearPath();
	}
	else {
		// Not a model - use the tile
		if ( SelectedSoldierModel() && !selection.targetUnit && hasTilePos ) {
			Vector2<S16> start   = { (S16)SelectedSoldierModel()->X(), (S16)SelectedSoldierModel()->Z() };

			Vector2<S16> end = { (S16)tilePos.x, (S16)tilePos.y };

			// Compute the path:
			float cost;
			const Stats& stats = selection.soldierUnit->GetStats();

			int result = engine->GetMap()->SolvePath( selection.soldierUnit, start, end, &cost, &pathCache );
			if ( result == micropather::MicroPather::SOLVED && cost <= selection.soldierUnit->TU() ) {
				// TU for a move gets used up "as we go" to account for reaction fire and changes.
				// Go!
				Action action;
				action.Init( ACTION_MOVE, SelectedSoldierUnit() );
				action.type.move.path.Init( pathCache );
				actionStack.Push( action );

				engine->GetMap()->ClearNearPath();
			}
		}
	}

#if 0 // implements tapping to select.
	if ( tappedModel && tappedUnit ) {
		if ( tappedUnit->Team() == ALIEN_TEAM ) {
			SetSelection( UnitFromModel( tappedModel ) );		// sets either the Alien or the Unit
			map->ClearNearPath();
		}
		else if ( tappedUnit->Team() == TERRAN_TEAM ) {
			SetSelection( UnitFromModel( tappedModel ) );
			nearPathState = NEAR_PATH_INVALID;
		}
		else {
			SetSelection( 0 );
			map->ClearNearPath();
		}
	}
	else if ( !tappedModel ) {
		// Not a model - use the tile
		if ( SelectedSoldierModel() && !selection.targetUnit && hasTilePos ) {
			Vector2<S16> start   = { (S16)SelectedSoldierModel()->X(), (S16)SelectedSoldierModel()->Z() };

			Vector2<S16> end = { (S16)tilePos.x, (S16)tilePos.y };

			// Compute the path:
			float cost;
			const Stats& stats = selection.soldierUnit->GetStats();

			int result = engine->GetMap()->SolvePath( selection.soldierUnit, start, end, &cost, &pathCache );
			if ( result == micropather::MicroPather::SOLVED && cost <= selection.soldierUnit->TU() ) {
				// TU for a move gets used up "as we go" to account for reaction fire and changes.
				// Go!
				Action action;
				action.Init( ACTION_MOVE, SelectedSoldierUnit() );
				action.type.move.path.Init( pathCache );
				actionStack.Push( action );

				engine->GetMap()->ClearNearPath();
			}
		}
	}
#endif
}


void BattleScene::ShowNearPath( const Unit* unit )
{
	if ( unit == 0 && nearPathState.unit == 0 )		
		return;		// drawing nothing correctly
	if (    unit == nearPathState.unit
		 && unit->TU() == nearPathState.tu
		 && unit->Pos() == nearPathState.pos ) {
			return;		// drawing something that is current.
	}

	nearPathState.Clear();
	engine->GetMap()->ClearNearPath();

	if ( unit && unit->GetModel() ) {

		float autoTU = 0.0f;
		float snappedTU = 0.0f;

		if ( unit->GetWeapon() ) {
			const WeaponItemDef* wid = unit->GetWeapon()->GetItemDef()->IsWeapon();
			snappedTU = unit->FireTimeUnits( kModeSnap );
			autoTU = unit->FireTimeUnits( kModeAuto );
		}

		nearPathState.unit = unit;
		nearPathState.tu = unit->TU();
		nearPathState.pos = unit->Pos();

		Vector2<S16> start = { (S16)unit->Pos().x, (S16)unit->Pos().y };
		float tu = unit->TU();

		Vector2F range[3] = { 
			{ 0.0f,	tu-autoTU },
			{ tu-autoTU, tu-snappedTU },
			{ tu-snappedTU, tu }
		};
		engine->GetMap()->ShowNearPath( unit->Pos(), unit, start, tu, range );
	}
}


void BattleScene::MakePathBlockCurrent( Map* map, const void* user )
{
	const Unit* exclude = (const Unit*)user;
	GLASSERT( exclude >= units && exclude < &units[MAX_UNITS] );

	grinliz::BitArray<MAP_SIZE, MAP_SIZE, 1> block;

	for( int i=0; i<MAX_UNITS; ++i ) {
		if (    units[i].Status() == Unit::STATUS_ALIVE 
			 && units[i].GetModel() 
			 && &units[i] != exclude ) // oops - don't cause self to not path
		{
			int x = (int)units[i].GetModel()->X();
			int z = (int)units[i].GetModel()->Z();
			block.Set( x, z );
		}
	}
	// Checks for equality before the reset.
	map->SetPathBlocks( block );
}

Unit* BattleScene::UnitFromModel( Model* m, bool useWeaponModel )
{
	if ( m ) {
		for( int i=0; i<MAX_UNITS; ++i ) {
			if (	( !useWeaponModel && units[i].GetModel() == m )
				 || ( useWeaponModel && units[i].GetWeaponModel() == m ) )
				return &units[i];
		}
	}
	return 0;
}


Unit* BattleScene::GetUnitFromTile( int x, int z )
{
	for( int i=0; i<MAX_UNITS; ++i ) {
		if ( units[i].GetModel() ) {
			int ux = (int)units[i].GetModel()->X();
			if ( ux == x ) {
				int uz = (int)units[i].GetModel()->Z();
				if ( uz == z ) { 
					return &units[i];
				}
			}
		}
	}
	return 0;
}


void BattleScene::DumpTargetEvents()
{
	for( int i=0; i<targetEvents.Size(); ++i ) {
		const TargetEvent& e = targetEvents[i];
		const char* teams[] = { "Terran", "Civ", "Alien" };

		if ( !e.team ) {
			GLOUTPUT(( "%s Unit %d %s %s %d\n",
						teams[ units[e.viewerID].Team() ],
						e.viewerID, 
						e.gain ? "gain" : "loss", 
						teams[ units[e.targetID].Team() ],
						e.targetID ));
		}
		else {
			GLOUTPUT(( "%s Team %s %s %d\n",
						teams[ e.viewerID ],
						e.gain ? "gain" : "loss", 
						teams[ units[e.targetID].Team() ],
						e.targetID ));
		}
	}
}


void BattleScene::CalcTeamTargets()
{
	// generate events.
	// - if team gets/loses target
	// - if unit gets/loses target

	Targets old = m_targets;
	m_targets.Clear();

	Vector2I targets[] = { { ALIEN_UNITS_START, ALIEN_UNITS_END },   { TERRAN_UNITS_START, TERRAN_UNITS_END } };
	Vector2I viewers[] = { { TERRAN_UNITS_START, TERRAN_UNITS_END }, { ALIEN_UNITS_START, ALIEN_UNITS_END } };
	int viewerTeam[3]  = { TERRAN_TEAM, ALIEN_TEAM };

	for( int range=0; range<2; ++range ) {
		// Terran to Alien
		// Go through each alien, and check #terrans that can target it.
		//
		for( int j=targets[range].x; j<targets[range].y; ++j ) {
			// Push IsAlive() checks into the cases below, so that a unit
			// death creates a visibility change. However, initialization 
			// happens at the beginning, so we can skip that.

			if ( !units[j].InUse() )
				continue;

			Vector2I mapPos;
			units[j].CalcMapPos( &mapPos, 0 );
			
			for( int k=viewers[range].x; k<viewers[range].y; ++k ) {
				// Main test: can a terran see an alien at this location?
				if (    units[k].IsAlive() 
					 && units[j].IsAlive() 
					 && visibility.UnitCanSee( k, mapPos.x, mapPos.y ) ) 
				{
					m_targets.Set( k, j );
				}

				// check unit change.
				if ( old.CanSee( k, j ) && !m_targets.CanSee( k, j ) ) 
				{
					// Lost unit.
					TargetEvent e = { 0, 0, k, j };
					targetEvents.Push( e );
				}
				else if ( !old.CanSee( k, j )  && m_targets.CanSee( k, j ) ) 
				{
					// Gain unit.
					TargetEvent e = { 0, 1, k, j };
					targetEvents.Push( e );
				}
			}
			// Check team change.
			if ( old.TeamCanSee( viewerTeam[range], j ) && !m_targets.TeamCanSee( viewerTeam[range], j ) )
			{	
				TargetEvent e = { 1, 0, viewerTeam[range], j };
				targetEvents.Push( e );
			}
			else if ( !old.TeamCanSee( viewerTeam[range], j ) && m_targets.TeamCanSee( viewerTeam[range], j ) )
			{	
				TargetEvent e = { 1, 1, viewerTeam[range], j };
				targetEvents.Push( e );
			}
		}
	}
}


BattleScene::Visibility::Visibility() : battleScene( 0 ), units( 0 ), map( 0 )
{
	memset( current, 0, sizeof(bool)*MAX_UNITS );
	fogInvalid = true;
}


void BattleScene::Visibility::InvalidateUnit( int i ) 
{
	GLASSERT( i>=0 && i<MAX_UNITS );
	current[i] = false;
	// Do not check IsAlive(). Specifically called when units are alive or just killed.
	if ( i >= TERRAN_UNITS_START && i < TERRAN_UNITS_END ) {
		fogInvalid = true;
	}
}


void BattleScene::Visibility::InvalidateAll( const Rectangle2I& bounds )
{
	if ( !bounds.IsValid() )
		return;

	Vector2I range[2] = {{ TERRAN_UNITS_START, TERRAN_UNITS_END }, {ALIEN_UNITS_START, ALIEN_UNITS_END}};
	Rectangle2I vis;

	for( int k=0; k<2; ++k ) {
		for( int i=range[k].x; i<range[k].y; ++i ) {
			if ( units[i].IsAlive() ) {
				units[i].CalcVisBounds( &vis );
				if ( bounds.Intersect( vis ) ) {
					current[i] = false;
					if ( units[i].Team() == TERRAN_TEAM ) {
						fogInvalid = true;
					}
				}
			}
		}
	}
}


void BattleScene::Visibility::InvalidateAll()
{
	memset( current, 0, sizeof(bool)*MAX_UNITS );
	fogInvalid = true;
}


int BattleScene::Visibility::TeamCanSee( int team, int x, int y )
{
	int r0, r1;
	if ( team == TERRAN_TEAM ) {
		r0 = TERRAN_UNITS_START;
		r1 = TERRAN_UNITS_END;
	}
	else if ( team == ALIEN_TEAM ) {
		r0 = ALIEN_UNITS_START;
		r1 = ALIEN_UNITS_END;
	}
	else {
		GLASSERT( 0 );
	}
	
	for( int i=r0; i<r1; ++i ) {
		if ( UnitCanSee( i, x, y ) )
			return true;
	}
	return false;
}


int BattleScene::Visibility::UnitCanSee( int i, int x, int y )
{
#ifdef MAPMAKER
	return true;
#else
	if ( units[i].IsAlive() ) {
		if ( !current[i] ) {
			CalcUnitVisibility( i );
			current[i] = true;
		}
		if ( visibilityMap.IsSet( x, y, i ) ) {
			return true;
		}
	}
	return false;
#endif
}



/*	Huge ol' performance bottleneck.
	The CalcVis() is pretty good (or at least doesn't chew up too much time)
	but the CalcAll() is terrible.
	Debug mode.
	Start: 141 MClocks
	Moving to "smart recursion": 18 MClocks - that's good! That's good enough to hide the cost is caching.
		...but also has lots of artifacts in visibility.
	Switched to a "cached ray" approach. 58 MClocks. Much better, correct results, and can be optimized more.

	In Core clocks:
	"cached ray" 45 clocks
	33 clocks after tuning. (About 1/4 of initial cost.)
	Tighter walk: 29 MClocks

	Back on the Atom:
	88 MClocks. But...experimenting with switching to 360degree view.
	...now 79 MClocks. That makes little sense. Did facing take a bunch of cycles??
*/


void BattleScene::Visibility::CalcUnitVisibility( int unitID )
{
	//unit = units;	// debugging: 1st unit only
	GLASSERT( unitID >= 0 && unitID < MAX_UNITS );
	const Unit* unit = &units[unitID];

	Vector2I pos = unit->Pos();

	// Clear out the old settings.
	// Walk the area in range around the unit and cast rays.
	visibilityMap.ClearPlane( unitID );
	visibilityProcessed.ClearAll();

	Rectangle2I mapBounds;
	map->CalcBounds( &mapBounds );

	// Can always see yourself.
	visibilityMap.Set( pos.x, pos.y, unitID );
	visibilityProcessed.Set( pos.x, pos.y, 0 );

	const int MAX_SIGHT_SQUARED = MAX_EYESIGHT_RANGE*MAX_EYESIGHT_RANGE;

	for( int r=MAX_EYESIGHT_RANGE; r>0; --r ) {
		Vector2I p = { pos.x-r, pos.y-r };
		static const Vector2I delta[4] = { { 1,0 }, {0,1}, {-1,0}, {0,-1} };

		for( int k=0; k<4; ++k ) {
			for( int i=0; i<r*2; ++i ) {
				if (    mapBounds.Contains( p )
					 && !visibilityProcessed.IsSet( p.x, p.y )
					 && (p-pos).LengthSquared() <= MAX_SIGHT_SQUARED ) 
				{
					CalcVisibilityRay( unitID, p, pos );
				}
				p += delta[k];
			}
		}
	}
}


void BattleScene::Visibility::CalcVisibilityRay( int unitID, const Vector2I& pos, const Vector2I& origin )
{
	/* Previous pass used a true ray casting approach, but this doesn't get good results. Numerical errors,
	   view stopped by leaves, rays going through cracks. Switching to a line walking approach to 
	   acheive stability and simplicity. (And probably performance.)
	*/


#ifdef DEBUG
	{
		// Max sight
		const int MAX_SIGHT_SQUARED = MAX_EYESIGHT_RANGE*MAX_EYESIGHT_RANGE;
		Vector2I vec = pos - origin;
		int len2 = vec.LengthSquared();
		GLASSERT( len2 <= MAX_SIGHT_SQUARED );
		if ( len2 > MAX_SIGHT_SQUARED )
			return;
	}
#endif

	const Surface* lightMap = map->GetLightMap(1);
	GLASSERT( lightMap->Format() == Surface::RGB16 );

	const float OBSCURED = 0.50f;
	const float DARK  = (units[unitID].Team() == ALIEN_TEAM) ? 0.40f : 0.32f;
	const float LIGHT = 0.12f;
	//const int EPS = 10;

	float light = 1.0f;
	bool canSee = true;

	// Always walk the entire line so that the places we can not see are set
	// as well as the places we can.
	LineWalk line( origin.x, origin.y, pos.x, pos.y ); 
	while ( line.CurrentStep() <= line.NumSteps() )
	{
		Vector2I p = line.P();
		Vector2I q = line.Q();
		Vector2I delta = q-p;

		if ( canSee ) {
			canSee = map->CanSee( p, q );

			if ( canSee ) {
				U16 c = lightMap->GetImg16( q.x, q.y );
				Surface::RGBA rgba = Surface::CalcRGB16( c );

				const float distance = ( delta.LengthSquared() > 1 ) ? 1.4f : 1.0f;

				if ( map->Obscured( q.x, q.y ) ) {
					light -= OBSCURED * distance;
				}
				else
				{
					// Blue channel is typically high. So 
					// very dark  ~255
					// very light ~255*3 (white)
					int lum = rgba.r + rgba.g + rgba.b;
					float fraction = Interpolate( 255.0f, DARK, 765.0f, LIGHT, (float)lum ); 
					light -= fraction * distance;
				}
			}
		}
		visibilityProcessed.Set( q.x, q.y );
		if ( canSee ) {
			visibilityMap.Set( q.x, q.y, unitID, true );
		}

		// If all the light is used up, we will see no further.	
		if ( canSee && light < 0.0f )
			canSee = false;	

		line.Step(); 
	}
}


void BattleScene::Drag( int action, const grinliz::Vector2I& view )
{
	Vector2I screen;
	engine->GetScreenport().ViewToScreen( view.x, view.y, &screen );
	
	switch ( action ) 
	{
		case GAME_DRAG_START:
		{
			Ray ray;
			engine->GetScreenport().ViewProjectionInverse3D( &dragMVPI );
			engine->RayFromScreenToYPlane( screen.x, screen.y, dragMVPI, &ray, &dragStart );
			dragStartCameraWC = engine->camera.PosWC();
		}
		break;

		case GAME_DRAG_MOVE:
		{
			Vector3F drag;
			Ray ray;
			engine->RayFromScreenToYPlane( screen.x, screen.y, dragMVPI, &ray, &drag );

			Vector3F delta = drag - dragStart;
			//GLOUTPUT(( "screen=%d,%d drag=%.2f,%.2f  delta=%.2f,%.2f\n", screen.x, screen.y, drag.x, drag.z, delta.x, delta.z ));
			delta.y = 0.0f;

			engine->camera.SetPosWC( dragStartCameraWC - delta );
			engine->RestrictCamera();
		}
		break;

		case GAME_DRAG_END:
		{
		}
		break;

		default:
			GLASSERT( 0 );
			break;
	}
}


void BattleScene::Zoom( int action, int distance )
{
	switch ( action )
	{
		case GAME_ZOOM_START:
			initZoomDistance = distance;
			initZoom = engine->GetZoom();
//			GLOUTPUT(( "initZoomStart=%.2f distance=%d initDist=%d\n", initZoom, distance, initZoomDistance ));
			break;

		case GAME_ZOOM_MOVE:
			{
				//float z = initZoom * (float)distance / (float)initZoomDistance;	// original. wrong feel.
				float z = initZoom - (float)(distance-initZoomDistance)/800.0f;	// better, but slow out zoom-out, fast at zoom-in
				
//				GLOUTPUT(( "initZoom=%.2f distance=%d initDist=%d\n", initZoom, distance, initZoomDistance ));
				engine->SetZoom( z );
			}
			break;

		default:
			GLASSERT( 0 );
			break;
	}
	//GLOUTPUT(( "Zoom action=%d distance=%d initZoomDistance=%d lastZoomDistance=%d z=%.2f\n",
	//		   action, distance, initZoomDistance, lastZoomDistance, GetZoom() ));
}


void BattleScene::Rotate( int action, float degrees )
{
	if ( action == GAME_ROTATE_START ) {
		orbitPole.Set( 0.0f, 0.0f, 0.0f );

		const Vector3F* dir = engine->camera.EyeDir3();
		const Vector3F& pos = engine->camera.PosWC();

		IntersectRayPlane( pos, dir[0], 1, 0.0f, &orbitPole );	
		//orbitPole.Dump( "pole" );

		orbitStart = ToDegree( atan2f( pos.x-orbitPole.x, pos.z-orbitPole.z ) );
	}
	else {
		//GLOUTPUT(( "degrees=%.2f\n", degrees ));
		engine->camera.Orbit( orbitPole, orbitStart + degrees );
	}
}


void BattleScene::DrawHUD()
{
#ifdef MAPMAKER
	engine->GetMap()->DumpTile( (int)mapSelection->X(), (int)mapSelection->Z() );
	UFOText::Draw( 0,  16, "(%2d,%2d) 0x%2x:'%s'", 
				   (int)mapSelection->X(), (int)mapSelection->Z(),
				   currentMapItem, engine->GetMap()->GetItemDefName( currentMapItem ) );
#else
	{
		bool enabled = SelectedSoldierUnit() && actionStack.Empty() && (uiMode != UIM_TARGET_TILE);
		targetButton.SetEnabled( enabled );
		invButton.SetEnabled( enabled );
		controlButton[ROTATE_CCW_BUTTON].SetEnabled( enabled );
		controlButton[ROTATE_CW_BUTTON].SetEnabled( enabled );
	}
	{
		bool enabled = actionStack.Empty() && (uiMode != UIM_TARGET_TILE);
		controlButton[NEXT_BUTTON].SetEnabled( enabled );
		controlButton[PREV_BUTTON].SetEnabled( enabled );
		exitButton.SetEnabled( enabled );
		nextTurnButton.SetEnabled( enabled );
		helpButton.SetEnabled( enabled );
	}
	// overrides enabled, above.
	if ( uiMode == UIM_TARGET_TILE ) {
		targetButton.SetEnabled( true );
	}
	
	controlButton[ROTATE_CCW_BUTTON].SetVisible( rotationUIOn );
	controlButton[ROTATE_CW_BUTTON].SetVisible( rotationUIOn );
	controlButton[NEXT_BUTTON].SetVisible( nextUIOn );
	controlButton[PREV_BUTTON].SetVisible( nextUIOn );

	alienImage.SetVisible( currentTeamTurn == ALIEN_TEAM );
	const int CYCLE = 5000;
	float rotation = (float)(game->CurrentTime() % CYCLE)*(360.0f/(float)CYCLE);
	if ( rotation > 90 && rotation < 270 )
		rotation += 180;
	alienImage.SetRotationY( rotation );

	nameRankUI.SetVisible( SelectedSoldierUnit() != 0 );
	nameRankUI.Set( 50, 0, SelectedSoldierUnit(), true );

	DrawHPBars();
#endif
}


float MotionPath::DeltaToRotation( int dx, int dy )
{
	float rot = 0.0f;
	GLASSERT( dx || dy );
	GLASSERT( dx >= -1 && dx <= 1 );
	GLASSERT( dy >= -1 && dy <= 1 );

	if ( dx == 1 ) 
		if ( dy == 1 )
			rot = 45.0f;
		else if ( dy == 0 )
			rot = 90.0f;
		else
			rot = 135.0f;
	else if ( dx == 0 )
		if ( dy == 1 )
			rot = 0.0f;
		else
			rot = 180.0f;
	else
		if ( dy == 1 )
			rot = 315.0f;
		else if ( dy == 0 )
			rot = 270.0f;
		else
			rot = 225.0f;
	return rot;
}


void MotionPath::Init( const std::vector< Vector2<S16> >& pathCache ) 
{
	GLASSERT( pathCache.size() <= MAX_TU );
	GLASSERT( pathCache.size() > 1 );	// at least start and end

	pathLen = (int)pathCache.size();
	for( unsigned i=0; i<pathCache.size(); ++i ) {
		GLASSERT( pathCache[i].x < 256 );
		GLASSERT( pathCache[i].y < 256 );
		pathData[i*2+0] = (U8) pathCache[i].x;
		pathData[i*2+1] = (U8) pathCache[i].y;
	}
}

		
void MotionPath::CalcDelta( int i0, int i1, grinliz::Vector2I* vec, float* rot )
{
	Vector2<S16> path0 = GetPathAt( i0 );
	Vector2<S16> path1 = GetPathAt( i1 );

	int dx = path1.x - path0.x;
	int dy = path1.y - path0.y;
	if ( vec ) {
		vec->x = dx;
		vec->y = dy;
	}
	if ( rot ) {
		*rot = DeltaToRotation( dx, dy );
	}
}


void MotionPath::Travel(	float* travel,
							int* pos,
							float* fraction )
{
	// fraction is a bit funny. It is the lerp value between 2 path locations,
	// so it isn't a constant distance.
	Vector2I vec;
	CalcDelta( *pos, *pos+1, &vec, 0 );

	float distBetween = 1.0f;
	if ( vec.x && vec.y ) {
		distBetween = 1.41f;
	}
	float distRemain = (1.0f-*fraction) * distBetween;

	if ( *travel >= distRemain ) {
		*travel -= distRemain;
		(*pos)++;
		*fraction = 0.0f;
	}
	else {
		*fraction += *travel / distBetween;
		*travel = 0.0f;
	}
}


void MotionPath::GetPos( int step, float fraction, float* x, float* z, float* rot )
{
	GLASSERT( step < pathLen );
	GLASSERT( fraction >= 0.0f && fraction < 1.0f );
	if ( step == pathLen-1 ) {
		step = pathLen-2;
		fraction = 1.0f;
	}
	Vector2<S16> path0 = GetPathAt( step );
	Vector2<S16> path1 = GetPathAt( step+1 );

	int dx = path1.x - path0.x;
	int dy = path1.y - path0.y;
	*x = (float)path0.x + fraction*(float)( dx );
	*z = (float)path0.y + fraction*(float)( dy );
	*rot = DeltaToRotation( dx, dy );
}


#if !defined(MAPMAKER) && defined(_MSC_VER)
// TEST CODE
void BattleScene::MouseMove( int x, int y )
{
	/*
	grinliz::Matrix4 mvpi;
	grinliz::Ray ray;
	Vector3F intersection;

	engine->CalcModelViewProjectionInverse( &mvpi );
	engine->RayFromScreen( x, y, mvpi, &ray );
	Model* model = engine->IntersectModel( ray, TEST_TRI, Model::MODEL_SELECTABLE, 0, &intersection );

	if ( model ) {
		Color4F color = { 1.0f, 0.0f, 0.0f, 1.0f };
		Color4F colorVel = { 0.0f, 0.0f, 0.0f, 0.0f };
		Vector3F vel = { 0, -0.5, 0 };

		game->particleSystem->Emit(	ParticleSystem::POINT,
									0, 1, ParticleSystem::PARTICLE_RAY,
									color,
									colorVel,
									intersection,
									0.0f,			// posFuzz
									vel,
									0.0f,			// velFuzz
									2000 );


		vel.y = 0;
		intersection.y = 0;
		color.Set( 0, 0, 1, 1 );

		game->particleSystem->Emit(	ParticleSystem::POINT,
									0, 1, ParticleSystem::PARTICLE_RAY,
									color,
									colorVel,
									intersection,
									0.0f,			// posFuzz
									vel,
									0.0f,			// velFuzz
									2000 );
	}	
	*/
}
#endif



#ifdef MAPMAKER


void BattleScene::UpdatePreview()
{
	if ( preview ) {
		engine->FreeModel( preview );
		preview = 0;
	}
	if ( currentMapItem >= 0 ) {
		preview = engine->GetMap()->CreatePreview(	(int)mapSelection->X(), 
													(int)mapSelection->Z(), 
													currentMapItem, 
													(int)(mapSelection->GetRotation()/90.0f) );

		if ( preview ) {
			Texture* t = TextureManager::Instance()->GetTexture( "translucent" );
			preview->SetTexture( t );
			preview->SetTexXForm( 0, 0, TRANSLUCENT_WHITE, 0.0f );
		}
	}
}


void BattleScene::MouseMove( int x, int y )
{
	grinliz::Matrix4 mvpi;
	grinliz::Ray ray;
	grinliz::Vector3F p;

	engine->GetScreenport().ViewProjectionInverse3D( &mvpi );
	engine->RayFromScreenToYPlane( x, y, mvpi, &ray, &p );

	int newX = (int)( p.x );
	int newZ = (int)( p.z );
	newX = Clamp( newX, 0, Map::SIZE-1 );
	newZ = Clamp( newZ, 0, Map::SIZE-1 );
	mapSelection->SetPos( (float)newX + 0.5f, 0.0f, (float)newZ + 0.5f );
	
	UpdatePreview();
}

void BattleScene::RotateSelection( int delta )
{
	float rot = mapSelection->GetRotation() + 90.0f*(float)delta;
	mapSelection->SetRotation( rot );
	UpdatePreview();
}

void BattleScene::DeleteAtSelection()
{
	const Vector3F& pos = mapSelection->Pos();
	engine->GetMap()->DeleteAt( (int)pos.x, (int)pos.z );
	UpdatePreview();
}


void BattleScene::DeltaCurrentMapItem( int d )
{
	currentMapItem += d;
	while ( currentMapItem < 0 ) { currentMapItem += Map::MAX_ITEM_DEF; }
	while ( currentMapItem >= Map::MAX_ITEM_DEF ) { currentMapItem -= Map::MAX_ITEM_DEF; }
	if ( currentMapItem == 0 ) currentMapItem = 1;
	UpdatePreview();
}

#endif


