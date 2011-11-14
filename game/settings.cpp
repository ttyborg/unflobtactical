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

#include "settings.h"
#include "../tinyxml/tinyxml.h"
#include "../engine/serialize.h"

using namespace grinliz;
SettingsManager* SettingsManager::instance = 0;

void SettingsManager::Create( const char* savepath )
{
	GLASSERT( instance == 0 );
	instance = new SettingsManager( savepath );
}


void SettingsManager::Destroy()
{
	delete instance;
	instance = 0;
}


SettingsManager::SettingsManager( const char* savepath )
{
	
	path = savepath;
	path += "settings.xml";

	// Set defaults.
	audioOn = 1;
	suppressCrashLog = 0;
	playerAI = 0;
	battleShipParty = 0;
	nWalkingMaps = 1;
	confirmMove = 
		#ifdef ANDROID_NDK
			1;
		#else
			0;
		#endif
	allowDrag = true;
	testAlien = 0;

	// Parse actuals.
	TiXmlDocument doc;
	if ( doc.LoadFile( path.c_str() ) ) {
		TiXmlElement* root = doc.RootElement();
		if ( root ) {
			root->QueryIntAttribute( "audioOn", &audioOn );
			root->QueryIntAttribute( "suppressCrashLog", &suppressCrashLog );
			root->QueryIntAttribute( "playerAI", &playerAI );
			root->QueryIntAttribute( "battleShipParty", &battleShipParty );
			root->QueryIntAttribute( "nWalkingMaps", &nWalkingMaps );
			root->QueryBoolAttribute( "confirmMove", &confirmMove );
			root->QueryBoolAttribute( "allowDrag", &allowDrag );
			root->QueryIntAttribute( "testAlien", &testAlien );
			currentMod = "";
			if ( root->Attribute( "currentMod" ) ) {
				currentMod = root->Attribute( "currentMod" );
			}
		}
	}
	nWalkingMaps = grinliz::Clamp( nWalkingMaps, 1, 2 );
}


void SettingsManager::SetNumWalkingMaps( int maps )
{
	maps = grinliz::Clamp( maps, 1, 2 );
	if ( maps != nWalkingMaps ) {
		nWalkingMaps = maps;
		Save();
	}
}


void SettingsManager::SetCurrentModName( const GLString& name )
{
	if ( name != currentMod ) {
		currentMod = name;
		Save();
	}
}


void SettingsManager::SetConfirmMove( bool confirm )
{
	if ( confirm != confirmMove ) {
		confirmMove = confirm;
		Save();
	}
}


void SettingsManager::SetAudioOn( bool _value )
{
	int value = _value ? 1 : 0;

	if ( audioOn != value ) {
		audioOn = value;
		Save();
	}
}


void SettingsManager::SetAllowDrag( bool allow ) 
{
	if ( allowDrag != allow ) {
		allowDrag = allow;
		Save();
	}
}


void SettingsManager::Save()
{
	FILE* fp = fopen( path.c_str(), "w" );
	if ( fp ) {
		XMLUtil::OpenElement( fp, 0, "Settings" );

		XMLUtil::Attribute( fp, "currentMod", currentMod.c_str() );
		XMLUtil::Attribute( fp, "audioOn", audioOn );
		XMLUtil::Attribute( fp, "suppressCrashLog", suppressCrashLog );
		XMLUtil::Attribute( fp, "playerAI", playerAI );
		XMLUtil::Attribute( fp, "battleShipParty", battleShipParty );
		XMLUtil::Attribute( fp, "nWalkingMaps", nWalkingMaps );
		XMLUtil::Attribute( fp, "confirmMove", confirmMove );
		XMLUtil::Attribute( fp, "allowDrag", allowDrag );
		XMLUtil::Attribute( fp, "testAlien", testAlien );

		XMLUtil::SealCloseElement( fp );

		fclose( fp );
	}
}
