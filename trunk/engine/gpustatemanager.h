#ifndef UFOATTACK_STATE_MANAGER_INCLUDED
#define UFOATTACK_STATE_MANAGER_INCLUDED

// Be sure to NOT include the gl platform header, so this can be 
// used as a platform-independent header file, and still exclude
// the gl headers.
#include <stdint.h>
#include "../grinliz/gldebug.h"
#include "../grinliz/glmatrix.h"
#include "../grinliz/glcolor.h"

#include "vertex.h"
#include "enginelimits.h"


class Texture;

namespace grinliz {
	class Matrix4;
};

class MatrixStack
{
public:
	MatrixStack();
	~MatrixStack();

	void Push();
	void Pop();
	void Set( const grinliz::Matrix4& m )			{ stack[index] = m; }
	void Multiply( const grinliz::Matrix4& m );

	const grinliz::Matrix4& Top() const				{ GLASSERT( index < MAX_DEPTH ); return stack[index]; }
	bool Empty() const								{ 
														#ifdef DEBUG
																if ( index == 0 ) {
																	grinliz::Matrix4 identity;
																	GLASSERT( identity == Top() );
																}
														#endif
														return index == 0; 
													}
	int NumMatrix() const							{	return index+1; }

private:
	enum { MAX_DEPTH = 4 };
	int index;
	grinliz::Matrix4 stack[MAX_DEPTH];
};


class GPUBuffer
{
public:
	GPUBuffer() : id( 0 )			{}
	bool IsValid() const			{ return id != 0; }
	U32 ID() const					{ return id; }
	void Clear()					{ id = 0; }

protected:	
	U32 id;
};


class GPUVertexBuffer : public GPUBuffer
{
public:
	// a null value for vertex will create an empty buffer
	static GPUVertexBuffer Create( const Vertex* vertex, int nVertex );
	void Upload( const Vertex* data, int size, int start );

	GPUVertexBuffer() : GPUBuffer() {}
	void Destroy();
private:
};


class GPUIndexBuffer : public GPUBuffer
{
public:
	static GPUIndexBuffer Create( const uint16_t* index, int nIndex );
	void Upload( const uint16_t* data, int size, int start );

	GPUIndexBuffer() : GPUBuffer() {}
	void Destroy();
};


struct GPUStream {
	// WARNING: Clear/init calls memset(0) on structure. Need to change
	// if this gets a vtable
	// Defines float sized components.
	int stride;
	int nPos;		
	int posOffset;
	int nTexture0;
	int texture0Offset;
	int nTexture1;
	int texture1Offset;
	int nNormal;
	int normalOffset;
	int nColor;
	int colorOffset;

	GPUStream() :  stride( 0 ),
				nPos( 0 ), posOffset( 0 ), 
				nTexture0( 0 ), texture0Offset( 0 ),
				nTexture1( 0 ), texture1Offset( 0 ), 
				nNormal( 0 ), normalOffset( 0 ),
				nColor( 0 ), colorOffset( 0 ) {}

	GPUStream( const Vertex* vertex );
	enum GamuiType { kGamuiType };
	GPUStream( GamuiType );
	GPUStream( const PTVertex2* vertex );
	void Clear();

	bool HasPos() const			{ return nPos > 0; }
	bool HasNormal() const		{ return nNormal > 0; }
	bool HasColor() const		{ return nColor > 0; }
	bool HasTexture0() const	{ return nTexture0 > 0; }
	bool HasTexture1() const	{ return nTexture1 > 0; }
};


class GPUShader 
{
public:
	virtual ~GPUShader();

	enum MatrixType {
		MODELVIEW_MATRIX,
		PROJECTION_MATRIX,
	};

	static void ResetState();
	static void Clear();

	// Set the top level state. The engine has top level (root transforms)
	// for the screen size, scissor, and transform:
	static void SetViewport( int w, int h );
	static void SetOrthoTransform( int width, int height, int rotation );
	static void SetPerspectiveTransform( float left, float right, 
										 float bottom, float top, 
										 float near, float far,
										 int rotation );
	// The top level MV matrix in perspective mode.
	static void SetCameraTransform( const grinliz::Matrix4& camera );
	static void SetScissor( int x, int y, int w, int h );
	

	void SetStream( const GPUStream& stream, const void* ptr, int nIndex, const uint16_t* indices ) 
	{
		GLASSERT( stream.stride > 0 );
		GLASSERT( nIndex % 3 == 0 );

		this->stream = stream;
		this->streamPtr = ptr;
		this->indexPtr = indices;
		this->nIndex = nIndex;
		this->vertexBuffer = 0;
		this->indexBuffer = 0;
	}


	void SetStream( const GPUStream& stream, const GPUVertexBuffer& vertex, int nIndex, const GPUIndexBuffer& index ) 
	{
		GLASSERT( stream.stride > 0 );
		GLASSERT( nIndex % 3 == 0 );
		GLASSERT( vertex.IsValid() );
		GLASSERT( index.IsValid() );

		this->stream = stream;
		this->streamPtr = 0;
		this->indexPtr = 0;
		this->nIndex = nIndex;
		this->vertexBuffer = vertex.ID();
		this->indexBuffer = index.ID();
	}


	void SetStream( const GPUStream& stream, const GPUVertexBuffer& vertex, int nIndex, const uint16_t* index ) 
	{
		GLASSERT( stream.stride > 0 );
		GLASSERT( nIndex % 3 == 0 );
		GLASSERT( vertex.IsValid() );
		GLASSERT( index );

		this->stream = stream;
		this->streamPtr = 0;
		this->indexPtr = index;
		this->nIndex = nIndex;
		this->vertexBuffer = vertex.ID();
		this->indexBuffer = 0;
	}


	void SetTexture0( Texture* tex ) { texture0 = tex; }
	bool HasTexture0() const { return texture0 != 0; }

	void SetTexture1( Texture* tex ) { texture1 = tex; }
	bool HasTexture1() const { return texture1 != 0; }

	void SetColor( float r, float g, float b )				{ color.r = r; color.g = g; color.b = b; color.a = 1; }
	void SetColor( float r, float g, float b, float a )		{ color.r = r; color.g = g; color.b = b; color.a = a; }
	void SetColor( const grinliz::Color4F& c )				{ color = c; }
	void SetColor( const grinliz::Color4U8& color ) {
		static const float INV = 1.0f/255.0f;
		grinliz::Color4F c = { (float)color.r*INV, (float)color.g*INV, (float)color.b*INV, (float)color.a*INV };
		SetColor( c );
	}

	void SetDiffuse( const grinliz::Vector3F direction, const grinliz::Color3F ambient, const grinliz::Color3F diffuse ) {
		this->direction = direction;
		this->ambient = ambient;
		this->diffuse = diffuse;
	}

	void PushMatrix( MatrixType type );
	void MultMatrix( MatrixType type, const grinliz::Matrix4& m );
	void PopMatrix( MatrixType type );

	// Special, because there is a texture matrix per texture unit. Painful painful system.
	// Mask is a bit mask:
	// 1: unit0, 2: unit1, 3:both units
	void PushTextureMatrix( int mask );
	void MultTextureMatrix( int mask, const grinliz::Matrix4& m );
	void PopTextureMatrix( int mask );

#if XENOENGINE_OPENGL == 2
	const grinliz::Matrix4& ConcatedMatrix( MatrixType type ) const;
	const grinliz::Matrix4& ConcatedTextureMatrix( int unit ) const;
#endif

	void Draw();

	void Debug_DrawQuad( const grinliz::Vector3F p0, const grinliz::Vector3F p1 );

	int SortOrder()	const { 
		if ( blend ) return 2;
		if ( alphaTest ) return 1;
		return 0;
	}

	static void ResetTriCount()	{ trianglesDrawn = 0; drawCalls = 0; }
	static int TrianglesDrawn() { return trianglesDrawn; }
	static int DrawCalls()		{ return drawCalls; }

	static bool SupportsVBOs();

protected:

	GPUShader() : texture0( 0 ), texture1( 0 ), 
				 streamPtr( 0 ), nIndex( 0 ), indexPtr( 0 ),
				 vertexBuffer( 0 ), indexBuffer( 0 ),
				 blend( false ), alphaTest( 0 ), lighting( false ),
				 depthWrite( true ), depthTest( true )
	{
		color.Set( 1, 1, 1, 1 );
		matrixDepth[0] = matrixDepth[1] = matrixDepth[2] = 0;
	}

	static void SetState( const GPUShader& );

private:

	void SwitchMatrixMode( MatrixType type );	
	static GPUShader		current;
	static MatrixType		matrixMode;		// Note this is static and global!
	static int vboSupport;

	// Seem to do okay on MODELVIEW and PERSPECTIVE stacks, but
	// not so much on TEXTURE. Use our own texture stack, one for each texture unit.
	static MatrixStack textureStack[2];
	static bool textureXFormInUse[2];
#if (XENOENGINE_OPENGL == 2)
	static MatrixStack mvStack;			// FIXME: check opengl preserves it's matrix stack in programmable mode, and delete this
	static MatrixStack projStack;
#endif

	static void SetTextureXForm( int unit );

protected:
	static int trianglesDrawn;
	static int drawCalls;
	static uint32_t uid;

	static const void* PTR( const void* base, int offset ) {
		return (const void*)((const U8*)base + offset);
	}

	Texture* texture0;
	Texture* texture1;

	GPUStream stream;
	const void* streamPtr;
	int nIndex;
	const uint16_t* indexPtr;
	U32 vertexBuffer;
	U32 indexBuffer;

	bool		blend;
	bool		alphaTest;
	bool		lighting;
	grinliz::Color3F	diffuse;
	grinliz::Color3F	ambient;
	grinliz::Vector3F   direction;

	bool		depthWrite;
	bool		depthTest;

	grinliz::Color4F		color;
	int			matrixDepth[3];
};


class CompositingShader : public GPUShader
{
public:
	/** Writes texture or color and neither writes nor tests z. 
		Texture/Color support
			- no texture
			- texture0, modulated by color
			- texture0 and texture1 (light map compositing)
		Blend support
	*/
	CompositingShader( bool blend=false );
	void SetBlend( bool _blend )				{ this->blend = _blend; }
};


class LightShader : public GPUShader
{
public:
	/** Texture or color. Writes & tests z. Enables lighting. */
	LightShader( const grinliz::Color4F& ambient, const grinliz::Vector4F& direction, const grinliz::Color4F& diffuse, bool blend );
	~LightShader();

protected:
	void SetLightParams() const;

	static int locked;

	grinliz::Color4F				ambient;
	grinliz::Vector4F	direction;
	grinliz::Color4F				diffuse;
};


class FlatShader : public GPUShader
{
public:
	FlatShader()	{}	// totally vanilla
};


class PointParticleShader : public GPUShader
{
private:
	static int particleSupport;
public:
	static bool IsSupported();

	PointParticleShader();
	// Does not support VBOs and ignores the index binding.
	void DrawPoints( Texture* texture, float pointSize, int start, int count );
};


class QuadParticleShader : public GPUShader
{
public:
	QuadParticleShader();
};

#endif
