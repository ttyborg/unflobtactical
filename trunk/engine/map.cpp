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

#include "platformgl.h"
#include "map.h"
#include "model.h"
#include "loosequadtree.h"
#include "renderqueue.h"
#include "surface.h"
#include "text.h"
#include "../game/material.h"		// bad call to less general directory. FIXME. Move map to game?
#include "../game/unit.h"			// bad call to less general directory. FIXME. Move map to game?
#include "../game/game.h"			// bad call to less general directory. FIXME. Move map to game?

using namespace grinliz;
using namespace micropather;


Map::Map( SpaceTree* tree )
{
	memset( itemDefArr, 0, sizeof(MapItemDef)*MAX_ITEM_DEF );
	memset( tileArr, 0, sizeof(MapTile)*SIZE*SIZE );
	microPather = new MicroPather(	this,									// graph interface
									&patherMem, PATHER_MEM32*sizeof(U32),	// memory
									8 );									// max adjacent states

	this->tree = tree;
	width = height = SIZE;
	texture = 0;

	vertex[0].pos.Set( 0.0f,		0.0f, 0.0f );
	vertex[1].pos.Set( 0.0f,		0.0f, (float)SIZE );
	vertex[2].pos.Set( (float)SIZE, 0.0f, (float)SIZE );
	vertex[3].pos.Set( (float)SIZE, 0.0f, 0.0f );

	vertex[0].tex.Set( 0.0f, 1.0f );
	vertex[1].tex.Set( 0.0f, 0.0f );
	vertex[2].tex.Set( 1.0f, 0.0f );
	vertex[3].tex.Set( 1.0f, 1.0f );

	vertex[0].normal.Set( 0.0f, 1.0f, 0.0f );
	vertex[1].normal.Set( 0.0f, 1.0f, 0.0f );
	vertex[2].normal.Set( 0.0f, 1.0f, 0.0f );
	vertex[3].normal.Set( 0.0f, 1.0f, 0.0f );

	texture1[0].Set( 0.0f, 1.0f );
	texture1[1].Set( 0.0f, 0.0f );
	texture1[2].Set( 1.0f, 0.0f );
	texture1[3].Set( 1.0f, 1.0f );

	queryID = 1;

	finalMap.Set( SIZE, SIZE, 2 );
	lightMap.Set( SIZE, SIZE, 2 );

	memset( finalMap.Pixels(), 255, SIZE*SIZE*2 );
	memset( lightMap.Pixels(), 255, SIZE*SIZE*2 );

	U32 id = finalMap.CreateTexture( false );
	finalMapTex.Set( "lightmap", id, false );
}


Map::~Map()
{
	Clear();
	delete microPather;
}


void Map::Clear()
{
	for( int j=0; j<SIZE; ++j ) {
		for( int i=0; i<SIZE; ++i ) {
			MapTile* tile = &tileArr[j*SIZE+i];
			int count = tile->CountItems();
			for( int k=0; k<count; ++k ) {
				GLASSERT( tile->CountItems() > 0 );
				DeleteAt( i, j );
			}
			GLASSERT( tile->CountItems() == 0 );

			if ( tile->storage ) {
				delete tile->storage;
			}
			if ( tile->debris ) {
				tree->FreeModel( tile->debris );
			}
		}
	}
	memset( tileArr, 0, sizeof(MapTile)*SIZE*SIZE );
}


void Map::Draw()
{
	U8* v = (U8*)vertex + Vertex::POS_OFFSET;
	U8* n = (U8*)vertex + Vertex::NORMAL_OFFSET;
	U8* t = (U8*)vertex + Vertex::TEXTURE_OFFSET;

	/* Texture 0 */
	glBindTexture( GL_TEXTURE_2D, texture->glID );

	glVertexPointer(   3, GL_FLOAT, sizeof(Vertex), v);			// last param is offset, not ptr
	glNormalPointer(      GL_FLOAT, sizeof(Vertex), n);		
	glTexCoordPointer( 2, GL_FLOAT, sizeof(Vertex), t); 
	
	/* Texture 1 */
	glActiveTexture( GL_TEXTURE1 );
	glClientActiveTexture( GL_TEXTURE1 );
	glEnable( GL_TEXTURE_2D );
	glBindTexture( GL_TEXTURE_2D, finalMapTex.glID );
	glEnableClientState( GL_TEXTURE_COORD_ARRAY );
	glTexCoordPointer( 2, GL_FLOAT, 0, texture1 );

	glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
	CHECK_GL_ERROR

	glDrawArrays( GL_TRIANGLE_FAN, 0, 4 );

	glDisable( GL_TEXTURE_2D );
	glDisableClientState( GL_TEXTURE_COORD_ARRAY );
	glClientActiveTexture( GL_TEXTURE0 );
	glActiveTexture( GL_TEXTURE0 );

	CHECK_GL_ERROR
}

void Map::BindTextureUnits()
{
	/* Texture 0 */
	glBindTexture( GL_TEXTURE_2D, texture->glID );

	/* Texture 1 */
	glActiveTexture( GL_TEXTURE1 );
	glClientActiveTexture( GL_TEXTURE1 );
	glEnable( GL_TEXTURE_2D );
	glBindTexture( GL_TEXTURE_2D, finalMapTex.glID );
	glEnableClientState( GL_TEXTURE_COORD_ARRAY );
	glTexCoordPointer( 2, GL_FLOAT, 0, texture1 );

	glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
	
	CHECK_GL_ERROR
}


void Map::UnBindTextureUnits()
{
	glActiveTexture( GL_TEXTURE1 );
	glClientActiveTexture( GL_TEXTURE1 );
	glDisable( GL_TEXTURE_2D );

	glDisableClientState( GL_TEXTURE_COORD_ARRAY );
	glClientActiveTexture( GL_TEXTURE0 );
	glActiveTexture( GL_TEXTURE0 );
}

	
void Map::SetLightMap( const Surface* surface )
{
	if ( surface ) {
		GLASSERT( surface->BytesPerPixel() == 2 );
		GLASSERT( surface->Width() == 64 );
		GLASSERT( surface->Height() == 64 );
		memcpy( lightMap.Pixels(), surface->Pixels(), SIZE*SIZE*2 );
	}
	else {
		memset( lightMap.Pixels(), 255, SIZE*SIZE*2 );
	}
}


void Map::GenerateLightMap( const grinliz::BitArray<SIZE, SIZE, 1>& fogOfWar )
{
	BitArrayRowIterator<SIZE, SIZE, 1> it( fogOfWar ); 
	U16* dst = (U16*) finalMap.Pixels();

	const U16* src =   (U16*)lightMap.Pixels();
	GLASSERT( lightMap.BytesPerPixel() == 2 );

	for( int j=0; j<SIZE; ++j ) {
		for( int i=0; i<SIZE; ++i ) {
			*dst = fogOfWar.IsSet( i, SIZE-1-j ) ? *src : 0;
			++src;
			++dst;
		}
	}
	finalMap.UpdateTexture( false, finalMapTex.glID );
}



Map::MapItemDef* Map::InitItemDef( int i )
{
	GLASSERT( i > 0 );				// 0 is reserved
	GLASSERT( i < MAX_ITEM_DEF );
	return &itemDefArr[i];
}


const char* Map::GetItemDefName( int i )
{
	const char* result = "";
	if ( i > 0 && i < MAX_ITEM_DEF ) {
		result = itemDefArr[i].name;
	}
	return result;
}


int Map::MapTile::FindFreeItem() const
{
	for( int i=0; i<ITEM_PER_TILE; ++i ) {
		if ( !item[i].InUse() )
			return i;
	}
	return -1;
}


int Map::MapTile::CountItems( bool countReferences ) const
{
	int count = 0;
	for( int i=0; i<ITEM_PER_TILE; ++i ) {
		if ( item[i].InUse() && ( countReferences || !item[i].IsReference() ) )
			++count;
	}
	return count;
}


void Map::ResolveReference( const MapItem* inItem, MapItem** outItem, MapTile** outTile, int *dx, int* dy ) const
{

	if ( !inItem->IsReference() ) {
		*outItem = const_cast<MapItem*>( inItem );
		*outTile = GetTileFromItem( inItem, 0, 0, 0 );
		if ( dx && dy ) {
			*dx = 0;
			*dy = 0;
		}
	}
	else {
		int layer, parentX, parentY;
		*outTile = GetTileFromItem( inItem->ref, &layer, &parentX, &parentY );
		*outItem = &((*outTile)->item[ layer ]);

		int childX, childY;
		GetTileFromItem( inItem, 0, &childX, &childY );
		if ( dx && dy ) {
			*dx = childX - parentX;
			*dy = childY - parentY;
		}
	}
	GLASSERT( !dx || *dx >= 0 && *dx < MapItemDef::MAX_CX );
	GLASSERT( !dy || *dy >= 0 && *dy < MapItemDef::MAX_CY );
}


void Map::IMat::Init( int w, int h, int r )
{
	x = 0;
	z = 0;

	// Matrix to map from map coordinates
	// back to object coordinates.
	switch ( r ) {
		case 0:
			a = 1;	b = 0;	x = 0;
			c = 0;	d = 1;	z = 0;
			break;
		
		case 1:
			a = 0;	b = -1;	x = w-1;
			c = 1;	d = 0;	z = 0;
			break;

		case 2:
			a = -1;	b = 0;	x = w-1;
			c = 0;	d = -1;	z = h-1;
			break;

		case 3:
			a = 0;	b = 1;	x = 0;
			c = -1;	d = 0;	z = h-1;
			break;

		default:
			break;
	};
}


void Map::IMat::Mult( const grinliz::Vector2I& in, grinliz::Vector2I* out  )
{
	out->x = a*in.x + b*in.y + x;
	out->y = c*in.x + d*in.y + z;
}


Map::MapTile* Map::GetTileFromItem( const MapItem* item, int* _layer, int* x, int *y ) const
{
	int index = ((const U8*)item - (const U8*)tileArr) / sizeof( MapTile );
	GLASSERT( index >= 0 && index < SIZE*SIZE );
	int layer = item - tileArr[index].item;
	GLASSERT( layer >= 0 && layer < ITEM_PER_TILE );

	if ( _layer ) {
		*_layer = layer;
	}
	if ( x && y ) {
		*y = index / SIZE;
		*x = index - (*y) * SIZE;
		GLASSERT( *x >=0 && *x < SIZE );
		GLASSERT( *y >=0 && *y < SIZE );
	}
	return const_cast< MapTile* >(tileArr + index);
}


void Map::DoDamage( int baseDamage, Model* m, int shellFlags )
{
	if ( m->IsFlagSet( Model::MODEL_OWNED_BY_MAP ) ) 
	{
		GLASSERT( m->stats );

		MapItem* item = (MapItem*) m->stats;
		const MapItemDef& itemDef = itemDefArr[item->itemDefIndex];
		int hp = MaterialDef::CalcDamage( baseDamage, shellFlags, itemDef.materialFlags );

		if ( itemDef.CanDamage() && item->DoDamage(hp) ) {
			// Destroy the current model. Replace it with "destroyed"
			// model if there is one.
			Vector3F pos = item->model->Pos();
			float rot = item->model->GetYRotation();
			tree->FreeModel( item->model );
			item->model = 0;

			if ( itemDef.modelResourceDestroyed ) {
				item->model = tree->AllocModel( itemDef.modelResourceDestroyed );
				item->model->SetFlag( Model::MODEL_OWNED_BY_MAP );
				item->model->SetPos( pos );
				item->model->SetYRotation( rot );
				item->model->stats = item;			
			}
			microPather->Reset();
		}
	}
}


void Map::GetItemPos( const MapItem* inItem, int* x, int* y, int* cx, int* cy )
{
	MapItem* item = 0;
	MapTile* tile = 0;
	ResolveReference( inItem, &item, &tile, 0, 0 );	

	const MapItemDef& itemDef = itemDefArr[item->itemDefIndex];
	int rot = item->rotation;
	GLASSERT( rot >= 0 && rot < 4 );

	Vector2I prime = { 1, 1 };

	if ( itemDef.cx > 1 || itemDef.cy > 1 ) {
		Vector2I size = { itemDef.cx, itemDef.cy };
		
		IMat iMat;
		iMat.Init( size.x, size.y, rot );
		Vector2I origin = { size.x-1, size.y-1 };
		iMat.Mult( origin, &prime );
	}

	GetTileFromItem( item, 0, x, y );
	*cx = prime.x;
	*cy = prime.y;
}


void Map::GetMapBoundsOfModel( const Model* m, Rectangle2I* bounds )
{
	GLASSERT( m && m->IsFlagSet( Model::MODEL_OWNED_BY_MAP ) );
	
	const MapItem* item = (const MapItem*) m->stats;
	int x, y, cx, cy;
	GetItemPos( item, &x, &y, &cx, &cy );
	bounds->Set( x, y, x+cx-1, y+cy-1 );
}


#ifdef MAPMAKER
Model* Map::CreatePreview( int x, int y, int defIndex, int rotation )
{
	Model* model = 0;
	if ( itemDefArr[defIndex].modelResource ) {
		Rectangle2I mapBounds;
		Vector2F modelPos;
		CalcModelPos( x, y, rotation, itemDefArr[defIndex], &mapBounds, &modelPos );

		model = tree->AllocModel( itemDefArr[defIndex].modelResource );
		model->SetPos( modelPos.x, 0.0f, modelPos.y );
		model->SetYRotation( 90.0f * rotation );
	}
	return model;
}
#endif


bool Map::AddToTile( int x, int y, int defIndex, int rotation, int hp, bool open )
{
	GLASSERT( x >= 0 && x < width );
	GLASSERT( y >= 0 && y < height );
	GLASSERT( defIndex > 0 && defIndex < MAX_ITEM_DEF );
	GLASSERT( rotation >=0 && rotation < 4 );
	
	if ( !itemDefArr[defIndex].modelResource ) {
		GLOUTPUT(( "No model resource.\n" ));
		GLASSERT( 0 );
		return false;
	}

	MapTile* tile = tileArr + ((y*SIZE)+x);

	Rectangle2I mapBounds;
	Vector2F modelPos;
	CalcModelPos( x, y, rotation, itemDefArr[defIndex], &mapBounds, &modelPos );
	
	// Check bounds on map.
	if ( mapBounds.min.x < 0 || mapBounds.min.y < 0 || mapBounds.max.x >= SIZE || mapBounds.max.y >= SIZE ) {
		GLOUTPUT(( "Out of bounds.\n" ));
		return false;
	}

	// Check for duplicate. Only check for exact dupe - same origin & rotation.
	// This isn't required, but prevents map creation mistakes.
	for( int nLayer=0; nLayer<ITEM_PER_TILE; ++nLayer )
	{
		MapItem* item = &tile->item[nLayer];

		if ( item->InUse() && !item->IsReference() ) {
			GLASSERT( item->model );
			if ( item->itemDefIndex == defIndex && item->rotation == rotation ) {
				GLOUTPUT(( "Duplicate layer.\n" ));
				return false;
			}
		}
	}

	// Check for open slots. This is required, since both the Item and the references
	// to it consume a map slot.
	for( int j=mapBounds.min.y; j<=mapBounds.max.y; ++j ) {
		for( int i=mapBounds.min.x; i<=mapBounds.max.x; ++i ) {
			if ( tileArr[j*SIZE+i].CountItems() == ITEM_PER_TILE ) {
				GLOUTPUT(( "Map full at %d,%d.\n", i, j ));
				return false;
			}
		}
	}

	// Finally add!!
	ResetPath();
	MapItem* mainItem = 0;
	for( int i=mapBounds.min.x; i<=mapBounds.max.x; ++i ) 
	{
		for( int j=mapBounds.min.y; j<=mapBounds.max.y; ++j ) 
		{
			MapTile* refTile	= &tileArr[j*SIZE+i];
			GLASSERT( refTile->FindFreeItem() >= 0 );
			MapItem* item		= &refTile->item[ refTile->FindFreeItem() ];

			if ( i==mapBounds.min.x && j==mapBounds.min.y ) 
			{
				// Main tile (or only tile for 1x1)
				mainItem = item;
				item->itemDefIndex = defIndex;
				item->rotation = rotation;

				const ModelResource* res = 0;

				if ( itemDefArr[defIndex].CanDamage() && hp == 0 ) {
					res = itemDefArr[defIndex].modelResourceDestroyed;
				}
				else {
					if ( open )
						res = itemDefArr[defIndex].modelResourceOpen;
					else
						res = itemDefArr[defIndex].modelResource;
				}
				if ( res ) {
					item->model = tree->AllocModel( res );
					item->model->SetFlag( Model::MODEL_OWNED_BY_MAP );
					item->model->SetPos( modelPos.x, 0.0f, modelPos.y );
					item->model->SetYRotation( 90.0f * rotation );
					item->model->stats = item;
				}
				//MapItemState* state = (MapItemState*) statePool.Alloc();
				//state->Init( itemDefArr[defIndex], hp );
				//state->item = item;
				//item->model->stats = state;
				if ( hp >= 0 )
					item->hp = hp;
				else
					item->hp = itemDefArr[defIndex].hp;

			}
			else {
				item->itemDefIndex	= defIndex;
				item->rotation		= 255;
				GLASSERT( !item->model );
				GLASSERT( !item->ref );

				GLASSERT( mainItem );
				item->ref = mainItem;
			}
		}
	}
	return true;
}


void Map::DeleteAt( int x, int y )
{
	GLASSERT( x >= 0 && x < width );
	GLASSERT( y >= 0 && y < height );

	MapTile* tile = tileArr + ((y*SIZE)+x);
	int layer = tile->CountItems() - 1;
	if ( layer < 0 ) {
		return;
	}

	// If reference, change everything to the main object.
	if ( tile->item[layer].IsReference() ) {
		tile = GetTileFromItem( tile->item[layer].ref, &layer, &x, &y );
	}

	GLASSERT( layer >= 0 && layer < ITEM_PER_TILE );
	if ( layer < 0 || layer >= ITEM_PER_TILE ) {
		return;
	}
	ResetPath();
	const MapItem* mainItem = &tile->item[layer];

	Rectangle2I bounds;
	Vector2F p;
	CalcModelPos( x, y, mainItem->rotation, itemDefArr[mainItem->itemDefIndex], &bounds, &p );
	
	// Free the main item.
	if ( tile->item[layer].model ) {
		//statePool.Free( tile->item[layer].model->stats );
		tree->FreeModel( tile->item[layer].model );
	}

	tile->item[layer].itemDefIndex = 0;
	tile->item[layer].rotation = 0;
	tile->item[layer].model = 0;

	// free the references
	for( int j=bounds.min.y; j<=bounds.max.y; ++j ) {
		for( int i=bounds.min.x; i<=bounds.max.x; ++i ) {
			if ( j == bounds.min.y && i == bounds.min.x )
				continue;

			int k=0;
			for( k=0; k<ITEM_PER_TILE; ++k ) {
				if (    tileArr[j*SIZE+i].item[k].IsReference()
					 && tileArr[j*SIZE+i].item[k].ref == mainItem ) 
				{
					// Found it!
					tileArr[j*SIZE+i].item[k].itemDefIndex = 0;
					tileArr[j*SIZE+i].item[k].rotation = 0;
					tileArr[j*SIZE+i].item[k].ref = 0;
					break;
				}
			}
			GLASSERT( k != ITEM_PER_TILE );
		}
	}
}


void Map::CalcModelPos(	int x, int y, int r, const MapItemDef& itemDef, 
						grinliz::Rectangle2I* mapBounds,
						grinliz::Vector2F* origin )
{
	int cx = itemDef.cx;
	int cy = itemDef.cy;
	float halfCX = (float)cx * 0.5f;
	float halfCY = (float)cy * 0.5f;

	float xf = (float)x;
	float yf = (float)y;
	float cxf = (float)cx;
	float cyf = (float)cy;

	const ModelResource* resource = itemDef.modelResource;
	// rotates around the upper left, irrespective
	// of the actual model origin.
	//
	//		 0	 0	
	//		0ab	0bd
	//		 cd	 ac
	//

	if ( r & 1 ) {
		mapBounds->Set( x, y, x+cy-1, y+cx-1 );
	}
	else {
		mapBounds->Set( x, y, x+cx-1, y+cy-1 );
	}

	if ( resource->IsOriginUpperLeft() ) {
		switch( r ) {
			case 0:		origin->Set( xf,			yf );			break;
			case 1:		origin->Set( xf,			yf+cxf );		break;
			case 2:		origin->Set( xf+cxf,		yf+cyf );		break;
			case 3:		origin->Set( xf+cyf,		yf );			break;
			default:	GLASSERT( 0 );								break;
		}
	}
	else {
		switch( r ) {
			case 0:		
			case 2:
				origin->Set( xf + halfCX, yf + halfCY );	
				break;

			case 1:		
			case 3:
				origin->Set( xf + halfCY, yf + halfCX );	
				break;

			default:
				GLASSERT( 0 );
				break;
		}
	}
}


void Map::Save( UFOStream* s ) const
{
	s->WriteU8( 2 );	// version
	s->WriteU32( UFOStream::MAGIC0 );

	for( int j=0; j<SIZE; ++j ) {
		for( int i=0; i<SIZE; ++i ) {

			// Write every tile. The minimal information is a count of layers.
			// The layer data is only written if needed.
			const MapTile* tile = GetTile( i, j );
			
			s->WriteBool( tile->storage > 0 );	// v2
			if ( tile->storage ) {
				tile->storage->Save( s );
			}

			U8 count = tile->CountItems( false );

			// temporary debugging:
			if ( i>=40 || j>= 40 ) GLASSERT( count == 0 );

			s->WriteU8( count );
			
			for( int k=0; k<ITEM_PER_TILE; ++k ) {
				if ( tile->item[k].InUse() && !tile->item[k].IsReference() ) {
					const MapItem* item = &tile->item[k];

					s->WriteU8( item->itemDefIndex );
					s->WriteU8( item->rotation );

					s->WriteU16( item->hp );	// v2
					s->WriteBool( item->model && item->model->GetResource() == itemDefArr[item->itemDefIndex].modelResourceOpen );	// v2 - isOpen
				}
			}
		}
	}
	s->WriteU32( UFOStream::MAGIC1 );
	//GLOUTPUT(( "Map saved.\n" ));
}


void Map::Load( UFOStream* s, Game* game )
{
	Clear();

	int version = s->ReadU8();

	if ( version >= 2 ) {
		U32 magic = s->ReadU32();
		GLASSERT( magic == UFOStream::MAGIC0 );
	}

	for( int j=0; j<SIZE; ++j ) {
		for( int i=0; i<SIZE; ++i ) {

			MapTile* tile = GetTile( i, j );
			GLASSERT( tile->storage == 0 && tile->debris == 0 );

			if ( version >= 2 ) {
				bool hasStorage = s->ReadBool();
				if ( hasStorage ) {
					Storage* storage = new Storage( game->GetItemDefArray() );
					storage->Load( s );
					SetStorage( i, j, storage );
				}
			}

			int count = s->ReadU8();
			// temporary debugging:
			if ( i>=40 || j>= 40 ) GLASSERT( count == 0 );

			for( int k=0; k<count; ++k ) 
			{
				U8 defIndex = s->ReadU8();
				U8 rotation = s->ReadU8();

				if ( defIndex > 0 ) {
					int hp = -1;
					bool open = false;
					if ( version >= 2 ) {
						hp = s->ReadU16();
						open = s->ReadBool();
					}

					bool result = AddToTile( i, j, defIndex, rotation, hp, open ); 
					GLASSERT( result );
					(void)result;
				}
			}
		}
	}
	if ( version >= 2 ) {
		U32 magic = s->ReadU32();
		GLASSERT( magic == UFOStream::MAGIC1 );
	}
	//GLOUTPUT(( "Map loaded.\n" ));
}


void Map::SetStorage( int x, int y, Storage* storage )
{
	RemoveStorage( x, y );	// delete existing
	MapTile* tile = GetTile( x, y );
	tile->storage = storage;
	
	// Find an item:
	const ItemDef* itemDef = storage->SomeItem();
	if ( !itemDef ) {
		// empty.
		delete storage;
		tile->storage = 0;
	}
	else { 
		const ModelResource* res = itemDef->resource;
		if ( !res ) {
			res = ModelResourceManager::Instance()->GetModelResource( "crate" );
		}
		tile->debris = tree->AllocModel( res );
		const Rectangle3F& aabb = tile->debris->AABB();
		tile->debris->SetPos( (float)(x)+0.5f, -aabb.min.y, (float)(y)+0.5f );
		tile->debris->SetFlag( Model::MODEL_OWNED_BY_MAP );
	}
}


Storage* Map::RemoveStorage( int x, int y )
{
	MapTile* tile = GetTile( x, y );
	if ( tile->debris ) {
		tree->FreeModel( tile->debris );
		tile->debris = 0;
	}
	Storage* result = tile->storage;
	tile->storage = 0;
	return result;
}


void Map::DumpTile( int x, int y )
{
	if ( InRange( x, 0, SIZE-1 ) && InRange( y, 0, SIZE-1 )) 
	{
		const MapTile& tile = tileArr[y*SIZE+x];

		for( int i=0; i<ITEM_PER_TILE; ++i ) {
			int index = tile.item[i].itemDefIndex;
			const MapItemDef& itemDef = itemDefArr[index];

			if ( tile.item[i].IsReference() ) {
				int rx, ry, layer;
				MapTile* t = GetTileFromItem( tile.item[i].ref, &layer, &rx, &ry );

				UFOText::Draw( 0, 100-12*i, "->(%d,%d) %s r=%d", 
							   rx-x, ry-y, itemDef.name, t->item[layer].rotation );
			}
			else {
				if ( itemDef.name[0] ) {
					int r = tile.item[i].rotation;
					UFOText::Draw( 0, 100-12*i, "%s r=%d", itemDef.name, r*90 );
				}
				else {
					UFOText::Draw( 0, 100-12*i, "%s", "--" );
				}
			}
		}
	}
}


void Map::DrawPath()
{
	glEnable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

	glDisable( GL_TEXTURE_2D );
	glDisableClientState( GL_NORMAL_ARRAY );
	glDisableClientState( GL_TEXTURE_COORD_ARRAY );

	for( int j=0; j<SIZE; ++j ) {
		for( int i=0; i<SIZE; ++i ) {
			float x = (float)i;
			float y = (float)j;

			//const Tile& tile = tileArr[j*SIZE+i];
			int path = GetPathMask( i, j );

			if ( path == 0 ) 
				continue;

			Vector3F red[12];
			Vector3F green[12];
			int	nRed = 0, nGreen = 0;	
			const float EPS = 0.2f;

			Vector3F edge[5] = {
				{ x, EPS, y+1.0f },
				{ x+1.0f, EPS, y+1.0f },
				{ x+1.0f, EPS, y },
				{ x, EPS, y },
				{ x, EPS, y+1.0f }
			};
			Vector3F center = { x+0.5f, 0, y+0.5f };

			for( int k=0; k<4; ++k ) {
				int mask = (1<<k);

				if ( path & mask ) {
					red[nRed+0] = edge[k];
					red[nRed+1] = edge[k+1];
					red[nRed+2] = center;
					nRed += 3;
				}
				else {
					green[nGreen+0] = edge[k];
					green[nGreen+1] = edge[k+1];
					green[nGreen+2] = center;
					nGreen += 3;
				}
			}
			GLASSERT( nRed <= 12 );
			GLASSERT( nGreen <= 12 );

			glColor4f( 1.f, 0.f, 0.f, 0.5f );
			glVertexPointer( 3, GL_FLOAT, 0, red );
 			glDrawArrays( GL_TRIANGLES, 0, nRed );	
			CHECK_GL_ERROR;

			glColor4f( 0.f, 1.f, 0.f, 0.5f );
			glVertexPointer( 3, GL_FLOAT, 0, green );
 			glDrawArrays( GL_TRIANGLES, 0, nGreen );	
			CHECK_GL_ERROR;
		}
	}

	glEnableClientState( GL_NORMAL_ARRAY );
	glEnableClientState( GL_TEXTURE_COORD_ARRAY );
	glEnable( GL_TEXTURE_2D );
	glDisable( GL_BLEND );
}


void Map::ResetPath()
{
	microPather->Reset();
}



void Map::ClearPathBlocks()
{
	pathBlock.ClearAll();
}


void Map::SetPathBlock( int x, int y )
{
	pathBlock.Set( x, y );
}


int Map::GetPathMask( int x, int y )
{
	// fast return: if the pathBlock is set, we're done.
	if ( pathBlock.IsSet( x, y ) ) {
		return 0xf;
	}

	// Handles both rotation of the object and rotation of the masks
	// in the object. And tile resulotion. Tweaky function to get right.
	//
	int index = y*SIZE+x;
	MapTile* originTile = &tileArr[index];

	U32 id = originTile->pathMask >> 4;
	GLASSERT( id < 0xffffff );	// how did it get this big??

	if ( id != queryID ) {
		U32 path = 0;
		for( int i=0; i<ITEM_PER_TILE; ++i ) {
			const MapItem* inItem = &originTile->item[i];
			if ( inItem->itemDefIndex == 0 ) {
				continue;
			}

			MapItem* item = 0;
			MapTile* tile = 0;
			Vector2I origin;
			ResolveReference( inItem, &item, &tile, &origin.x, &origin.y );
			U32 p = 0;	// pather at this location.
			const MapItemDef& itemDef = itemDefArr[item->itemDefIndex];

			if ( !item->Destroyed( itemDef ) ) {
				int rot = item->rotation;

				GLASSERT( rot >= 0 && rot < 4 );

				Vector2I size = { itemDef.cx, itemDef.cy };
				Vector2I prime = { 0, 0 };

				// Account for object rotation (if needed)
				IMat iMat;
				if ( size.x > 1 || size.y > 1 ) {
					iMat.Init( size.x, size.y, rot );
					iMat.Mult( origin, &prime );
				}
				GLASSERT( prime.x >= 0 && prime.x < itemDef.cx );
				GLASSERT( prime.y >= 0 && prime.y < itemDef.cy );

				// Account for tile rotation. (Actually a bit rotation too, which is handy.)
				p = ( itemDef.pather[prime.y][prime.x] << rot );
				p = p | (p>>4);
			}
			path |= (U8)(p&0xf);
		}

		// Write the information - and the query ID - back to the originalTile and cache it.
		GLASSERT( ( path & 0xfffffff0 ) == 0 );
		originTile->pathMask = path | (queryID<<4);
	}
	return originTile->pathMask & 0xf;
}


float Map::LeastCostEstimate( void* stateStart, void* stateEnd )
{
	Vector2<S16> start, end;
	StateToVec( stateStart, &start );
	StateToVec( stateEnd, &end );

	float dx = (float)(start.x-end.x);
	float dy = (float)(start.y-end.y);

	return sqrtf( dx*dx + dy*dy );
}


bool Map::Connected( int x, int y, int dir )
{
	const Vector2I next[4] = {
		{ 0, 1 },
		{ 1, 0 },
		{ 0, -1 },
		{ -1, 0 } 
	};

	GLASSERT( dir >= 0 && dir < 4 );
	GLASSERT( x >=0 && x < SIZE );
	GLASSERT( y >=0 && y < SIZE );

	int bit = 1<<dir;
	Vector2I pos = { x, y };
	Vector2I nextPos = pos + next[dir];

	// To be connected, it must be clear from a->b and b->a
	if ( InRange( pos.x, 0, SIZE-1 )     && InRange( pos.y, 0, SIZE-1 ) &&
		 InRange( nextPos.x, 0, SIZE-1 ) && InRange( nextPos.y, 0, SIZE-1 ) ) 
	{
		int mask0 = GetPathMask( pos.x, pos.y );
		int maskN = GetPathMask( nextPos.x, nextPos.y );
		int inv = InvertPathMask( bit );

		if ( (( mask0 & bit ) == 0 ) && (( maskN & inv ) == 0 ) ) {
			return true;
		}
	}
	return false;
}


void Map::AdjacentCost( void* state, micropather::StateCost *adjacent, int* nAdjacent )
{
	Vector2<S16> pos;
	StateToVec( state, &pos );

	const Vector2<S16> next[8] = {
		{ 0, 1 },
		{ 1, 0 },
		{ 0, -1 },
		{ -1, 0 },

		{ 1, 1 },
		{ 1, -1 },
		{ -1, -1 },
		{ -1, 1 },
	};
	const float SQRT2 = 1.414f;
	const float cost[8] = {	1.0f, 1.0f, 1.0f, 1.0f, SQRT2, SQRT2, SQRT2, SQRT2 };

	*nAdjacent = 0;
	// N S E W
	for( int i=0; i<4; i++ ) {
		if ( Connected( pos.x, pos.y, i ) ) {
			Vector2<S16> nextPos = pos + next[i];
			adjacent[*nAdjacent].cost = cost[i];
			adjacent[*nAdjacent].state = VecToState( nextPos );
			(*nAdjacent)++;
		}
	}
	// Diagonals. Need to check if all the NSEW connections work. If
	// so, then the diagonal connection works too.
	for( int i=0; i<4; i++ ) {
		int j=(i+1)&3;
		Vector2<S16> nextPos = pos + next[i+4];
		int iInv = (i+2)&3;
		int jInv = (j+2)&3;
		 
		if (    Connected( pos.x, pos.y, i ) 
			 && Connected( pos.x, pos.y, j )
			 && Connected( nextPos.x, nextPos.y, iInv )
			 && Connected( nextPos.x, nextPos.y, jInv ) ) 
		{
			adjacent[*nAdjacent].cost = cost[i+4];
			adjacent[*nAdjacent].state = VecToState( nextPos );
			(*nAdjacent)++;
		}
	}
}


void Map::PrintStateInfo( void* state )
{
	Vector2<S16> pos;
	StateToVec( state, &pos );
	GLOUTPUT(( "[%d,%d]", pos.x, pos.y ));
}


int Map::SolvePath( const Vector2<S16>& start, const Vector2<S16>& end, float *cost, Vector2<S16>* path, int* nPath, int maxPath )
{
	GLASSERT( sizeof( int ) == sizeof( void* ));	// fix this for 64 bit
	GLASSERT( sizeof(Vector2<S16>) == sizeof( void* ) );
	++queryID;

	unsigned pathNodesAllocated = microPather->PathNodesAllocated();
	int result = microPather->Solve(	VecToState( start ),
										VecToState( end ),
										maxPath, 
										(void**)path, nPath, cost );

	if ( pathNodesAllocated > 0 && result == MicroPather::OUT_OF_MEMORY ) {
		// MicroPather as of April09 doesn't free memory aggressively enough. This works
		// around the problem by reset (frees all memory) and solving again.
		// This makes the worse case - a long path consumes all the memory - by
		// doing it again.
		microPather->Reset();
		result = microPather->Solve(	VecToState( start ),
										VecToState( end ),
										maxPath, 
										(void**)path, nPath, cost );
	}
	/*
	switch( result ) {
		case MicroPather::SOLVED:			GLOUTPUT(( "Solved nPath=%d\n", *nPath ));			break;
		case MicroPather::NO_SOLUTION:		GLOUTPUT(( "NoSolution nPath=%d\n", *nPath ));		break;
		case MicroPather::START_END_SAME:	GLOUTPUT(( "StartEndSame nPath=%d\n", *nPath ));	break;
		case MicroPather::OUT_OF_MEMORY:	GLOUTPUT(( "OutOfMemory nPath=%d\n", *nPath ));		break;
		default:	GLASSERT( 0 );	break;
	}
	*/
	return result;
}