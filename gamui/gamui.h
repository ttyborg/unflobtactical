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

#ifndef GAMUI_INCLUDED
#define GAMUI_INCLUDED

#include <stdint.h>

#include <string>
#include <vector>


#if defined( _DEBUG ) || defined( DEBUG )
#	if defined( _MSC_VER )
#		define GAMUIASSERT( x )		if ( !(x)) { _asm { int 3 } }
#	else
#		include <assert.h>
#		define GAMUIASSERT assert
#	endif
#else
#	define GAMUIASSERT( x ) {}
#endif


namespace gamui
{

class Gamui;
class IGamuiRenderer;
class IGamuiText;
class GamItem;

/*

	Screen coordinates:
	+--- x
	|
	|
	y

	Texture (OpenGL standard, although feels like it should be the other way...)
	ty
	|
	|
	+---tx


*/

struct RenderAtom 
{
	RenderAtom() : renderState( 0 ), textureHandle( 0 ), tx0( 0 ), ty0( 0 ), tx1( 0 ), ty1( 0 ), user( 0 ) {}
	// sorting fields
	const void* renderState;
	const void* textureHandle;

	// non-sorting
	float tx0, ty0, tx1, ty1;
	void SetCoord( float _tx0, float _ty0, float _tx1, float _ty1 ) { tx0 = _tx0; ty0 = _ty0; tx1 = _tx1; ty1 = _ty1; }

	// ignored
	void* user;
};


class Gamui
{
public:
	enum {
		LEVEL_BACKGROUND = 0,
		LEVEL_FOREGROUND = 1,
		LEVEL_DECO		 = 2,
		LEVEL_TEXT		 = 3
	};

	struct Vertex {
		float x;
		float y;
		float tx;
		float ty;

		void Set( float _x, float _y, float _tx, float _ty ) { x = _x; y = _y; tx = _tx; ty = _ty; }
	};

	Gamui();
	~Gamui();

	void Add( GamItem* item );
	void Remove( GamItem* item );

	void Render( IGamuiRenderer* renderer );

	void InitText(	const RenderAtom& enabled, 
					const RenderAtom& disabled,
					int pixelHeight,
					IGamuiText* iText );

	RenderAtom* GetTextAtom() 		{ return &m_textAtomEnabled; }
	RenderAtom* GetDisabledTextAtom() { return &m_textAtomDisabled; }


	IGamuiText* GetTextInterface() const	{ return m_iText; }
	int GetTextHeight() const				{ return m_textHeight; }

	const GamItem* TapDown( int x, int y );
	const GamItem* TapUp( int x, int y );

private:
	static bool SortItems( const GamItem* a, const GamItem* b );

	GamItem*						m_itemTapped;
	RenderAtom						m_textAtomEnabled;
	RenderAtom						m_textAtomDisabled;
	IGamuiText*						m_iText;
	int								m_textHeight;
	enum { INDEX_SIZE = 6000,
		   VERTEX_SIZE = 4000 };
	int16_t							m_indexBuffer[INDEX_SIZE];
	Vertex							m_vertexBuffer[VERTEX_SIZE];

	std::vector< GamItem* > m_itemArr;
};


class IGamuiRenderer
{
public:
	virtual ~IGamuiRenderer()	{}
	virtual void BeginRender() = 0;
	virtual void EndRender() = 0;

	virtual void BeginRenderState( const void* renderState ) = 0;
	virtual void BeginTexture( const void* textureHandle ) = 0;
	virtual void Render( const void* renderState, const void* textureHandle, int nIndex, const int16_t* index, int nVertex, const Gamui::Vertex* vertex ) = 0;
};


class IGamuiText
{
public:
	struct GlyphMetrics {
		int advance;				// distance in pixels from this glyph to the next.
		int width;					// width of this glyph
		float tx0, ty0, tx1, ty1;	// texture coordinates of the glyph );
	};

	virtual ~IGamuiText()	{}
	virtual void GamuiGlyph( int c, GlyphMetrics* metric ) = 0;
};


/*
	Subclasses:
		x TextLabel
		x Bar
		x Image
		x Button
		x	- Toggle
		x	- Radio

		effects:
		- rotate x
		- rotate y
		- rotate z

		Layout
*/
class GamItem
{
public:
	virtual void SetPos( float x, float y )		{ m_x = x; m_y = y; }
	void SetPos( const float* x )				{ SetPos( x[0], x[1] ); }
	
	float X() const								{ return m_x; }
	float Y() const								{ return m_y; }
	virtual int Width() const = 0;
	virtual int Height() const = 0;

	int Level() const							{ return m_level; }

	virtual void SetEnabled( bool enabled )		{}		// does nothing for many items.
	bool Enabled() const						{ return m_enabled; }
	virtual void SetVisible( bool visible )		{ m_visible = visible; }
	bool Visible() const						{ return m_visible; }
	
	void SetLevel( int level )					{ m_level = level; }

	void SetRotationX( float degrees )			{ m_rotationX = degrees; }
	void SetRotationY( float degrees )			{ m_rotationY = degrees; }
	void SetRotationZ( float degrees )			{ m_rotationZ = degrees; }
	float RotationX() const						{ return m_rotationX; }
	float RotationY() const						{ return m_rotationY; }
	float RotationZ() const						{ return m_rotationZ; }

	virtual void Attach( Gamui* );

	enum {
		TAP_UP,
		TAP_DOWN
	};
	virtual bool HandleTap( int action, int x, int y )	{ return false; }

	virtual const RenderAtom* GetRenderAtom() const = 0;
	virtual void Requires( int* indexNeeded, int* vertexNeeded ) = 0;
	virtual void Queue( int *nIndex, int16_t* index, int *nVertex, Gamui::Vertex* vertex ) = 0;

private:
	GamItem( const GamItem& );			// private, not implemented.
	void operator=( const GamItem& );	// private, not implemented.

	float m_x;
	float m_y;
	int m_level;
	bool m_visible;
	float m_rotationX;
	float m_rotationY;
	float m_rotationZ;

protected:
	template <class T> T Min( T a, T b ) const		{ return a<b ? a : b; }
	template <class T> T Max( T a, T b ) const		{ return a>b ? a : b; }
	float Mean( float a, float b ) const			{ return (a+b)*0.5f; }
	static void PushQuad( int *nIndex, int16_t* index, int base, int a, int b, int c, int d, int e, int f );

	void ApplyRotation( int nVertex, Gamui::Vertex* vertex );

	GamItem( int level );
	virtual ~GamItem()					{}

	Gamui* m_gamui;
	bool m_enabled;
};


class TextLabel : public GamItem
{
public:
	TextLabel();
	virtual ~TextLabel();

	virtual int Width() const;
	virtual int Height() const;

	void SetText( const char* t );
	const char* GetText();
	void ClearText();
	virtual void SetEnabled( bool enabled )		{ m_enabled = enabled; }

	virtual const RenderAtom* GetRenderAtom() const;
	virtual void Requires( int* indexNeeded, int* vertexNeeded );
	virtual void Queue( int *nIndex, int16_t* index, int *nVertex, Gamui::Vertex* vertex );

private:
	void CalcSize( int* width, int* height ) const;

	enum { ALLOCATE = 16 };
	union {
		char buf[ALLOCATE];
		std::string* str;
	};
	mutable int m_width;
	mutable int m_height;
};


class Image : public GamItem
{
public:
	Image();
	virtual ~Image();

	void Init( const RenderAtom& atom, int srcWidth, int srcHeight );
	void SetAtom( const RenderAtom& atom, int srcWidth, int srcHeight );
	void SetSlice( bool enable );

	void SetSize( int width, int height )							{ m_width = width; m_height = height; }
	void SetForeground( bool foreground );

	virtual int Width() const										{ return m_width; }
	virtual int Height() const										{ return m_height; }
	int SrcWidth() const											{ return m_srcWidth; }
	int SrcHeight() const											{ return m_srcHeight; }

	virtual const RenderAtom* GetRenderAtom() const;
	virtual void Requires( int* indexNeeded, int* vertexNeeded );
	virtual void Queue( int *nIndex, int16_t* index, int *nVertex, Gamui::Vertex* vertex );

private:
	RenderAtom m_atom;
	int m_srcWidth;
	int m_srcHeight;
	int m_width;
	int m_height;

	bool m_slice;
};


class Button : public GamItem
{
public:

	void Init(	const RenderAtom& atomUpEnabled,
				const RenderAtom& atomUpDisabled,
				const RenderAtom& atomDownEnabled,
				const RenderAtom& atomDownDisabled,
				const RenderAtom& decoEnabled, 
				const RenderAtom& decoDisabled,
				int srcWidth,
				int srcHeight );

	virtual int Width() const										{ return m_face.Width(); }
	virtual int Height() const										{ return m_face.Height(); }

	virtual void Attach( Gamui* gamui );
	virtual void SetPos( float x, float y );
	void SetSize( int width, int height );
	void SetText( const char* text );
	virtual void SetEnabled( bool enabled );

	virtual const RenderAtom* GetRenderAtom() const;
	virtual void Requires( int* indexNeeded, int* vertexNeeded );
	virtual void Queue( int *nIndex, int16_t* index, int *nVertex, Gamui::Vertex* vertex );

protected:

	Button();
	virtual ~Button()	{}

	bool m_up;
	void SetState( bool enabled, bool up );


private:

	void PositionChildren();

	enum {
		UP,
		UP_D,
		DOWN,
		DOWN_D,
		DECO,
		DECO_D,
		COUNT
	};
	RenderAtom m_atoms[COUNT];
	
	Image		m_face;
	Image		m_deco;
	TextLabel	m_label;
};


class PushButton : public Button
{
public:
	PushButton() : Button()	{}
	virtual ~PushButton()	{}

	virtual bool HandleTap( int action, int x, int y );
};


class ToggleButton : public Button
{
public:
	ToggleButton() : Button()	{}
	virtual ~ToggleButton()		{}

	virtual bool HandleTap( int action, int x, int y );
};


class DigitalBar : public GamItem
{
public:
	DigitalBar();
	virtual ~DigitalBar()		{}

	void Init(	int nTicks,
				const RenderAtom& atom0,
				const RenderAtom& atom1,
				const RenderAtom& atom2,
				int tickSrcWidth,
				int tickSrcHeight,
				int spacing );

	void SetRange( float t0, float t1 );

	virtual int Width() const				{ return m_image[0].SrcWidth()*m_nTicks + m_spacing*(m_nTicks-1); }
	virtual int Height() const				{ return m_image[0].SrcHeight(); }

	virtual void Attach( Gamui* gamui );

	virtual const RenderAtom* GetRenderAtom() const;
	virtual void Requires( int* indexNeeded, int* vertexNeeded );
	virtual void Queue( int *nIndex, int16_t* index, int *nVertex, Gamui::Vertex* vertex );

private:
	enum { MAX_TICKS = 10 };
	int			m_nTicks;
	float		m_t0, m_t1;
	RenderAtom	m_atom[3];
	int			m_spacing;
	Image		m_image[MAX_TICKS];
};


class Layout
{
public:
	Layout()	{}
	~Layout()	{}

	void DoLayout( GamItem** item, 
				   int cx, int cy,
				   int tableWidth, int tableHeight,
				   float originX, float originY,
				   int flags );

};

class Matrix
{
  public:
	Matrix()		{	x[0] = x[5] = x[10] = x[15] = 1.0f;
						x[1] = x[2] = x[3] = x[4] = x[6] = x[7] = x[8] = x[9] = x[11] = x[12] = x[13] = x[14] = 0.0f; 
					}

	void SetTranslation( float _x, float _y, float _z )		{	x[12] = _x;	x[13] = _y;	x[14] = _z;	}

	void SetXRotation( float thetaDegree );
	void SetYRotation( float thetaDegree );
	void SetZRotation( float thetaDegree );

	float x[16];

	inline friend Matrix operator*( const Matrix& a, const Matrix& b )
	{	
		Matrix result;
		MultMatrix( a, b, &result );
		return result;
	}


	inline friend void MultMatrix( const Matrix& a, const Matrix& b, Matrix* c )
	{
	// This does not support the target being one of the sources.
		GAMUIASSERT( c != &a && c != &b && &a != &b );

		// The counters are rows and columns of 'c'
		for( int i=0; i<4; ++i ) 
		{
			for( int j=0; j<4; ++j ) 
			{
				// for c:
				//	j increments the row
				//	i increments the column
				*( c->x + i +4*j )	=   a.x[i+0]  * b.x[j*4+0] 
									  + a.x[i+4]  * b.x[j*4+1] 
									  + a.x[i+8]  * b.x[j*4+2] 
									  + a.x[i+12] * b.x[j*4+3];
			}
		}
	}

	inline friend void MultMatrix( const Matrix& m, const float* v, int components, float* out )
	{
		float w = 1.0f;
		for( int i=0; i<components; ++i )
		{
			*( out + i )	=		m.x[i+0]  * v[0] 
								  + m.x[i+4]  * v[1]
								  + m.x[i+8]  * v[2]
								  + m.x[i+12] * w;
		}
	}
};


};


#endif // GAMUI_INCLUDED