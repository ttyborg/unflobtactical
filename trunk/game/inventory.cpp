#include "inventory.h"
#include "game.h"
#include "../tinyxml/tinyxml.h"


Inventory::Inventory()
{
	memset( slots, 0, sizeof( Item )*NUM_SLOTS );
}


int Inventory::CalcClipRoundsTotal( const ClipItemDef* cid ) const
{
	int total = 0;
	for( int i=0; i<NUM_SLOTS; ++i ) {
		if ( slots[i].IsSomething() && slots[i].IsClip() && slots[i].IsClip() == cid ) {
			total += slots[i].Rounds();
		}
	}
	return total;
}


void Inventory::UseClipRound( const ClipItemDef* cid )
{
	for( int i=0; i<NUM_SLOTS; ++i ) {
		if ( slots[i].IsSomething() && slots[i].IsClip() && slots[i].IsClip() == cid ) {
			slots[i].UseRounds( 1 );
			if ( slots[i].Rounds() == 0 ) {
				// clip is consumed.
				slots[i].Clear();
			}
			return;
		}
	}
	GLASSERT( 0 );
}


bool Inventory::IsGeneralSlotFree()
{
	for( int i=GENERAL_SLOT; i<NUM_SLOTS; ++i ) {
		if ( slots[i].IsNothing() )
			return true;
	}
	return false;
}


bool Inventory::IsSlotFree( const ItemDef* itemDef )
{
	Item item( itemDef );
	int slot = AddItem( item );
	if ( slot >= 0 ) {
		RemoveItem( slot );
		return true;
	}
	return false;
}


int Inventory::AddItem( const Item& item )
{
	GLASSERT( slots[WEAPON_SLOT_PRIMARY].IsNothing() || slots[WEAPON_SLOT_PRIMARY].IsWeapon() );
	GLASSERT( slots[WEAPON_SLOT_SECONDARY].IsNothing() || slots[WEAPON_SLOT_SECONDARY].IsWeapon() );
	GLASSERT( slots[ARMOR_SLOT].IsNothing() || slots[ARMOR_SLOT].IsArmor() );

	if ( item.IsArmor() ) {
		if ( slots[ARMOR_SLOT].IsNothing() ) {
			slots[ARMOR_SLOT] = item;
			return ARMOR_SLOT;
		}
		return -1;
	}

	if ( item.IsWeapon() ) {
		for( int i=0; i<2; ++i ) {
			if ( slots[WEAPON_SLOT_PRIMARY+i].IsNothing() ) {
				slots[WEAPON_SLOT_PRIMARY+i] = item;
				return WEAPON_SLOT_PRIMARY+i;
			}
		}
		return -1;
	}

	for( int j=GENERAL_SLOT; j<NUM_SLOTS; ++j ) {
		if ( slots[j].IsNothing() ) {
			slots[j] = item;
			return j;
		}
	}

	return -1;
}


bool Inventory::RemoveItem( int slot ) 
{
	GLASSERT( slot >= 0 && slot < NUM_SLOTS );
	if ( slots[slot].IsSomething() ) {
		slots[slot].Clear();
		return true;
	}
	return false;
}


Item* Inventory::ArmedWeapon()
{
	if ( slots[0].IsSomething() && slots[0].IsWeapon() )
		return &slots[0];
	return 0;
}


const Item* Inventory::ArmedWeapon() const
{
	if ( slots[0].IsSomething() && slots[0].IsWeapon() )
		return &slots[0];
	return 0;
}


const Item* Inventory::SecondaryWeapon() const
{
	if ( slots[WEAPON_SLOT_SECONDARY].IsSomething() && slots[WEAPON_SLOT_SECONDARY].IsWeapon() )
		return &slots[WEAPON_SLOT_SECONDARY];
	return 0;
}


int Inventory::GetDeco( int s0 ) const
{
	int deco = DECO_NONE;
	GLASSERT( s0 >= 0 && s0 < NUM_SLOTS );
	if ( slots[s0].IsSomething() ) {
		deco = slots[s0].Deco();
	}
	return deco;
}


void Inventory::SwapWeapons()
{
	GLASSERT( slots[WEAPON_SLOT_PRIMARY].IsNothing() || slots[WEAPON_SLOT_PRIMARY].IsWeapon() );
	GLASSERT( slots[WEAPON_SLOT_SECONDARY].IsNothing() || slots[WEAPON_SLOT_SECONDARY].IsWeapon() );
	GLASSERT( slots[ARMOR_SLOT].IsNothing() || slots[ARMOR_SLOT].IsArmor() );

	grinliz::Swap( &slots[WEAPON_SLOT_PRIMARY], &slots[WEAPON_SLOT_SECONDARY] );
}


void Inventory::Save( TiXmlElement* doc ) const
{
	TiXmlElement* inventoryEle = new TiXmlElement( "Inventory" );
	doc->LinkEndChild( inventoryEle );

	for( int i=0; i<NUM_SLOTS; ++i ) {
		slots[i].Save( inventoryEle );
	}
}


void Inventory::Load( const TiXmlElement* parent, Engine* engine, Game* game )
{
	const TiXmlElement* ele = parent->FirstChildElement( "Inventory" );
	int count = 0;
	if ( ele ) {
		for( const TiXmlElement* slot = ele->FirstChildElement( "Item" );
			 slot && count<NUM_SLOTS;
			 slot = slot->NextSiblingElement( "Item" ) )
		{
			Item item;
			item.Load( slot, engine, game );
			AddItem( item );
			++count;
		}
	}
}


