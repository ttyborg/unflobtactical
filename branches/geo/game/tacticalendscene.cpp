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

#include "tacticalendscene.h"
#include "../engine/uirendering.h"
#include "../engine/engine.h"
#include "../engine/text.h"
#include "game.h"
#include "cgame.h"
#include "../grinliz/glstringutil.h"
#include "tacticalintroscene.h"
#include "research.h"

using namespace grinliz;

TacticalEndScene::TacticalEndScene( Game* _game, const TacticalEndSceneData* d ) : Scene( _game )
{
	Engine* engine = GetEngine();
	data = d;

	gamui::RenderAtom nullAtom;
	backgroundUI.Init( game, &gamui2D, false );

	for( int i=0; i<TEXT_ROW*TEXT_COL; ++i ) {
		textTable[i].Init( &gamui2D );
	}

	enum {
		DESC,
		COUNT,
		SCORE,
		ITEM
	};
	//									description  count				 score					items
	static const float X_ORIGIN[4]  = { GAME_GUTTER, GAME_GUTTER+150.0f, GAME_GUTTER+200.0f,	GAME_GUTTER+270.0f };
	static const float Y_ORIGIN		= GAME_GUTTER;
	static const float SPACING		= GAME_SPACING;
	float yPos = GAME_GUTTER;

	victory.Init( &gamui2D );
	int nSoldiers = Unit::Count( data->soldiers, MAX_TERRANS, -1 );
	int nSoldiersStanding = Unit::Count( data->soldiers, MAX_TERRANS, Unit::STATUS_ALIVE );
	int nSoldiersDown      =   Unit::Count( data->soldiers, MAX_TERRANS, Unit::STATUS_KIA )
						     + Unit::Count( data->soldiers, MAX_TERRANS, Unit::STATUS_UNCONSCIOUS )
							 + Unit::Count( data->soldiers, MAX_TERRANS, Unit::STATUS_MIA );

	int nAliens = Unit::Count( data->aliens, MAX_ALIENS, -1 );
	int nAliensAlive = Unit::Count( data->aliens, MAX_ALIENS, Unit::STATUS_ALIVE );
	int nAliensKIA = Unit::Count( data->aliens, MAX_ALIENS, Unit::STATUS_KIA );

	int nCivsAlive = Unit::Count( data->civs, MAX_CIVS, Unit::STATUS_ALIVE );
	int nCivsKIA = Unit::Count( data->civs, MAX_CIVS, Unit::STATUS_KIA );

	if ( nSoldiersStanding>0 && nAliensAlive==0 ) {
		victory.SetText( "Victory!" );
		
		// Award UFO stuff
		int scenario = game->GetScenario();
		if ( TacticalIntroScene::IsScoutScenario( scenario ) ) {
			d->storage->AddItem( "Cor:S" );
		}
		else if ( TacticalIntroScene::IsFrigateScenario( scenario ) ) {
			d->storage->AddItem( "Cor:F" );
		}
		else if ( scenario == TacticalIntroScene::BATTLESHIP ) {
			d->storage->AddItem( "Cor:B" );
		}

		// Alien corpses:
		for( int i=0; i<MAX_ALIENS; ++i ) {
			if ( data->aliens[i].InUse() ) {
				switch( data->aliens[i].AlienType() ) {

				case Unit::ALIEN_GREEN:		d->storage->AddItem( "Green" );	break;
				case Unit::ALIEN_PRIME:		d->storage->AddItem( "Prime" );	break;
				case Unit::ALIEN_HORNET:	d->storage->AddItem( "Hrnet" );	break;
				case Unit::ALIEN_JACKAL:	d->storage->AddItem( "Jackl" );	break;
				case Unit::ALIEN_VIPER:		d->storage->AddItem( "Viper" );	break;
				default: GLASSERT( 0 );	break;

				}
			}
		}
	}
	else if ( nSoldiersStanding==0 ) {
		victory.SetText( "Defeat." );
	}
	else {
		victory.SetText( "Mission Summary:" );
	}
	victory.SetPos( X_ORIGIN[DESC], yPos );
	yPos += SPACING;

	const char* text[TEXT_ROW] = { "Soldiers standing",  "Soldiers down",
		                           "Aliens survived", "Aliens killed", 
								   "Civs Saved", "Civs Killed" };

	int value[TEXT_ROW]		   = {	nSoldiersStanding, nSoldiersDown,
									nAliensAlive, nAliensKIA,
									nCivsAlive, nCivsKIA
								 };

	// Lose points for soldiers killed,
	// Gain points for aliens killed,
	// Gain + Lose points for civs

	int score[3] = { 0 };

	for( int i=0; i<MAX_TERRANS; ++i ) {
		if ( d->soldiers[i].InUse() && !d->soldiers[i].IsAlive() ) {
			score[0] -= d->soldiers[i].GetStats().ScoreLevel();
		}
	}
	for( int i=0; i<MAX_ALIENS; ++i ) {
		if ( d->aliens[i].InUse() && !d->aliens[i].IsAlive() ) {
			score[1] += d->aliens[i].GetStats().ScoreLevel();
		}
	}
	if ( !d->dayTime ) {
		// 50% bonus for night.
		score[1] = score[1]*3/2;
	}
	// Adjust for odds, based on the starting out numbers.
	if ( nSoldiers ) {
		score[1] = score[1] * nAliens / nSoldiers;
	}

	for( int i=0; i<MAX_CIVS; ++i ) {
		if ( d->civs[i].InUse() ) {
			if ( d->civs[i].IsAlive() )
				score[2] += d->civs[i].GetStats().ScoreLevel();
			else
				score[2] -= d->civs[i].GetStats().ScoreLevel();
		}
	}

	for( int i=0; i<TEXT_ROW; ++i ) {
		textTable[i*TEXT_COL].SetText( text[i] );
		textTable[i*TEXT_COL].SetPos( X_ORIGIN[DESC], yPos );

		CStr<16> sBuf = value[i];
		textTable[i*TEXT_COL+1].SetText( sBuf.c_str() );
		textTable[i*TEXT_COL+1].SetPos( X_ORIGIN[COUNT], yPos );

		if ( i&1 ) {
			sBuf = score[i/2];
			textTable[i*TEXT_COL+2].SetText( sBuf.c_str() );
			textTable[i*TEXT_COL+2].SetPos( X_ORIGIN[SCORE], yPos );
		}
		yPos += SPACING;
	}

	// If the tech isn't high enough, can't use cells and anti
	const Research* research = game->GetResearch();
	static const char* remove[2] = { "Cell", "Anti" };
	for( int i=0; i<2; ++i ) {
		if ( research->GetStatus( remove[i] ) != Research::TECH_RESEARCH_COMPLETE ) {
			const ItemDef* itemDef = game->GetItemDefArr().Query( remove[i] );

			while( d->storage->Contains( itemDef ) ) {
				Item item;
				d->storage->RemoveItem( itemDef, &item );
			}
		}
	}

	int row=0;
	for( int i=0; i<EL_MAX_ITEM_DEFS; ++i ) {
		if ( row == ITEM_NUM )
			break;

		const ItemDef* itemDef = game->GetItemDefArr().GetIndex(i);
		if ( !itemDef )
			continue;

		if ( d->storage->GetCount(i) ) {
			char buf[30];
			const char* display = itemDef->displayName.c_str();

			int count =  d->storage->GetCount(i);
			SNPrintf( buf, 30, "%s +%d", display, count );

			items[row].Init( &gamui2D );
			items[row].SetPos( X_ORIGIN[ITEM], Y_ORIGIN + (float)row*SPACING );
			items[row].SetText( buf );
			++row;
		}
	}

	CStr<16> totalBuf = (score[0]+score[1]+score[2]);
	totalScoreValue.Init( &gamui2D );
	totalScoreValue.SetPos( X_ORIGIN[SCORE], yPos );
	totalScoreValue.SetText( totalBuf.c_str() );

	totalScoreLabel.Init( &gamui2D );
	totalScoreLabel.SetPos( X_ORIGIN[DESC], yPos );
	totalScoreLabel.SetText( "Total Score" );

	const gamui::ButtonLook& look = game->GetButtonLook( Game::GREEN_BUTTON );
	okayButton.Init( &gamui2D, look );
	okayButton.SetText( "OK" );
	okayButton.SetPos( 400, (float)(engine->GetScreenport().UIHeight() - (GAME_BUTTON_SIZE + 5)) );
	okayButton.SetSize( GAME_BUTTON_SIZE_F, GAME_BUTTON_SIZE_F );
}


TacticalEndScene::~TacticalEndScene()
{
}


void TacticalEndScene::Activate()
{
	GetEngine()->SetMap( 0 );
}



void TacticalEndScene::DrawHUD()
{
}


void TacticalEndScene::Tap(	int action, 
							const grinliz::Vector2F& screen,
							const grinliz::Ray& world )
{
	Vector2F ui;
	GetEngine()->GetScreenport().ViewToUI( screen, &ui );

	const gamui::UIItem* item = 0;
	if ( action == GAME_TAP_DOWN ) {
		gamui2D.TapDown( ui.x, ui.y );
		return;
	}
	else if ( action == GAME_TAP_CANCEL ) {
		gamui2D.TapCancel();
		return;
	}
	else if ( action == GAME_TAP_UP ) {
		item = gamui2D.TapUp( ui.x, ui.y );
	}

	if ( item == &okayButton ) {
		game->PopScene();
		game->PushScene( Game::UNIT_SCORE_SCENE, new TacticalEndSceneData( *data ) );
	}
}

