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

#include "screenport.h"
#include "platformgl.h"
#include "../grinliz/glrectangle.h"
#include "../grinliz/glutil.h"
#include "../grinliz/glmatrix.h"

using namespace grinliz;


Screenport::Screenport( int w, int h, int r )
{
	Resize( w, h, r );
	uiMode = false;
	clipInUI2D.Set( 0, 0, UIWidth(), UIHeight() );
	clipInUI3D.Set( 0, 0, UIWidth(), UIHeight() );
}


void Screenport::Resize( int w, int h, int r )
{
	physicalWidth = (float)w;
	physicalHeight = (float)h;
	rotation =	r;
	GLASSERT( rotation >= 0 && rotation < 4 );

	glViewport( 0, 0, w, h );

	// Sad but true: the game assets are set up for 480x320 resolution.
	// How to scale?
	//   1. Scale the smallest axis to be within 320x480. But then buttons get
	//      bigger and all layout has to be programmatic.
	//   2. Clip to a window. Seems a waste to lose that space.
	//   3. Fix UI Height at 320. Stretch backgrounds. That looks weird...but
	//		the background images can be patched up.
	// Try #3.

	if ( (rotation&1) == 0 ) {
		screenHeight = 320.0f;
		screenWidth = screenHeight * physicalWidth / physicalHeight;
//		if ( physicalWidth / physicalHeight > 480.0f/320.0f ) {
//			screenWidth = 480.0f;
//			screenHeight = screenWidth * physicalHeight / physicalWidth;
//		}
//		else {
//			screenHeight = 320.0f;
//			screenWidth = screenHeight * physicalWidth / physicalHeight;
//		};
//		GLASSERT( screenWidth <= 480.0f );
//		GLASSERT( screenHeight <= 320.0f );
	}
	else {
		GLASSERT( 0 );
		screenHeight = screenWidth * physicalHeight / physicalWidth;
		screenWidth  = 320.0f;
	}

	GLOUTPUT(( "Screenport::Resize physical=(%.1f,%.1f) view=(%.1f,%.1f) rotation=%d\n", physicalWidth, physicalHeight, screenWidth, screenHeight, r ));
}


void Screenport::SetUI( const Rectangle2I* clip )	
{
	if ( clip ) {
		clipInUI2D.Set( (float)clip->min.x, (float)clip->min.x, (float)clip->max.x, (float)clip->max.y );
	}
	else {
		clipInUI2D.Set( 0, 0, UIWidth(), UIHeight() );
	}
	GLASSERT( clipInUI2D.IsValid() );
	GLASSERT( clipInUI2D.min.x >= 0 && clipInUI2D.max.x <= UIWidth() );
	GLASSERT( clipInUI2D.min.y >= 0 && clipInUI2D.max.y <= UIHeight() );

	Rectangle2F scissor;
	UIToWindow( clipInUI2D, &scissor );
	glEnable( GL_SCISSOR_TEST );
	glScissor( (int)scissor.min.x, (int)scissor.min.y, (int)scissor.max.x, (int)scissor.max.y );
	
	view2D.SetIdentity();
	Matrix4 r, t;
	r.SetZRotation( (float)(90*Rotation() ));
	
	// the tricky bit. After rotating the ortho display, move it back on screen.
	switch (Rotation()) {
		case 0:
			break;
		case 1:
			t.SetTranslation( 0, -screenWidth, 0 );	
			break;
			
		default:
			GLASSERT( 0 );	// work out...
			break;
	}
	view2D = r*t;
	
	projection2D.SetIdentity();
	projection2D.SetOrtho( 0, screenWidth, screenHeight, 0, -1, 1 );

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();				// projection

	// Set the ortho matrix, help the driver
	//glMultMatrixf( projection.x );
	glOrthofX( 0, screenWidth, screenHeight, 0, -100, 100 );
	
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();				// model
	glMultMatrixf( view2D.x );

	uiMode = true;
	CHECK_GL_ERROR;
}


void Screenport::SetView( const Matrix4& _view )
{
	GLASSERT( uiMode == false );
	view3D = _view;

	glMatrixMode(GL_MODELVIEW);
	// In normalized coordinates.
	glLoadMatrixf( view3D.x );
	CHECK_GL_ERROR;
}


void Screenport::SetPerspective( float near, float far, float fovDegrees, const grinliz::Rectangle2I* clip )
{
	uiMode = false;

	if ( clip ) {
		clipInUI2D.Set( (float)clip->min.x, (float)clip->min.x, (float)clip->max.x, (float)clip->max.y );
	}
	else {
		clipInUI3D.Set( 0, 0, UIWidth(), UIHeight() );
	}
	GLASSERT( clipInUI3D.IsValid() );
	GLASSERT( clipInUI3D.min.x >= 0 && clipInUI3D.max.x < UIWidth() );
	GLASSERT( clipInUI3D.min.y >= 0 && clipInUI3D.max.y < UIHeight() );
	
	Rectangle2F scissor;
	UIToWindow( clipInUI2D,  &scissor );
	glEnable( GL_SCISSOR_TEST );
	glScissor( (int)scissor.min.x, (int)scissor.min.y, (int)scissor.max.x, (int)scissor.max.y );

	GLASSERT( uiMode == false );
	GLASSERT( near > 0.0f );
	GLASSERT( far > near );

	frustum.zNear = near;
	frustum.zFar = far;

	// Convert from the FOV to the half angle.
	float theta = ToRadian( fovDegrees * 0.5f );
	float tanTheta = tanf( theta );
	float longSide = tanTheta * frustum.zNear;

	// left, right, top, & bottom are on the near clipping
	// plane. (Not an obvious point to my mind.)
	
	if ( Rotation() & 1 ) {
		float ratio = (float)clipInUI3D.Height() / (float)clipInUI3D.Width();
		
		// frustum is in original screen coordinates.
		frustum.top		=  longSide;
		frustum.bottom	= -longSide;
		
		frustum.left	= -ratio * longSide;
		frustum.right	=  ratio * longSide;
	}
	else {
		// Since FOV is specified as the 1/2 width, the ratio
		// is the height/width (different than gluPerspective)
		float ratio = (float)clipInUI3D.Height() / (float)clipInUI3D.Width();

		frustum.top		= ratio * longSide;
		frustum.bottom	= -frustum.top;

		frustum.left	= -longSide;
		frustum.right	=  longSide;
	}
	
	glMatrixMode(GL_PROJECTION);
	// In normalized coordinates.
	projection3D.SetFrustum( frustum.left, frustum.right, frustum.bottom, frustum.top, frustum.zNear, frustum.zFar );
	// Give the driver hints:
	glLoadIdentity();
	glFrustumfX( frustum.left, frustum.right, frustum.bottom, frustum.top, frustum.zNear, frustum.zFar );
	//glOrthofX( frustum.left, frustum.right, frustum.bottom, frustum.top, frustum.zNear, frustum.zFar );
	
	glMatrixMode(GL_MODELVIEW);	
	CHECK_GL_ERROR;
}


void Screenport::ViewToUI( const grinliz::Vector2F& in, grinliz::Vector2F* ui ) const
{
	switch ( rotation ) {
		case 0:
			ui->x = in.x;
			ui->y = (screenHeight-1.0f-in.y);
			break;

		case 1:
			GLASSERT( 0 );
			ui->x = in.y;
			ui->y = in.x;
			break;

		default:
			GLASSERT( 0 );
			break;
	}
}


void Screenport::UIToView( const grinliz::Vector2F& in, grinliz::Vector2F* out ) const
{
	switch ( rotation ) {
		case 0:
			out->x = in.x;
			out->y = (screenHeight-in.y);
			break;
		case 1:
			GLASSERT( 0 );	// fixme
//			out->x = (LRintf(screenWidth)-1)-in.y;
//			out->y = in.x;
			break;
		default:
			GLASSERT( 0 );
			break;
	}
}


bool Screenport::ViewToWorld( const grinliz::Vector2F& v, const grinliz::Matrix4* _mvpi, grinliz::Ray* ray ) const
{
	Matrix4 mvpi;
	if ( _mvpi ) {
		mvpi = *_mvpi;
	}
	else {
		Matrix4 mvp;
		ViewProjection3D( &mvp );
		mvp.Invert( &mvpi );
	}

	Vector2F v0, v1;
	Rectangle2F clipInView;
	UIToView( clipInUI3D.min, &v0 );
	UIToView( clipInUI3D.max, &v1 ); 
	clipInView.FromPair( v0.x, v0.y, v1.x, v1.y );

	Vector4F in = { (v.x - (float)(clipInView.min.x))*2.0f / (float)clipInView.Width() - 1.0f,
					(v.y - (float)(clipInView.min.y))*2.0f / (float)clipInView.Height() - 1.0f,
					0.f, //v.z*2.0f-1.f,
					1.0f };

	Vector4F out0, out1;
	MultMatrix4( mvpi, in, &out0 );
	in.z = 1.0f;
	MultMatrix4( mvpi, in, &out1 );

	if ( out0.w == 0.0f ) {
		return false;
	}
	ray->origin.x = out0.x / out0.w;
	ray->origin.y = out0.y / out0.w;
	ray->origin.z = out0.z / out0.w;

	ray->direction.x = out1.x / out1.w - ray->origin.x;
	ray->direction.y = out1.y / out1.w - ray->origin.y;
	ray->direction.z = out1.z / out1.w - ray->origin.z;

	return true;
}


/*
void Screenport::ScreenToWorld( int x, int y, Ray* world ) const
{
	//GLOUTPUT(( "ScreenToWorld(upper) %d,%d\n", x, y ));

	Vector2I v;
	ScreenToView( x, y, &v );

	Matrix4 mvpi;
	ViewProjectionInverse3D( &mvpi );

	Vector3F win0 = { (float)v.x, (float)v.y, 0 };
	Vector3F win1 = { (float)v.x, (float)v.y, 1 };
	Vector3F p0, p1;

	ViewToWorld( win0, mvpi, &p0 );
	ViewToWorld( win1, mvpi, &p1 );

	world->origin = p0;
	world->direction = p1 - p0;
}
*/

void Screenport::WorldToView( const grinliz::Vector3F& world, grinliz::Vector2F* v ) const
{
	Matrix4 mvp;
	ViewProjection3D( &mvp );

	Vector4F p, r;
	p.Set( world, 1 );

	r = mvp * p;

	// Normalize to view.
	// r in range [-1,1] which maps to screen[0,Width()-1]
	Vector2F v0, v1;
	Rectangle2F clipInView;
	UIToView( clipInUI3D.min, &v0 );
	UIToView( clipInUI3D.max, &v1 ); 
	clipInView.FromPair( v0.x, v0.y, v1.x, v1.y );
	
	v->x = Interpolate( -1.0f, (float)clipInView.min.x,
						1.0f,  (float)clipInView.max.x,
						r.x/r.w );
	v->y = Interpolate( -1.0f, (float)clipInView.min.y,
						1.0f,  (float)clipInView.max.y,
						r.y/r.w );
}

/*
void Screenport::WorldToGUI( const grinliz::Vector3F& p, grinliz::Vector2F* ui ) const
{
	Vector2F v;
	WorldToScreen( p, &v );
	ViewToGUI( LRintf(v.x), LRintf(v.y), &ui->x, &ui->y );
}
*/


void Screenport::ViewToWindow( const Vector2F& view, Vector2F* window ) const
{
	window->x = view.x * physicalWidth  / screenWidth;
	window->y = view.y * physicalHeight / screenHeight;
}


void Screenport::WindowToView( const Vector2F& window, Vector2F* view ) const
{
	view->x = window.x * screenWidth / physicalWidth;
	view->y = window.y * screenHeight / physicalHeight;
}


void Screenport::UIToWindow( const grinliz::Rectangle2F& ui, grinliz::Rectangle2F* clip ) const
{	
	Vector2F v;
	Vector2F w;
	
	UIToView( ui.min, &v );
	ViewToWindow( v, &w );
	clip->min = clip->max = w;

	UIToView( ui.max, &v );
	ViewToWindow( v, &w );
	clip->DoUnion( w );
}


/*
void Screenport::SetViewport( const grinliz::Rectangle2I* uiClip )
{
	if ( uiClip ) {
		Rectangle2I scissor;
		UIToScissor( uiClip->min.x, uiClip->min.y, uiClip->Width(), uiClip->Height(), &scissor );

		glEnable( GL_SCISSOR_TEST );
		glScissor( scissor.min.x, scissor.min.y, scissor.Width(), scissor.Height() );
		glViewport( scissor.min.x, scissor.min.y, scissor.Width(), scissor.Height() );
	}
	else {
		glDisable( GL_SCISSOR_TEST );
	}
	CHECK_GL_ERROR;
}
*/
