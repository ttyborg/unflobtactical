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
class UIItem;
class ToggleButton;


/**
	The most basic unit of state. A set of vertices and indices are sent to the GPU with a given RenderAtom, which
	encapselates a state (renderState), texture (textureHandle), and coordinates. 
*/
struct RenderAtom 
{
	/// Creates a default that renders nothing.
	RenderAtom() : renderState( 0 ), textureHandle( 0 ), tx0( 0 ), ty0( 0 ), tx1( 0 ), ty1( 0 ), srcWidth( 0 ), srcHeight( 0 ), user( 0 ) {}
	
	/// Copy constructor that allows a different renderState. It's often handy to render the same texture in different states.
	template <class T > RenderAtom( const RenderAtom& rhs, const T _renderState ) {
		Init( _renderState, rhs.textureHandle, rhs.tx0, rhs.ty0, rhs.tx1, rhs.ty1, rhs.srcWidth, rhs.srcHeight );
	}

	/// Constructor with all parameters.
	template <class T0, class T1 > RenderAtom( const T0 _renderState, const T1 _textureHandle, float _tx0, float _ty0, float _tx1, float _ty1, float _srcWidth, float _srcHeight ) {
		GAMUIASSERT( sizeof( T0 ) <= sizeof( const void* ) );
		GAMUIASSERT( sizeof( T1 ) <= sizeof( const void* ) );
		Init( (const void*) _renderState, (const void*) _textureHandle, _tx0, _ty0, _tx1, _ty1, _srcWidth, _srcHeight );
	}

	/// Initialize - works just like the constructor.
	template <class T0, class T1 > void Init( const T0 _renderState, const T1 _textureHandle, float _tx0, float _ty0, float _tx1, float _ty1, float _srcWidth, float _srcHeight ) {
		GAMUIASSERT( sizeof( T0 ) <= sizeof( const void* ) );
		GAMUIASSERT( sizeof( T1 ) <= sizeof( const void* ) );
		SetCoord( _tx0, _ty0, _tx1, _ty1 );
		renderState = (const void*)_renderState;
		textureHandle = (const void*) _textureHandle;
		srcWidth = _srcWidth;
		srcHeight = _srcHeight;
	}


	/// Utility method to set the texture coordinates.
	void SetCoord( float _tx0, float _ty0, float _tx1, float _ty1 ) {
		tx0 = _tx0;
		ty0 = _ty0;
		tx1 = _tx1;
		ty1 = _ty1;
	}

	const void* renderState;		///< Application defined render state.
	const void* textureHandle;		///< Application defined texture handle, noting that 0 (null) is assumed to be non-rendering.

	// non-sorting
	float tx0, ty0, tx1, ty1;		///< Texture coordinates to use within the atom.
	float srcWidth, srcHeight;		///< The width and height of the sub-image (as defined by tx0, etc.) Used to know the "natural" size, and how to scale.

	void* user;						///< ignored by gamui
};


/**
	Coordinate system assumed by Gamui.

	Screen coordinates:
	+--- x
	|
	|
	y

	Texture coordinates:
	ty
	|
	|
	+---tx
*/
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
	Gamui(	IGamuiRenderer* renderer,
			const RenderAtom& textEnabled, 
			const RenderAtom& textDisabled,
			IGamuiText* iText );
	~Gamui();

	void Init(	IGamuiRenderer* renderer,
				const RenderAtom& textEnabled, 
				const RenderAtom& textDisabled,
				IGamuiText* iText );

	void Add( UIItem* item );
	void Remove( UIItem* item );

	void Render();

	RenderAtom* GetTextAtom() 				{ return &m_textAtomEnabled; }
	RenderAtom* GetDisabledTextAtom()		{ return &m_textAtomDisabled; }

	IGamuiText* GetTextInterface() const	{ return m_iText; }

	const UIItem* TapDown( float x, float y );
	const UIItem* TapUp( float x, float y );

	static void Layout(	UIItem** item, int nItem,
						int cx, int cy,
						float originX, float originY,
						float tableWidth, float tableHeight,
						int flags );

private:
	static bool SortItems( const UIItem* a, const UIItem* b );

	UIItem*							m_itemTapped;
	RenderAtom						m_textAtomEnabled;
	RenderAtom						m_textAtomDisabled;
	IGamuiRenderer*					m_iRenderer;
	IGamuiText*						m_iText;

	enum { INDEX_SIZE = 6000,
		   VERTEX_SIZE = 4000 };
	int16_t							m_indexBuffer[INDEX_SIZE];
	Vertex							m_vertexBuffer[VERTEX_SIZE];

	std::vector< UIItem* > m_itemArr;
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
		float advance;				// distance in pixels from this glyph to the next.
		float width;				// width of this glyph
		float height;
		float tx0, ty0, tx1, ty1;	// texture coordinates of the glyph );
	};

	virtual ~IGamuiText()	{}

	void Init( Gamui* gamui );

	virtual void GamuiGlyph( int c, gamui::IGamuiText::GlyphMetrics* metric ) = 0;
};


class UIItem
{
public:
	virtual void SetPos( float x, float y )		{ m_x = x; m_y = y; }
	void SetPos( const float* x )				{ SetPos( x[0], x[1] ); }
	void SetCenterPos( float x, float y )		{ m_x = x - Width()*0.5f; m_y = y - Height()*0.5f; }
	
	float X() const								{ return m_x; }
	float Y() const								{ return m_y; }
	virtual float Width() const = 0;
	virtual float Height() const = 0;

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

	// Toggle buttons are special...
	virtual ToggleButton* IsToggle()			{ return 0; }

	enum {
		TAP_UP,
		TAP_DOWN
	};
	virtual bool HandleTap( int action, float x, float y )	{ return false; }

	virtual const RenderAtom* GetRenderAtom() const = 0;
	virtual void Requires( int* indexNeeded, int* vertexNeeded ) = 0;
	virtual void Queue( int *nIndex, int16_t* index, int *nVertex, Gamui::Vertex* vertex ) = 0;

	void Clear()	{ m_gamui = 0; }

private:
	UIItem( const UIItem& );			// private, not implemented.
	void operator=( const UIItem& );	// private, not implemented.

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

	UIItem( int level );
	virtual ~UIItem();

	Gamui* m_gamui;
	bool m_enabled;
};


class TextLabel : public UIItem
{
public:
	TextLabel();
	TextLabel( Gamui* );
	virtual ~TextLabel();

	void Init( Gamui* );

	virtual float Width() const;
	virtual float Height() const;

	void SetText( const char* t );
	const char* GetText() const;
	void ClearText();
	virtual void SetEnabled( bool enabled )		{ m_enabled = enabled; }

	virtual const RenderAtom* GetRenderAtom() const;
	virtual void Requires( int* indexNeeded, int* vertexNeeded );
	virtual void Queue( int *nIndex, int16_t* index, int *nVertex, Gamui::Vertex* vertex );

private:
	bool IsStr() const { return buf[ALLOCATE-1] != 0; }
	void CalcSize( float* width, float* height ) const;

	enum { ALLOCATE = 16 };
	union {
		char buf[ALLOCATE];
		std::string* str;
	};
	mutable float m_width;
	mutable float m_height;
};


class Image : public UIItem
{
public:
	Image();
	Image( Gamui*, const RenderAtom& atom );
	virtual ~Image();
	void Init( Gamui*, const RenderAtom& atom );

	void SetAtom( const RenderAtom& atom );
	void SetSlice( bool enable );

	void SetSize( float width, float height )							{ m_width = width; m_height = height; }
	void SetForeground( bool foreground );

	virtual float Width() const											{ return m_width; }
	virtual float Height() const										{ return m_height; }

	virtual const RenderAtom* GetRenderAtom() const;
	virtual void Requires( int* indexNeeded, int* vertexNeeded );
	virtual void Queue( int *nIndex, int16_t* index, int *nVertex, Gamui::Vertex* vertex );

private:
	RenderAtom m_atom;
	float m_width;
	float m_height;
	bool m_slice;
};


class TiledImageBase : public UIItem
{
public:

	virtual ~TiledImageBase();
	void Init( Gamui* );

	void SetTile( int x, int y, const RenderAtom& atom );

	void SetSize( float width, float height )							{ m_width = width; m_height = height; }
	void SetForeground( bool foreground );

	virtual float Width() const											{ return m_width; }
	virtual float Height() const										{ return m_height; }
	void Clear()														{ memset( Mem(), 0, CX()*CY() ); }

	virtual const RenderAtom* GetRenderAtom() const;
	virtual void Requires( int* indexNeeded, int* vertexNeeded );
	virtual void Queue( int *nIndex, int16_t* index, int *nVertex, Gamui::Vertex* vertex );

	virtual int CX() const = 0;
	virtual int CY() const = 0;

protected:
	TiledImageBase();
	TiledImageBase( Gamui* );

	virtual int8_t* Mem() = 0;

private:
	enum {
		MAX_ATOMS = 32
	};
	RenderAtom m_atom[MAX_ATOMS];
	float m_width;
	float m_height;
};

template< int _CX, int _CY > 
class TiledImage : public TiledImageBase
{
public:
	TiledImage() : TiledImageBase() {}
	TiledImage( Gamui* g ) : TiledImageBase( g )	{}

	virtual ~TiledImage() {}

	virtual int CX() const			{ return _CX; }
	virtual int CY() const			{ return _CY; }

protected:
	virtual int8_t* Mem()			{ return mem; }

private:
	int8_t mem[_CX*_CY];
};


struct ButtonLook
{
	ButtonLook()	{};
	ButtonLook( const RenderAtom& _atomUpEnabled,
				const RenderAtom& _atomUpDisabled,
				const RenderAtom& _atomDownEnabled,
				const RenderAtom& _atomDownDisabled ) 
		: atomUpEnabled( _atomUpEnabled ),
		  atomUpDisabled( _atomUpDisabled ),
		  atomDownEnabled( _atomDownEnabled ),
		  atomDownDisabled( _atomDownDisabled )
	{}
	void Init( const RenderAtom& _atomUpEnabled, const RenderAtom& _atomUpDisabled, const RenderAtom& _atomDownEnabled, const RenderAtom& _atomDownDisabled ) {
		atomUpEnabled = _atomUpEnabled;
		atomUpDisabled = _atomUpDisabled;
		atomDownEnabled = _atomDownEnabled;
		atomDownDisabled = _atomDownDisabled;
	}

	RenderAtom atomUpEnabled;
	RenderAtom atomUpDisabled;
	RenderAtom atomDownEnabled;
	RenderAtom atomDownDisabled;
};

class Button : public UIItem
{
public:

	virtual float Width() const;
	virtual float Height() const;

	virtual void SetPos( float x, float y );
	void SetSize( float width, float height );
	void SetSizeByScale( float sx, float sy );

	virtual void SetEnabled( bool enabled );

	bool Up() const		{ return m_up; }
	bool Down() const	{ return !m_up; }
	void SetDeco( const RenderAtom& atom, const RenderAtom& atomD )			{ m_atoms[DECO] = atom; m_atoms[DECO_D] = atomD; SetState(); }

	void SetText( const char* text );
	const char* GetText() const { return m_label[0].GetText(); }
	void SetText2( const char* text );
	const char* GetText2() const { return m_label[1].GetText(); }

	enum {
		CENTER, LEFT, RIGHT
	};
	void SetTextLayout( int alignment, float dx=0.0f, float dy=0.0f )		{ m_textLayout = alignment; m_textDX = dx; m_textDY = dy; }
	void SetDecoLayout( int alignment, float dx=0.0f, float dy=0.0f )		{ m_decoLayout = alignment; m_decoDX = dx; m_decoDY = dy; }

	virtual const RenderAtom* GetRenderAtom() const;
	virtual void Requires( int* indexNeeded, int* vertexNeeded );
	virtual void Queue( int *nIndex, int16_t* index, int *nVertex, Gamui::Vertex* vertex );


protected:

	void Init( Gamui* gamui, const ButtonLook& look ) {
		RenderAtom nullAtom;
		Init( gamui, look.atomUpEnabled, look.atomUpDisabled, look.atomDownEnabled, look.atomDownDisabled, nullAtom, nullAtom );
	}

	void Init(	Gamui*,
				const RenderAtom& atomUpEnabled,
				const RenderAtom& atomUpDisabled,
				const RenderAtom& atomDownEnabled,
				const RenderAtom& atomDownDisabled,
				const RenderAtom& decoEnabled, 
				const RenderAtom& decoDisabled );

	Button();
	virtual ~Button()	{}

	bool m_up;
	void SetState();


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

	bool		m_usingText1;
	int			m_textLayout;
	int			m_decoLayout;
	float		m_textDX;
	float		m_textDY;
	float		m_decoDX;
	float		m_decoDY;	
	TextLabel	m_label[2];
};


class PushButton : public Button
{
public:
	PushButton() : Button()	{}
	PushButton( Gamui* gamui,
				const RenderAtom& atomUpEnabled,
				const RenderAtom& atomUpDisabled,
				const RenderAtom& atomDownEnabled,
				const RenderAtom& atomDownDisabled,
				const RenderAtom& decoEnabled, 
				const RenderAtom& decoDisabled) : Button()	
	{
		Init( gamui, atomUpEnabled, atomUpDisabled, atomDownEnabled, atomDownDisabled, decoEnabled, decoDisabled );
	}

	PushButton( Gamui* gamui, const ButtonLook& look ) : Button()
	{
		RenderAtom nullAtom;
		Init( gamui, look.atomUpEnabled, look.atomUpDisabled, look.atomDownEnabled, look.atomDownDisabled, nullAtom, nullAtom );
	}


	void Init( Gamui* gamui, const ButtonLook& look ) {
		RenderAtom nullAtom;
		Init( gamui, look.atomUpEnabled, look.atomUpDisabled, look.atomDownEnabled, look.atomDownDisabled, nullAtom, nullAtom );
	}

	void Init(	Gamui* gamui,
				const RenderAtom& atomUpEnabled,
				const RenderAtom& atomUpDisabled,
				const RenderAtom& atomDownEnabled,
				const RenderAtom& atomDownDisabled,
				const RenderAtom& decoEnabled, 
				const RenderAtom& decoDisabled )
	{
		Button::Init( gamui, atomUpEnabled, atomUpDisabled, atomDownEnabled, atomDownDisabled, decoEnabled, decoDisabled );
	}

	virtual ~PushButton()	{}

	virtual bool HandleTap( int action, float x, float y );
};


class ToggleButton : public Button
{
public:
	ToggleButton() : Button(), m_toggleGroup( 0 )		{}
	ToggleButton(	Gamui* gamui,
					int toggleGroup,
					const RenderAtom& atomUpEnabled,
					const RenderAtom& atomUpDisabled,
					const RenderAtom& atomDownEnabled,
					const RenderAtom& atomDownDisabled,
					const RenderAtom& decoEnabled, 
					const RenderAtom& decoDisabled) : Button(), m_toggleGroup( 0 )	
	{
		m_toggleGroup = toggleGroup;
		Button::Init( gamui, atomUpEnabled, atomUpDisabled, atomDownEnabled, atomDownDisabled, decoEnabled, decoDisabled );
	}

	ToggleButton( Gamui* gamui, int toggleGroup, const ButtonLook& look ) : Button(), m_toggleGroup( 0 )
	{
		RenderAtom nullAtom;
		Button::Init( gamui, look.atomUpEnabled, look.atomUpDisabled, look.atomDownEnabled, look.atomDownDisabled, nullAtom, nullAtom );
	}

	void Init( Gamui* gamui, int toggleGroup, const ButtonLook& look ) {
		RenderAtom nullAtom;
		m_toggleGroup = toggleGroup;
		Button::Init( gamui, look.atomUpEnabled, look.atomUpDisabled, look.atomDownEnabled, look.atomDownDisabled, nullAtom, nullAtom );
	}

	void Init(	Gamui* gamui,
				int toggleGroup,
				const RenderAtom& atomUpEnabled,
				const RenderAtom& atomUpDisabled,
				const RenderAtom& atomDownEnabled,
				const RenderAtom& atomDownDisabled,
				const RenderAtom& decoEnabled, 
				const RenderAtom& decoDisabled )
	{
		m_toggleGroup = toggleGroup;
		Button::Init( gamui, atomUpEnabled, atomUpDisabled, atomDownEnabled, atomDownDisabled, decoEnabled, decoDisabled );
	}

	virtual ~ToggleButton()		{}


	virtual bool HandleTap(	int action, float x, float y );
	virtual int ToggleGroup() const				{ return m_toggleGroup; }
//	void SetToggleGroup( int group )			{ m_toggleGroup = group; }

	virtual ToggleButton* IsToggle()			{ return this; }

	void SetUp()								{ m_up = true; SetState(); }
	void SetDown()								{ m_up = false; SetState(); }

private:
	int m_toggleGroup;
};


class DigitalBar : public UIItem
{
public:
	DigitalBar();
	DigitalBar( Gamui* gamui,
				int nTicks,
				const RenderAtom& atom0,
				const RenderAtom& atom1,
				const RenderAtom& atom2,
				float spacing ) 
		: UIItem( Gamui::LEVEL_FOREGROUND )
	{
		Init( gamui, nTicks, atom0, atom1, atom2, spacing );
	}
	virtual ~DigitalBar()		{}

	void Init(	Gamui* gamui, 
				int nTicks,
				const RenderAtom& atom0,
				const RenderAtom& atom1,
				const RenderAtom& atom2,
				float spacing );

	void SetRange( float t0, float t1 );
	void SetRange0( float t0 )				{ SetRange( t0, m_t1 ); }
	void SetRange1( float t1 )				{ SetRange( m_t0, t1 ); }
	float GetRange0() const					{ return m_t0; }
	float GetRange1() const					{ return m_t1; }

	virtual float Width() const;
	virtual float Height() const;
	virtual void SetVisible( bool visible );

	virtual const RenderAtom* GetRenderAtom() const;
	virtual void Requires( int* indexNeeded, int* vertexNeeded );
	virtual void Queue( int *nIndex, int16_t* index, int *nVertex, Gamui::Vertex* vertex );

private:
	enum { MAX_TICKS = 10 };
	int			m_nTicks;
	float		m_t0, m_t1;
	RenderAtom	m_atom[3];
	float		m_spacing;
	Image		m_image[MAX_TICKS];
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
