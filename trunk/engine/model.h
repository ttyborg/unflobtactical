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

#ifndef UFOATTACK_MODEL_INCLUDED
#define UFOATTACK_MODEL_INCLUDED

#include "../grinliz/gldebug.h"
#include "../grinliz/gltypes.h"
#include "../grinliz/glgeometry.h"
#include "vertex.h"
#include "enginelimits.h"
#include "serialize.h"

class Texture;
class SpaceTree;
class RenderQueue;

struct ModelAtom 
{
	const Texture* texture;
	U32 vertexID;
	U32 indexID;
	U32 nVertex;
	U32 nIndex;
	mutable int trisRendered;

	void Bind( bool bindTextureToVertex ) const;
	void Draw() const;
};


class ModelResource
{
public:
	ModelHeader header;				// loaded

	grinliz::Sphere			boundSphere;	// computed
	float					boundRadius2D;	// computed, bounding 2D radius centered at 0,0
	grinliz::Rectangle3F	hitBounds;		// for picking - a bounds approximation

	int IsOriginUpperLeft()	{ return header.flags & ModelHeader::UPPER_LEFT; }

	ModelAtom atom[EL_MAX_MODEL_GROUPS];
};


class ModelLoader
{
public:
	ModelLoader( const Texture* texture, int nTextures ) 	{
		this->texture = texture;
		this->nTextures = nTextures;
	}
	~ModelLoader()		{}

	void Load( FILE* fp, ModelResource* res );

private:
	const Texture* texture;
	int nTextures;

	U16		index[EL_MAX_INDEX_IN_MODEL];
	Vertex	vertex[EL_MAX_VERTEX_IN_MODEL];
};


class Model
{
public:
	Model()		{	Init( 0, 0 ); }
	~Model()	{}

	void Init( ModelResource* resource, SpaceTree* tree );

	// Queued rendering
	enum {
		MODEL_TEXTURE,	// use the texture of the models
		NO_TEXTURE,		// no texture at all - shadow pass Z
		TEXTURE_SET		// used for background texture tricks - shadow pass ONE_PASS
	};
	void Queue( RenderQueue* queue, int textureState=MODEL_TEXTURE );

	// Used by the queued rendering system:
	void PushMatrix( bool bindTextureToVertex ) const;
	void PopMatrix( bool bindTextureToVertex ) const;

	int IsDraggable()				{ return isDraggable; }
	void SetDraggable( bool drag )	{ isDraggable = drag; }
	int IsHiddenFromTree()			{ return hiddenFromTree; }
	void HideFromTree( bool hide )  { hiddenFromTree = hide; }
	int IsOwnedByMap()				{ return isOwnedByMap; }
	void OwnedByMap( bool map )		{ isOwnedByMap = map; }
	
	const grinliz::Vector3F& Pos()						{ return pos; }
	void SetPos( const grinliz::Vector3F& pos );
	void SetPos( float x, float y, float z )	{ grinliz::Vector3F vec = { x, y, z }; SetPos( vec ); }
	float X() { return pos.x; }
	float Y() { return pos.y; }
	float Z() { return pos.z; }

	void SetYRotation( float rot )		{
		while( rot < 0 )		{ rot += 360.0f; }
		while( rot >= 360 )		{ rot -= 360.0f; }
		this->rot = rot;		// won't change tree location, don't need to call Update()
	}
	const float GetYRotation()			{ return rot; }

	int IsBillboard()		{ return resource->header.flags & ModelHeader::BILLBOARD; }
	int IsOriginUpperLeft()	{ return resource->header.flags & ModelHeader::UPPER_LEFT; }
	void SetSkin( int armor, int skin, int hair );

	void CalcBoundSphere( grinliz::Sphere* sphere );
	void CalcBoundCircle( grinliz::Circle* circle );
	void CalcHitAABB( grinliz::Rectangle3F* aabb );

	void SetTexture( const Texture* t )			{ setTexture = t; }

	ModelResource* GetResource()				{ return resource; }
	bool Sentinel()								{ return resource==0 && tree==0; }

	Model* next;			// used by the SpaceTree query
	Model* next0;			// used by the Engine sub-sorting

private:
	SpaceTree* tree;
	ModelResource* resource;
	grinliz::Vector3F pos;
	float rot;
	float textureOffsetX;
	const Texture* setTexture;	// overrides the default texture

	// FIXME condense flags
	bool isDraggable;
	bool hiddenFromTree;
	bool isOwnedByMap;
};


#endif // UFOATTACK_MODEL_INCLUDED