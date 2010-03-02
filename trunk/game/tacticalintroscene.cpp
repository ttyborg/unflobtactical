#include "tacticalintroscene.h"
#include "../engine/uirendering.h"
#include "../engine/engine.h"
#include "game.h"

#include <string>
using namespace std;

TacticalIntroScene::TacticalIntroScene( Game* _game ) : Scene( _game )
{
	showNewChoices = false;

	Engine* engine = GetEngine();
	background = new UIImage( engine->GetScreenport() );

	// -- Background -- //
	const Texture* bg = TextureManager::Instance()->GetTexture( "intro" );
	GLASSERT( bg );
	background->Init( bg, 480, 320 );

	// -- Buttons -- //
	{
		buttons = new UIButtonBox( engine->GetScreenport() );

		int icons[] = { ICON_GREEN_BUTTON, ICON_GREEN_BUTTON, ICON_GREEN_BUTTON };
		const char* iconText[] = { "test", "new", "continue" };
		buttons->InitButtons( icons, 3 );
		buttons->SetOrigin( 42, 40 );
		buttons->SetButtonSize( 120, 50 );
		buttons->SetText( iconText );
	}

	// -- New Game choices -- //
	{
		choices = new UIButtonGroup( engine->GetScreenport() );
		
		const int XSIZE = 55;
		const int YSIZE = 55;
		int icons[] = { ICON_GREEN_BUTTON, ICON_GREEN_BUTTON_DOWN,
						ICON_GREEN_BUTTON, ICON_GREEN_BUTTON_DOWN, ICON_GREEN_BUTTON,
						ICON_GREEN_BUTTON_DOWN, ICON_GREEN_BUTTON,
						ICON_GREEN_BUTTON, ICON_GREEN_BUTTON_DOWN, ICON_GREEN_BUTTON,
						ICON_GREEN_BUTTON_DOWN, ICON_GREEN_BUTTON,
						ICON_BLUE_BUTTON
		};
		const char* iconText[] = {
						"4", "8",
						"Low", "Med", "Hi",
						"8", "16",
						"Low", "Med", "Hi",
						"Day", "Ngt",
						"GO!"
		};

		choices->SetOrigin( 280, 40 );

		choices->InitButtons( icons, 13 );
		choices->SetText( iconText );

		choices->SetPos( SQUAD_4, XSIZE*0, YSIZE*4 );
		choices->SetPos( SQUAD_8, XSIZE*1, YSIZE*4 );

		choices->SetPos( TERRAN_LOW, XSIZE*0, YSIZE*3 );
		choices->SetPos( TERRAN_MED, XSIZE*1, YSIZE*3 );
		choices->SetPos( TERRAN_HIGH, XSIZE*2, YSIZE*3 );

		choices->SetPos( ALIEN_8, XSIZE*0, YSIZE*2 );
		choices->SetPos( ALIEN_16, XSIZE*1, YSIZE*2 );

		choices->SetPos( ALIEN_LOW, XSIZE*0, YSIZE*1 );
		choices->SetPos( ALIEN_MED, XSIZE*1, YSIZE*1 );
		choices->SetPos( ALIEN_HIGH, XSIZE*2, YSIZE*1 );

		choices->SetPos( TIME_DAY, XSIZE*0, YSIZE*0 );
		choices->SetPos( TIME_NIGHT, XSIZE*1, YSIZE*0 );

		choices->SetPos( GO_NEW_GAME, XSIZE*25/10, -YSIZE*5/10 );
	}

	// Is there a current game?
	const std::string& savePath = game->GameSavePath();
	buttons->SetEnabled( CONTINUE_GAME, false );
	FILE* fp = fopen( savePath.c_str(), "r" );
	if ( fp ) {
		fseek( fp, 0, SEEK_END );
		unsigned long len = ftell( fp );
		if ( len > 100 ) {
			// 20 ignores empty XML noise (hopefully)
			buttons->SetEnabled( CONTINUE_GAME, true );
		}
		fclose( fp );
	}
}


TacticalIntroScene::~TacticalIntroScene()
{
	delete background;
	delete buttons;
	delete choices;
}


void TacticalIntroScene::DrawHUD()
{
	background->Draw();
	if ( showNewChoices ) {
		choices->Draw();
	}
	else {
		buttons->Draw();
	}
}


void TacticalIntroScene::Tap(	int count, 
								const grinliz::Vector2I& screen,
								const grinliz::Ray& world )
{
	int ux, uy;
	GetEngine()->GetScreenport().ViewToUI( screen.x, screen.y, &ux, &uy );

	if ( !showNewChoices ) {
		int tap = buttons->QueryTap( ux, uy );
		switch ( tap ) {
			case TEST_GAME:			game->loadRequested = 2;			break;
			case NEW_GAME:			showNewChoices = true;				break;
			case CONTINUE_GAME:		game->loadRequested = 0;			break;

			default:
				break;
		}
	}
	else {
		int tap = choices->QueryTap( ux, uy );
		switch ( tap ) {
			case SQUAD_4:	
			case SQUAD_8:
				choices->SetButton( SQUAD_4, (tap==SQUAD_4) ? ICON_GREEN_BUTTON_DOWN : ICON_GREEN_BUTTON );
				choices->SetButton( SQUAD_8, (tap==SQUAD_8) ? ICON_GREEN_BUTTON_DOWN : ICON_GREEN_BUTTON );
				break;

			case TERRAN_LOW:	
			case TERRAN_MED:
			case TERRAN_HIGH:
				choices->SetButton( TERRAN_LOW, (tap==TERRAN_LOW) ? ICON_GREEN_BUTTON_DOWN : ICON_GREEN_BUTTON );
				choices->SetButton( TERRAN_MED, (tap==TERRAN_MED) ? ICON_GREEN_BUTTON_DOWN : ICON_GREEN_BUTTON );
				choices->SetButton( TERRAN_HIGH, (tap==TERRAN_HIGH) ? ICON_GREEN_BUTTON_DOWN : ICON_GREEN_BUTTON );
				break;

			case ALIEN_8:	
			case ALIEN_16:
				choices->SetButton( ALIEN_8,  (tap==ALIEN_8) ? ICON_GREEN_BUTTON_DOWN : ICON_GREEN_BUTTON );
				choices->SetButton( ALIEN_16, (tap==ALIEN_16) ? ICON_GREEN_BUTTON_DOWN : ICON_GREEN_BUTTON );
				break;

			case ALIEN_LOW:	
			case ALIEN_MED:
			case ALIEN_HIGH:
				choices->SetButton( ALIEN_LOW, (tap==ALIEN_LOW) ? ICON_GREEN_BUTTON_DOWN : ICON_GREEN_BUTTON );
				choices->SetButton( ALIEN_MED, (tap==ALIEN_MED) ? ICON_GREEN_BUTTON_DOWN : ICON_GREEN_BUTTON );
				choices->SetButton( ALIEN_HIGH, (tap==ALIEN_HIGH) ? ICON_GREEN_BUTTON_DOWN : ICON_GREEN_BUTTON );
				break;

			case TIME_DAY:	
			case TIME_NIGHT:
				choices->SetButton( TIME_DAY,  (tap==TIME_DAY) ? ICON_GREEN_BUTTON_DOWN : ICON_GREEN_BUTTON );
				choices->SetButton( TIME_NIGHT, (tap==TIME_NIGHT) ? ICON_GREEN_BUTTON_DOWN : ICON_GREEN_BUTTON );
				break;

			case GO_NEW_GAME:
				game->loadRequested = 1;
				WriteXML( &game->newGameXML );
				break;
		}
	}
	if ( game->loadRequested >= 0 ) {
		game->PopScene();
		game->PushScene( Game::BATTLE_SCENE, 0 );
	}
}


void TacticalIntroScene::WriteXML( std::string* _xml )
{
	std::string& xml = *_xml;

	xml.clear();
	xml += "<Game><Scene id='0' />";
	if ( choices->GetButton( TIME_NIGHT ) == ICON_GREEN_BUTTON_DOWN )
		xml += "<BattleScene dayTime='0' />";
	else
		xml += "<BattleScene dayTime='1' />";
	xml += game->AccessTextResource( "new_map0" );
	xml += "<Units>";

	const char* squad = game->AccessTextResource( "new_squad_LA" );
	if ( choices->GetButton( TERRAN_MED ) == ICON_GREEN_BUTTON_DOWN )
		squad = game->AccessTextResource( "new_squad_MA" );
	else if ( choices->GetButton( TERRAN_HIGH ) == ICON_GREEN_BUTTON_DOWN )
		squad = game->AccessTextResource( "new_squad_HA" );

	xml += squad;
	if ( choices->GetButton( SQUAD_8 ) == ICON_GREEN_BUTTON_DOWN ) {
		xml += squad;
	}

	const char* alien = game->AccessTextResource( "new_alien_LA" );
	if ( choices->GetButton( ALIEN_MED ) == ICON_GREEN_BUTTON_DOWN )
		alien = game->AccessTextResource( "new_alien_MA" );
	else if ( choices->GetButton( ALIEN_HIGH ) == ICON_GREEN_BUTTON_DOWN )
		alien = game->AccessTextResource( "new_alien_HA" );

	xml += alien;
	if ( choices->GetButton( ALIEN_16 ) == ICON_GREEN_BUTTON_DOWN ) {
		xml += alien;
	}

	xml += "</Units>";
	xml += "</Game>";

}
