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

#ifndef UFOATTACK_MAP_INCLUDED
#define UFOATTACK_MAP_INCLUDED

#include <stdio.h>
#include "../grinliz/gldebug.h"
#include "../grinliz/gltypes.h"
#include "../grinliz/glbitarray.h"
#include "../grinliz/glmemorypool.h"
#include "../micropather/micropather.h"
#include "vertex.h"
#include "surface.h"
#include "enginelimits.h"
#include "serialize.h"

class Model;
class ModelResource;
class SpaceTree;
class RenderQueue;
class Texture;
class ItemDef;
class Storage;
class Game;


class Map : public micropather::Graph
{
public:
	enum {
		SIZE = 64,					// maximum size. FIXME: duplicated in gamelimits.h
		MAX_ITEM_DEF = 256,
		ITEM_PER_TILE = 4,
		MAX_TRAVEL = 16,
	};

	struct MapItemDef 
	{
		enum { MAX_CX = 6, MAX_CY = 6 };

		void Init() {	name[0] = 0;
						
						cx = 1; 
						cy = 1; 
						hp = 0; 
						materialFlags = 0;

						modelResource			= 0;
						modelResourceOpen		= 0;
						modelResourceDestroyed	= 0;

						memset( pather, 0, MAX_CX*MAX_CY );
						memset( visibility, 0, MAX_CX*MAX_CY );
					}

		U16		cx, cy;
		U16		hp;					// 0 infinite, >0 can be damaged
		U32		materialFlags;

		const ModelResource* modelResource;
		const ModelResource* modelResourceOpen;
		const ModelResource* modelResourceDestroyed;

		char	name[EL_FILE_STRING_LEN];
		U8		pather[MAX_CX][MAX_CY];
		U8		visibility[MAX_CX][MAX_CY];

		// return true if the object can take damage
		bool CanDamage() const { return hp > 0; }
	};

	struct MapItem
	{
		U8		itemDefIndex;	// 0: not in use, >0 is the index
		U8		rotation;		// 0-3: rotation, 255: reference	
		U16		hp;

		union {
			Model*	model;
			const MapItem*	ref;
		};

		bool InUse() const			{ return itemDefIndex > 0; }
		bool IsReference() const	{ return (rotation == 255); }
		// returns true if destroyed
		bool DoDamage( int _hp )		
		{	
			if ( _hp >= hp ) {
				hp = 0;
				return true;
			}
			hp -= _hp;
			return false;						
		}
		bool Destroyed( const MapItemDef& itemDef ) { return (itemDef.hp > 0) && (hp == 0); }
	};

	struct MapTile
	{
		U32 pathMask;		// bits 0-3 bits are the mask.
							//      4-31 is the query id
		U32 visibilityMask;	// bits 0-3 are the mask
							//		4-31 is the query id
		Storage* storage;	// if !null, then Tile has items sitting on it, which creates a Storage object.
							// A storage object ALSO creates a crate and possible weapons at this location.
		Model* debris;		// The crate or weapon at this location. (Just use one icon.)

		MapItem item[ITEM_PER_TILE];

		int CountItems( bool countReference=true ) const;
		int FindFreeItem() const;
	};


	/* FIXME: The map lives between the game and the engine. It should probably be moved
	   to the Game layer. Until then, it takes an engine primitive (SpaceTree) and the game
	   basic class (Game) when it loads. Very strange.
	*/
	Map( SpaceTree* tree );
	virtual ~Map();

	// The size of the map in use, which is <=SIZE
	int Height() { return height; }
	int Width()  { return width; }

	void SetSize( int w, int h )					{ width = w; height = h; }

	// The background texture of the map. The map is just one big tetxure.
	void SetTexture( const Texture* texture )		{ this->texture = texture; }

	// The light map is a 64x64 texture of the lighting at each point. Without
	// a light map, full white (daytime) is used. GenerateLightMap creates the
	// resulting map by combining light with FogOfWar.
	void SetLightMap( const Surface* surface );
	void GenerateLightMap( const grinliz::BitArray<SIZE, SIZE, 1>& fogOfWar );
	
	// Rendering.
	void BindTextureUnits();
	void UnBindTextureUnits();
	void Draw();
	void DrawPath();
	void DrawOverlay();

	// Explosions impacts and such.
	void DoDamage( int damageBase, Model* m, int shellFlags );

	// Sets objects to block the path (usually other sprites) that the map doesn't know about.
	void ClearPathBlocks();
	void SetPathBlock( int x, int y );

	void SetStorage( int x, int y, Storage* storage );
	Storage* RemoveStorage( int x, int y );
	void GetMapBoundsOfModel( const Model* model, grinliz::Rectangle2I* bounds );

	MapItemDef* InitItemDef( int i );
	const char* GetItemDefName( int i );

#ifdef MAPMAKER
	Model* CreatePreview( int x, int z, int itemDefIndex, int rotation );
#endif
	// hp = -1 default
	//       0 destroyed
	//		1+ hp remaining
	bool AddToTile( int x, int z, int itemDefIndex, int rotation, int hp, bool open );

	void DeleteAt( int x, int z );
	void ResetPath();	// normally called automatically

	void Save( UFOStream* s ) const;
	void Load( UFOStream* s, Game* game );
	void Clear();

	void DumpTile( int x, int z );

	// Solves a path on the map. Returns total cost. 
	// returns MicroPather::SOLVED, NO_SOLUTION, START_END_SAME, or OUT_OF_MEMORY
	int SolvePath(	const grinliz::Vector2<S16>& start,
					const grinliz::Vector2<S16>& end,
					float* cost,
					std::vector< void* >* path );
	
	void ShowNearPath(	const grinliz::Vector2<S16>& start,
						float cost0, float cost1, float cost2 );
	void ClearNearPath();

					

	// micropather:
	virtual float LeastCostEstimate( void* stateStart, void* stateEnd );
	virtual void  AdjacentCost( void* state, std::vector< micropather::StateCost > *adjacent );
	virtual void  PrintStateInfo( void* state );

	// like pathing, but visibility
	bool CanSee( const grinliz::Vector2I& p, const grinliz::Vector2I& delta );

private:
	struct IMat
	{
		int a, b, c, d, x, z;

		void Init( int w, int h, int r );
		void Mult( const grinliz::Vector2I& in, grinliz::Vector2I* out );
	};

	enum ConnectionType {
		PATH_TYPE,
		VISIBILITY_TYPE
	};

	int InvertPathMask( U32 m ) {
		U32 m0 = (m<<2) | (m>>2);
		return m0 & 0xf;
	}

	int GetPathMask( ConnectionType c, int x, int z );
	bool Connected( ConnectionType c, int x, int y, int dir );

	void StateToVec( const void* state, grinliz::Vector2<S16>* vec ) { *vec = *((grinliz::Vector2<S16>*)&state); }
	void* VecToState( const grinliz::Vector2<S16>& vec )			 { return (void*)(*(int*)&vec); }

	void CalcModelPos(	int x, int y, int r, const MapItemDef& itemDef, 
						grinliz::Rectangle2I* mapBounds,
						grinliz::Vector2F* origin );

	MapTile* GetTile( int x, int y )				{ return &tileArr[y*SIZE+x]; }
	const MapTile* GetTile( int x, int y ) const	{ return &tileArr[y*SIZE+x]; }
	void GetItemPos( const MapItem* item, int* x, int* y, int* cx, int* cy );

	// Performs no translation of references.
	// item: input
	// output layer (to get item from returned tile)
	// x and y: map locations
	MapTile* GetTileFromItem( const MapItem* item, int* layer, int* x, int *y ) const;

	// resolve a reference:
	// outItem: item referred to
	// outTile: tile referred to
	// dx, dy: position of inItem relative to outItem. (Will be >= 0 )
	void ResolveReference( const MapItem* inTtem, MapItem** outItem, MapTile** outTile, int *dx, int* dy ) const;
	
	int width, height;
	grinliz::Rectangle3F bounds;
	const Texture* texture;
	SpaceTree* tree;

	Vertex vertex[4];
	grinliz::Vector2F texture1[4];

	Texture finalMapTex;
	Surface finalMap;
	Surface lightMap;
	U32 pathQueryID;
	U32 visibilityQueryID;

	micropather::MicroPather* microPather;

	grinliz::BitArray<SIZE, SIZE, 1>	pathBlock;	// spaces the pather can't use (units are there)	
	MapItemDef							itemDefArr[MAX_ITEM_DEF];
	MapTile								tileArr[ SIZE*SIZE ];
	std::vector<void*>					mapPath;
	std::vector< micropather::StateCost > stateCostArr;
	grinliz::BitArray< SIZE, SIZE, 3 >	walkingMap;

	void PushWalkingVertex( int x, int z, float tx, float ty ) 
	{
		GLASSERT( nWalkingVertex < 6*MAX_TRAVEL*MAX_TRAVEL );
		walkingVertex[ nWalkingVertex ].normal.Set( 0.0f, 1.0f, 0.0f );
		walkingVertex[ nWalkingVertex ].pos.Set( (float)x, 0.0f,(float)(z) );
		walkingVertex[ nWalkingVertex ].tex.Set( tx, ty );
		++nWalkingVertex;
	}

	Vertex								walkingVertex[6*MAX_TRAVEL*MAX_TRAVEL];
	int									nWalkingVertex;
};

#endif // UFOATTACK_MAP_INCLUDED
