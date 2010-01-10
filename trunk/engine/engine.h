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

#ifndef UFOATTACK_ENGINE_INCLUDED
#define UFOATTACK_ENGINE_INCLUDED

#include "../grinliz/gldebug.h"
#include "../grinliz/gltypes.h"
#include "../grinliz/glvector.h"
#include "../grinliz/glrectangle.h"
#include "../grinliz/glmatrix.h"
#include "../grinliz/glbitarray.h"

#include "map.h"
#include "camera.h"
#include "enginelimits.h"
#include "model.h"
#include "screenport.h"

class RenderQueue;


/*
	Standard state:
	Z-Write		enabled
	Z-Test		enabled
	Blend		disabled
*/

struct EngineData
{
	EngineData() : 
		fov( 20.f ),
		farPlane( 240.f ),
		nearPlane( 2.f ),
		cameraTilt( -50.f ),
		cameraMin( 8.0f ),
		cameraMax( 140.0f ),
		cameraHeight( 15.f )
		{}

	float fov;
	float farPlane;
	float nearPlane;
	float cameraTilt;		// degrees
	float cameraMin;
	float cameraMax;
	float cameraHeight;
};


class Engine
{
public:
	Engine( const Screenport& screenport, const EngineData& engineData );
	~Engine();

	const float AMBIENT;
	const float DIFFUSE;
	const float DIFFUSE_SHADOW;

	Camera camera;

	// Sets the opengl perspective matrices.
	void SetPerspective();
	// Render everything. Return triangles drawn. (Currently excludes UI and particles in tri-count.)
	void Draw();

	void MoveCameraHome();
	// Move the camera so that it points to x,z. If 'calc' is non-null,
	// the camera will *not* be moved, but the destination for the camera
	// is returned.
	void MoveCameraXZ( float x, float z, grinliz::Vector3F* calc=0 );

	Model* AllocModel( const ModelResource* );
	void FreeModel( Model* );
	
	void EnableMap( bool enable )	{ enableMap = enable; }
	Map* GetMap() { return map; }

	static bool UnProject(	const grinliz::Vector3F& window,
							const grinliz::Rectangle2I& screen,
							const grinliz::Matrix4& modelViewProjectionInverse,
							grinliz::Vector3F* world );

	void CalcModelViewProjectionInverse( grinliz::Matrix4* modelViewProjectionInverse );

	void WorldToScreen( const grinliz::Vector3F& p, grinliz::Vector2F* screen );
	void RayFromScreen( int x, int y, 
						const grinliz::Matrix4& modelViewProjectionInverse, 
						grinliz::Ray* ray );

	void RayFromScreenToYPlane( int x, int y, 
								const grinliz::Matrix4& modelViewProjectionInverse, 
								grinliz::Ray* ray,
								grinliz::Vector3F* intersection );

	Model* IntersectModel(	const grinliz::Ray& ray, 
							HitTestMethod testMethod,
							int required, int exclude, const Model* ignore[],
							grinliz::Vector3F* intersection );

	enum {
		NEAR,
		FAR,
		LEFT,
		RIGHT,
		BOTTOM,
		TOP
	};
	void CalcFrustumPlanes( grinliz::Plane* planes );

	float GetZoom();
	// 0.0 far, 1.0 close
	void SetZoom( float z );

	const Screenport& GetScreenport() { return screenport; }

	void SetDayNight( bool dayTime, Surface* lightMap );
	bool GetDayTime()							{ return (dayNight == DAY_TIME); }

	// FIXME - automatic??
	void RestrictCamera();

	void Save( UFOStream* s );
	void Load( UFOStream* s );

private:
	enum ShadowState {
		IN_SHADOW,
		OPEN_LIGHT
	};

	enum DayNight {
		DAY_TIME,
		NIGHT_TIME
	};

	void DrawCamera();
	void CalcCameraRotation( grinliz::Matrix4* );
	void EnableLights( bool enable, DayNight dayNight );
	void LightGroundPlane( ShadowState shadows, float shadowAmount, Color4F* outColor );

	void PushShadowMatrix();

	Screenport screenport;
 	float	frustumLeft, frustumRight, 
			frustumTop, frustumBottom, 
			frustumNear, frustumFar;
	float	initZoom;
	int		initZoomDistance;
	int		depthFunc;
	
	const EngineData& engineData;

	Map* map;
	SpaceTree* spaceTree;
	RenderQueue* renderQueue;
	bool enableMap;
	Surface diffuseLightMap;

	grinliz::Vector3F lightDirection;
	DayNight dayNight;
};

#endif // UFOATTACK_ENGINE_INCLUDED