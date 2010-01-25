#include "item.h"
#include "material.h"
#include "game.h"
#include "../engine/serialize.h"
#include "gamelimits.h"
#include "../engine/particleeffect.h"
#include "../engine/particle.h"

using namespace grinliz;


bool WeaponItemDef::IsAlien() const 
{ 
	return weapon[0].clipItemDef->IsAlien(); 
}


void WeaponItemDef::RenderWeapon(	int select,
									ParticleSystem* system,
									const Vector3F& p0, 
									const Vector3F& p1,
									bool useImpact,
									U32 currentTime,
									U32* duration ) const
{
	enum { BEAM, TRAIL };
	enum { SPLASH, EXPLOSION };
	int first, second;

	const ClipItemDef* cid = weapon[select].clipItemDef;

	if ( cid->IsAlien() ) {
		first = BEAM;
		second = (weapon[select].flags & WEAPON_EXPLOSIVE) ? EXPLOSION : SPLASH;
	}
	else {
		first = (weapon[select].flags & WEAPON_EXPLOSIVE) ? TRAIL : BEAM;
		second = (weapon[select].flags & WEAPON_EXPLOSIVE) ? EXPLOSION : SPLASH;
	}

	// effects: 
	//		bolt then particle
	//		beam

	if ( first == BEAM ) {
		BoltEffect* bolt = (BoltEffect*) system->EffectFactory( "BoltEffect" );
		if ( !bolt ) { 
			bolt = new BoltEffect( system );
		}
		
		bolt->Clear();
		bolt->SetColor( cid->color );
		bolt->SetSpeed( cid->speed );
		bolt->SetLength( cid->length );
		bolt->SetWidth( cid->width );
		bolt->Init( p0, p1, currentTime );

		*duration = bolt->CalcDuration();
		system->AddEffect( bolt );
	}
	else if ( first == TRAIL ) {
		SmokeTrailEffect* trail = (SmokeTrailEffect*) system->EffectFactory( "SmokeTrailEffect" );
		if ( !trail ) {
			trail = new SmokeTrailEffect( system );
		}

		trail->Clear();
		Color4F c = { 1, 1, 1, 1 };
		trail->SetColor( c );
		trail->SetSpeed( cid->speed*0.4f );
		trail->Init( p0, p1, currentTime );
		*duration = trail->CalcDuration();
		system->AddEffect( trail );
	}
	else {
		GLASSERT( 0 );
	}

	if ( useImpact ) {

		ImpactEffect* impact = (ImpactEffect*) system->EffectFactory( "ImpactEffect" );
		if ( !impact ) {
			impact = new ImpactEffect( system );
		}

		Vector3F n = p0 - p1;
		n.Normalize();
		impact->Clear();
		impact->Init( p1, currentTime + *duration );
		impact->SetColor( cid->color );
		impact->SetNormal( n );
		if ( second == SPLASH ) {
			impact->SetRadius( 1.5f );
		}
		else {
			// explosion
			impact->SetRadius( 3.5f );
			impact->SetConfig( ParticleSystem::PARTICLE_SPHERE );

			// 2nd set of particles:
			ImpactEffect* impact2 = (ImpactEffect*) system->EffectFactory( "ImpactEffect" );
			if ( !impact2 ) {
				impact2 = new ImpactEffect( system );
			}
			impact2->Clear();
			impact2->Init( p1, currentTime + *duration + 250 );
			impact2->SetColor( cid->color );
			impact2->SetNormal( n );
			impact2->SetRadius( 3.5f );
			impact2->SetConfig( ParticleSystem::PARTICLE_SPHERE );
			system->AddEffect( impact2 );
		}
		system->AddEffect( impact );
	}
}


bool WeaponItemDef::CompatibleClip( const ItemDef* id, int* which ) const
{
	if ( id->IsClip() ) {
		if ( weapon[0].clipItemDef == id ) {
			*which = 0;
			return true;
		}
		else if ( weapon[1].clipItemDef == id ) {
			*which = 1;
			return true;
		}
	}
	return false;
}


void WeaponItemDef::DamageBase( int select, DamageDesc* d ) const
{
	GLASSERT( weapon[select].clipItemDef );
	*d = weapon[select].clipItemDef->dd;
	d->Scale( weapon[select].damage );
}


float WeaponItemDef::TimeUnits( int select, int type ) const
{
	GLASSERT( select == 0 || select == 1 );
	GLASSERT( type >= 0 && type < 3 );

	float s = 0.0f;
	switch ( type ) {
		case SNAP_SHOT:		
			s = TU_SNAP_SHOT;	
			break;
		case AUTO_SHOT:		
			if ( weapon[select].flags & WEAPON_AUTO )
				s = TU_AUTO_SHOT;	
			break;
		case AIMED_SHOT:	
			s = TU_AIMED_SHOT;	
			break;
		default:
			GLASSERT( 0 );
	}
	
	s *= speed;

	// Secondary weapon is slower:
	if ( select == 1 )
		s *= SECONDARY_SHOT_SPEED_MULT;

	return s;
}


float WeaponItemDef::AccuracyBase( int select, int type ) const
{
	GLASSERT( select >= 0 && type >= 0 && select < 2 && type < 3 );

	static const float MULT[3] = { ACC_SNAP_SHOT_MULTIPLIER, ACC_AUTO_SHOT_MULTIPLIER, ACC_AIMED_SHOT_MULTIPLIER };
	float acc = weapon[select].accuracy * MULT[type];
	return acc;
}


void WeaponItemDef::FireModeToType( int mode, int* select, int* type ) const 
{
	GLASSERT( mode >= 0 && mode < 3 );
	if ( mode == 0 ) {
		*select = 0;
		*type = SNAP_SHOT;
	}
	else if ( mode == 1 ) {
		if ( SupportsType( 0, AUTO_SHOT ) ) {
			*select = 0;
			*type = AUTO_SHOT;
		}
		else {
			GLASSERT( SupportsType( 0, AIMED_SHOT ) );
			*select = 0;
			*type = AIMED_SHOT;
		}
	}
	else {
		*select = 1;
		*type = AIMED_SHOT;
	}
}


bool WeaponItemDef::SupportsType( int select, int type ) const
{ 
	if ( select == 0 ) {
		if ( type == SNAP_SHOT )
			return true;
		else if ( type == AUTO_SHOT && (weapon[0].flags & WEAPON_AUTO) )
			return true;
		else if ( type == AIMED_SHOT && !(weapon[0].flags & WEAPON_AUTO) )
			return true;
	}
	else if ( select ==1 ) {
		if ( type == AIMED_SHOT && HasWeapon( select ) )
			return true;
	}
	return false;
}


bool WeaponItemDef::FireStatistics( int select, int type, 
								    float accuracy, float distance, 
									float* chanceToHit, float* anyChanceToHit,
									float* totalDamage, float* damagePerTU ) const
{
	GLASSERT( SupportsType( select, type ) );

	*chanceToHit = 0.0f;
	*damagePerTU = 0.0f;
	*totalDamage = 0.0f;
	float tu = TimeUnits( select, type );
	DamageDesc dd;

	if ( tu > 0.0f ) {
		float targetRad = distance * accuracy * AccuracyBase( select, type );
		float targetArea = PI * targetRad*targetRad;

		*chanceToHit = STANDARD_TARGET_AREA / targetArea;
		if ( *chanceToHit > 0.98f )
			*chanceToHit = 0.98f;

		*anyChanceToHit = *chanceToHit;
		int nRounds = (type==AUTO_SHOT) ? 3 : 1;

		if ( nRounds == 3 ) {
			float chanceMiss = (1.0f-*chanceToHit);
			*anyChanceToHit = 1.0f - chanceMiss*chanceMiss*chanceMiss;
		}

		DamageBase( select, &dd );
		*totalDamage = dd.Total();

		*damagePerTU = (*chanceToHit) * (*totalDamage) / tu;
		if ( type == AUTO_SHOT )
			*damagePerTU *= 3.0f;
		return true;
	}
	return false;
}


Item::Item( const ItemDef* itemDef, int rounds )
{
	this->itemDef = itemDef;
	this->rounds = rounds;
}


Item::Item( Game* game, const char* name, int rounds )
{
	const ItemDef* itemDef = game->GetItemDef( name );
	this->itemDef = itemDef;
	this->rounds = rounds;
}


void Item::UseRounds( int i ) 
{
	GLASSERT( i <= rounds );
	rounds -= i;
}


void Item::Save( UFOStream* s ) const
{
/*	s->WriteU8( 1 );	// version

	if ( itemDef ) {
		s->WriteU8( 1 );
		s->WriteStr( itemDef->name );
		s->WriteU8( rounds );
	}
	else {
		s->WriteU8( 0 );
	}
	*/
}


void Item::Load( UFOStream* s, Engine* engine, Game* game )
{
	/*
	Clear();
	int version = s->ReadU8();
	GLASSERT( version == 1 );

	int filled = s->ReadU8();
	if ( filled ) {
		const char* name = s->ReadStr();
		itemDef = game->GetItemDef( name );
		GLASSERT( itemDef );
		rounds = s->ReadU8();
	}
	*/
}




Storage::~Storage()
{
}


bool Storage::Empty() const
{
	for( int i=0; i<EL_MAX_ITEM_DEFS; ++i ) {
		if ( rounds[i] )
			return false;
	}
	return true;
}


void Storage::AddItem( const Item& item )
{
	int index = GetIndex( item.GetItemDef() );
	rounds[index] += item.Rounds();
}


void Storage::RemoveItem( const ItemDef* _itemDef, Item* _item )
{
	int index = GetIndex( _itemDef );
	int r = grinliz::Min( rounds[index], _itemDef->Rounds() );

	if ( r == 0 ) {
		_item->Clear();
	}
	else {
		Item item( _itemDef, r );
		rounds[index] -= r;
		*_item = item;
	}
}


void Storage::SetCount( const ItemDef* itemDef, int count )
{
	int index = GetIndex( itemDef );
	rounds[index] = count*itemDef->Rounds();
}


int Storage::GetCount( const ItemDef* itemDef) const
{
	int index = GetIndex( itemDef );
	int r = rounds[index];
	return (r+itemDef->Rounds()-1)/itemDef->Rounds();
}


// Return the "best" item for on-screen rendering.
/*const ItemDef* Storage::SomeItem() const
{
	int bestScore = -1;
	const ItemDef* best = 0;

	for( unsigned i=0; i<EL_MAX_ITEM_DEFS; ++i ) {
		if ( rounds[i] > 0 ) {
			int score = rounds[i];
			
			if ( itemDefs[i]->IsWeapon() )
				score *= 20;

			if ( score > bestScore ) {
				bestScore = score;
				best = itemDefs[i];
			}
		}
	}
	return best;
}
*/

/*
void Storage::Save( UFOStream* s ) const
{
	s->WriteU32Arary( itemDefs.Size(), (const U32*) rounds.Mem() );
}

void Storage::Load( UFOStream* s )
{
	s->ReadU32Arary( itemDefs.Size(), (U32*) rounds.Mem() );
}
*/
