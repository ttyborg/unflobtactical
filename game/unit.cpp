#include "unit.h"
#include "game.h"
#include "../engine/engine.h"
#include "material.h"
#include "../tinyxml/tinyxml.h"
#include "../grinliz/glstringutil.h"

using namespace grinliz;

// Name first name length: 6
const char* gMaleFirstNames[16] = 
{
	"Lee",
	"Jeff",
	"Sean",
	"Vlad",
	"Arnold",
	"Max",
	"Otto",
	"James",
	"Jason",
	"John",
	"Hans",
	"Rupen",
	"Ping",
	"Yoshi",
	"Ishmael",
	"Roy",
};

// Name first name length: 6
const char* gFemaleFirstNames[16] = 
{
	"Rayne",
	"Anne",
	"Jade",
	"Suzie",
	"Greta",
	"Lilith",
	"Luna",
	"Riko",
	"Jane",
	"Sarah",
	"Jane",
	"Ashley",
	"Liara",
	"Rissa",
	"Lea",
	"Dahlia"
};


// Name last name length: 6
const char* gLastNames[16] = 
{
	"Payne",
	"Havok",
	"Fury",
	"Scharz",
	"Bourne",
	"Bond",
	"Smith",
	"Andson",
	"Dekard",
	"Jones",
	"Solo",
	"Skye",
	"Picard",
	"Kirk",
	"Spock",
	"Willis"
};


const char* gRank[NUM_RANKS] = {
	"Rki",
	"Prv",
	"Sgt",
	"Maj",
	"Cpt",
};


U32 Unit::GetValue( int which ) const
{
	const int NBITS[] = { 1, 2, 2, 4, 4 };

	int i;
	U32 shift = 0;
	for( i=0; i<which; ++i ) {
		shift += NBITS[i];
	}
	U32 mask = (1<<NBITS[which])-1;
	return (body>>shift) & mask;
}


const char* Unit::FirstName() const
{
	const char* str = "";
	if ( team == TERRAN_TEAM ) {
		if ( Gender() == MALE )
			str = gMaleFirstNames[ GetValue( FIRST_NAME ) ];
		else
			str = gFemaleFirstNames[ GetValue( FIRST_NAME ) ];
	}
	GLASSERT( strlen( str ) <= 6 );
	return str;
}


const char* Unit::LastName() const
{
	const char* str = "";
	if ( team == TERRAN_TEAM ) {
		str = gLastNames[ GetValue( LAST_NAME ) ];
	}
	GLASSERT( strlen( str ) <= 6 );
	return str;
}


const char* Unit::Rank() const
{
	GLASSERT( stats.Rank() >=0 && stats.Rank() < NUM_RANKS ); 
	return gRank[ stats.Rank() ];
}


/*static*/ void Unit::GenStats( int team, int type, int seed, Stats* s )
{
	Vector2I str, dex, psy;

	switch ( team ) {
		case TERRAN_TEAM:
			str.Set( TRAIT_TERRAN_LOW, TRAIT_TERRAN_HIGH );
			dex.Set( TRAIT_TERRAN_LOW, TRAIT_TERRAN_HIGH );
			psy.Set( TRAIT_TERRAN_LOW, TRAIT_TERRAN_HIGH );
			break;

		case ALIEN_TEAM:
			{
				switch( type ) {
					case 0:
						// Grey. Similar to human, a little weaker & smarter.
						str.Set( TRAIT_TERRAN_LOW/2, TRAIT_TERRAN_HIGH/2 );
						dex.Set( TRAIT_TERRAN_LOW, TRAIT_TERRAN_HIGH );
						psy.Set( TRAIT_TERRAN_LOW*15/10, TRAIT_TERRAN_HIGH*15/10 );
						break;
					case 1:
						// Mind-slayer
						str.Set( TRAIT_TERRAN_LOW/2, TRAIT_TERRAN_HIGH/2 );
						dex.Set( TRAIT_TERRAN_LOW, TRAIT_TERRAN_HIGH );
						psy.Set( TRAIT_TERRAN_HIGH, TRAIT_MAX );
						break;
					case 2:
						// Assault
						str.Set( TRAIT_TERRAN_LOW*15/10, TRAIT_MAX );
						dex.Set( TRAIT_TERRAN_LOW*15/10, TRAIT_TERRAN_HIGH*15/10 );
						psy.Set( TRAIT_TERRAN_LOW, TRAIT_TERRAN_HIGH );
						break;
					case 3:
						// Elite.
						str.Set( TRAIT_TERRAN_LOW, TRAIT_TERRAN_HIGH );
						dex.Set( TRAIT_TERRAN_LOW, TRAIT_TERRAN_HIGH );
						psy.Set( TRAIT_TERRAN_HIGH, TRAIT_MAX );
						break;
					default:
						GLASSERT( 0 );
						break;
				}
			}
			break;

		case CIV_TEAM:
			str.Set( TRAIT_TERRAN_LOW/2, TRAIT_TERRAN_HIGH/2 );
			dex.Set( TRAIT_TERRAN_LOW/2, TRAIT_TERRAN_HIGH/2 );
			psy.Set( TRAIT_TERRAN_LOW/2, TRAIT_TERRAN_HIGH/2 );
			break;

		default:
			GLASSERT( 0 );
			break;
	}

	Random r( seed );
	s->SetSTR( Stats::GenStat( &r, str.x, str.y ) );
	s->SetDEX( Stats::GenStat( &r, dex.x, dex.y ) );
	s->SetPSY( Stats::GenStat( &r, psy.x, psy.y ) );
}


void Unit::Init(	Engine* engine, 
					Game* game, 
					int team,	 
					int p_status,
					int alienType,
					int body )
{
	GLASSERT( this->status == STATUS_NOT_INIT );
	this->engine = engine;
	this->game = game;
	this->team = team;
	this->status = p_status;
	this->type = alienType;
	this->body = body;
	GLASSERT( type >= 0 && type < 4 );

	weapon = 0;
	visibilityCurrent = false;
	userDone = false;
	ai = AI_NORMAL;

	CreateModel();
}


Unit::~Unit()
{
	Free();
}


void Unit::Free()
{
	if ( status == STATUS_NOT_INIT )
		return;

	if ( model ) {
		engine->FreeModel( model );
		model = 0;
	}
	if ( weapon ) {
		engine->FreeModel( weapon );
		weapon = 0;
	}
	status = STATUS_NOT_INIT;
}


void Unit::SetMapPos( int x, int z )
{
	GLASSERT( status != STATUS_NOT_INIT );
	GLASSERT( model );

	grinliz::Vector3F p = { (float)x + 0.5f, 0.0f, (float)z + 0.5f };
	model->SetPos( p );
	UpdateWeapon();
	visibilityCurrent = false;
}


void Unit::SetYRotation( float rotation )
{
	GLASSERT( status != STATUS_NOT_INIT );
	GLASSERT( model );
	if ( IsAlive() )
		model->SetRotation( rotation );
	UpdateWeapon();
	visibilityCurrent = false;
}


void Unit::SetPos( const grinliz::Vector3F& pos, float rotation )
{
	GLASSERT( status != STATUS_NOT_INIT );
	GLASSERT( model );
	model->SetPos( pos );
	if ( IsAlive() )
		model->SetRotation( rotation );
	UpdateWeapon();
	visibilityCurrent = false;
}


Item* Unit::GetWeapon()
{
	GLASSERT( status == STATUS_ALIVE );
	return inventory.ArmedWeapon();
}


const Item* Unit::GetWeapon() const
{
	GLASSERT( status == STATUS_ALIVE );
	return inventory.ArmedWeapon();
}

Inventory* Unit::GetInventory()
{
	return &inventory;
}


const Inventory* Unit::GetInventory() const
{
	return &inventory;
}


void Unit::UpdateInventory() 
{
	GLASSERT( status != STATUS_NOT_INIT );

	if ( weapon ) {
		engine->FreeModel( weapon );
	}
	weapon = 0;	// don't render non-weapon items

	if ( IsAlive() ) {
		const Item* weaponItem = inventory.ArmedWeapon();
		if ( weaponItem  ) {
			weapon = engine->AllocModel( weaponItem->GetItemDef()->resource );
			weapon->SetFlag( Model::MODEL_NO_SHADOW );
			weapon->SetFlag( Model::MODEL_MAP_TRANSPARENT );
		}
		UpdateWeapon();

		// FIXME: delicate side effect. If armor changes it re-calcs the
		// hp and heals the unit. Never set the armor in battle.
		int armorAmount = inventory.ArmorAmount();
		if ( armorAmount != stats.Armor() ) {
			GLASSERT( 0 );	// should fire in screens before battle...
			stats.SetArmor( armorAmount );
		}
	}
}


void Unit::UpdateWeapon()
{
	GLASSERT( status == STATUS_ALIVE );
	if ( weapon && model ) {
		Matrix4 r;
		r.SetYRotation( model->GetRotation() );

		Vector4F mPos, gPos, pos4;

		mPos.Set( model->Pos(), 1 );
		gPos.Set( model->GetResource()->header.trigger, 1.0 );
		pos4 = mPos + r*gPos;
		weapon->SetPos( pos4.x, pos4.y, pos4.z );
		weapon->SetRotation( model->GetRotation() );
	}
}


void Unit::CalcPos( grinliz::Vector3F* vec ) const
{
	GLASSERT( status != STATUS_NOT_INIT );
	GLASSERT( model );

	*vec = model->Pos();
}


void Unit::CalcVisBounds( grinliz::Rectangle2I* b ) const
{
	GLASSERT( status != STATUS_NOT_INIT );
	GLASSERT( model );

	Vector2I p;
	CalcMapPos( &p, 0 );
	b->Set( Max( 0, p.x-MAX_EYESIGHT_RANGE ), 
			Max( 0, p.y-MAX_EYESIGHT_RANGE ),
			Min( p.x+MAX_EYESIGHT_RANGE, MAP_SIZE-1),
			Min( p.y+MAX_EYESIGHT_RANGE, MAP_SIZE-1) );
}


void Unit::CalcMapPos( grinliz::Vector2I* vec, float* rot ) const
{
	GLASSERT( status != STATUS_NOT_INIT );
	GLASSERT( model );

	// Account that model can be incrementally moved when animating.
	if ( vec ) {
		vec->x = LRintf( model->X() - 0.5f );
		vec->y = LRintf( model->Z() - 0.5f );
	}
	if ( rot ) {
		float r = model->GetRotation() + 45.0f/2.0f;
		int ir = (int)( r / 45.0f );
		*rot = (float)(ir*45);
	}
}


void Unit::Kill( Map* map )
{
	GLASSERT( status == STATUS_ALIVE );
	GLASSERT( model );
	float r = model->GetRotation();
	Vector3F pos = model->Pos();

	Free();
	status = STATUS_DEAD;
	
	CreateModel();

	stats.ZeroHP();
	model->SetRotation( 0 );	// set the y rotation to 0 for the "splat" icons
	model->SetPos( pos );
	model->SetFlag( Model::MODEL_NO_SHADOW );
	visibilityCurrent = false;

	if ( map && !inventory.Empty() ) {
		Vector2I pos = Pos();
		Storage* storage = map->LockStorage( pos.x, pos.y );
		if ( !storage ) {
			storage = new Storage();
		}
		for( int i=0; i<Inventory::NUM_SLOTS; ++i ) {
			Item* item = inventory.AccessItem( i );
			if ( item->IsSomething() ) {
				storage->AddItem( *item );
				item->Clear();
			}
		}
		map->ReleaseStorage( pos.x, pos.y, storage, game->GetItemDefArr() );
	}
}


void Unit::DoDamage( const DamageDesc& damage, Map* map )
{
	GLASSERT( status != STATUS_NOT_INIT );
	if ( status == STATUS_ALIVE ) {
		// FIXME: account for armor
		stats.DoDamage( (int)damage.Total() );
		if ( !stats.HP() ) {
			Kill( map );
			visibilityCurrent = false;
		}
	}
}


void Unit::NewTurn()
{
	if ( status == STATUS_ALIVE ) {
		stats.RestoreTU();
		userDone = false;
	}
}


void Unit::CreateModel()
{
	GLASSERT( status != STATUS_NOT_INIT );

	const ModelResource* resource = 0;
	ModelResourceManager* modman = ModelResourceManager::Instance();
	bool alive = IsAlive();

	if ( alive ) {
		switch ( team ) {
			case TERRAN_TEAM:
				resource = modman->GetModelResource( ( Gender() == MALE ) ? "maleMarine" : "femaleMarine" );
				break;

			case CIV_TEAM:
				resource = modman->GetModelResource( ( Gender() == MALE ) ? "maleCiv" : "femaleCiv" );
				break;

			case ALIEN_TEAM:
				{
					switch( AlienType() ) {
						case 0:	resource = modman->GetModelResource( "alien0" );	break;
						case 1:	resource = modman->GetModelResource( "alien1" );	break;
						case 2:	resource = modman->GetModelResource( "alien2" );	break;
						case 3:	resource = modman->GetModelResource( "alien3" );	break;
						default: GLASSERT( 0 );	break;
					}
				}
				break;
			
			default:
				GLASSERT( 0 );
				break;
		}
		if ( resource ) {
			model = engine->AllocModel( resource );
			model->SetFlag( Model::MODEL_MAP_TRANSPARENT );
		}
	}
	else {
		if ( team != CIV_TEAM ) {
			model = engine->AllocModel( modman->GetModelResource( "unitplate" ) );
			model->SetFlag( Model::MODEL_MAP_TRANSPARENT );
			model->SetFlag( Model::MODEL_NO_SHADOW );

			const Texture* texture = TextureManager::Instance()->GetTexture( "particleQuad" );
			model->SetTexture( texture );

			if ( team == TERRAN_TEAM ) {
				model->SetTexXForm( 0.25, 0.25, 0.75f, 0.0f );
			}
			else {
				model->SetTexXForm( 0.25, 0.25, 0.75f, 0.25f );
			}
		}
	}
	UpdateModel();
}


void Unit::UpdateModel()
{
	GLASSERT( status != STATUS_NOT_INIT );

	if ( IsAlive() && model && team == TERRAN_TEAM ) {
		int armor = 0;
		int hair = GetValue( HAIR );
		int skin = GetValue( SKIN );
		model->SetSkin( armor, skin, hair );
	}
}


void Unit::Save( TiXmlElement* doc ) const
{
	if ( status != STATUS_NOT_INIT ) {
		TiXmlElement* unitElement = new TiXmlElement( "Unit" );
		doc->LinkEndChild( unitElement );

		unitElement->SetAttribute( "team", team );
		unitElement->SetAttribute( "type", type );
		unitElement->SetAttribute( "status", status );
		unitElement->SetAttribute( "body", body );
		unitElement->SetDoubleAttribute( "modelX", model->Pos().x );
		unitElement->SetDoubleAttribute( "modelZ", model->Pos().z );
		unitElement->SetDoubleAttribute( "yRot", model->GetRotation() );

		stats.Save( unitElement );
		inventory.Save( unitElement );
	}
}


void Unit::Load( const TiXmlElement* ele, Engine* engine, Game* game  )
{
	Free();

	// Give ourselves a random body based on the element address. 
	// Hard to get good randomness here (gender bit keeps being
	// consistent.) A combination of the element address and 'this'
	// seems to work pretty well.
	UPTR key[2] = { (UPTR)ele, (UPTR)this };
	Random random( Random::Hash( key, sizeof(UPTR)*2 ));
	random.Rand();
	random.Rand();

	team = TERRAN_TEAM;
	body = random.Rand();
	Vector3F pos = { 0, 0, 0 };
	float rot = 0;
	type = 0;
	int a_status = 0;
	ai = AI_NORMAL;

	GLASSERT( StrEqual( ele->Value(), "Unit" ) );
	ele->QueryIntAttribute( "status", &a_status );
	GLASSERT( a_status == STATUS_NOT_INIT || a_status == STATUS_ALIVE || a_status == STATUS_DEAD );

	if ( a_status != STATUS_NOT_INIT ) {
		ele->QueryValueAttribute( "team", &team );
		ele->QueryValueAttribute( "type", &type );
		ele->QueryValueAttribute( "body", &body );
		ele->QueryValueAttribute( "modelX", &pos.x );
		ele->QueryValueAttribute( "modelZ", &pos.z );
		ele->QueryValueAttribute( "yRot", &rot );

		GenStats( team, type, body, &stats );		// defaults if not provided
		stats.Load( ele );
		inventory.Load( ele, engine, game );
		// connect armor to the stats:
		stats.SetArmor( inventory.ArmorAmount() );

		Init( engine, game, team, a_status, type, body );
		if ( StrEqual( ele->Attribute( "ai" ), "guard" ) ) {
			ai = AI_GUARD;
		}

		if ( model ) {
			if ( pos.x == 0.0f ) {
				GLASSERT( pos.z == 0.0f );

				Vector2I pi;
				engine->GetMap()->PopLocation( team, ai == AI_GUARD, &pi, &rot );
				pos.Set( (float)pi.x+0.5f, 0.0f, (float)pi.y+0.5f );
			}

			model->SetPos( pos );
			if ( IsAlive() )
				model->SetRotation( rot );
		}
		UpdateInventory();

		GLOUTPUT(( "Unit loaded: team=%d STR=%d DEX=%d PSY=%d rank=%d armor=%d hp=%d acc=%.2f\n",
					this->team,
					this->stats.STR(),
					this->stats.DEX(),
					this->stats.PSY(),
					this->stats.Rank(),
					this->stats.Armor(),
					this->stats.HP(),
					this->stats.Accuracy() ));
	}
}


float Unit::AngleBetween( const Vector2I& p1, bool quantize ) const 
{
	Vector2I p0;
	CalcMapPos( &p0, 0 );

	//target->CalcMapPos( &p1, 0 );

	float angle = atan2( (float)(p1.x-p0.x), (float)(p1.y-p0.y) );
	angle = ToDegree( angle );

	if ( quantize ) {
		if ( angle < 0.0f ) angle += 360.0f;
		if ( angle >= 360.0f ) angle -= 360.0f;

		int r = (int)( (angle+45.0f/2.0f) / 45.0f );
		return (float)(r*45.0f);
	}
	return angle;
}


bool Unit::CanFire( int select, int type ) const
{
	int nShots = (type == AUTO_SHOT) ? 3 : 1;
	float tu = FireTimeUnits( select, type );

	if ( tu > 0.0f && tu <= stats.TU() ) {
		int rounds = inventory.CalcClipRoundsTotal( GetWeapon()->IsWeapon()->weapon[select].clipItemDef );
		if ( rounds >= nShots ) 
			return true;
	}
	return false;
}


float Unit::FireTimeUnits( int select, int type ) const
{
	float time = 0.0f;
	const Item* item = GetWeapon();
	if ( item && item->IsWeapon() ) {
		const WeaponItemDef* weaponItemDef = item->IsWeapon();
		time = weaponItemDef->TimeUnits( select, type );
	}
	// Note: don't adjust for stats. TU is based on DEX. Adjusting again double-applies.
	return time;
}


bool Unit::FireModeToType( int mode, int* select, int* type ) const
{
	if ( GetWeapon() && GetWeapon()->IsWeapon() ) {
		GetWeapon()->IsWeapon()->FireModeToType( mode, select, type );
		return true;
	}
	return false;
}


// Used by the AI
bool Unit::FireStatistics( int select, int type, float distance, 
						   float* chanceToHit, float* chanceAnyHit, float* tu, float* damagePerTU ) const
{
	*chanceToHit = 0.0f;
	*chanceAnyHit = 0.0f;
	*tu = 0.0f;
	*damagePerTU = 0.0f;
	
	float damage;

	if ( GetWeapon() && GetWeapon()->IsWeapon() ) {
		const WeaponItemDef* wid = GetWeapon()->IsWeapon();
		if ( wid->SupportsType( select, type ) ) {
			*tu = wid->TimeUnits( select, type );
			return GetWeapon()->IsWeapon()->FireStatistics(	select, type, 
															stats.Accuracy(), 
															distance, 
															chanceToHit, 
															chanceAnyHit, 
															&damage, 
															damagePerTU );
		}
	}
	return false;
}

