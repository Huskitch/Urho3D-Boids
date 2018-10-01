#pragma once
#include <Urho3D/Engine/Application.h>
#include <Urho3D/Input/Input.h>
#include <Urho3D/Core/ProcessUtils.h>
#include <Urho3D/Engine/Engine.h>
#include <Urho3D/Graphics/AnimatedModel.h>
#include <Urho3D/Graphics/AnimationController.h>
#include <Urho3D/Graphics/Camera.h>
#include <Urho3D/Graphics/Light.h>
#include <Urho3D/Graphics/Material.h>
#include <Urho3D/Graphics/Octree.h>
#include <Urho3D/Graphics/Renderer.h>
#include <Urho3D/Graphics/Zone.h>
#include <Urho3D/Input/Controls.h>
#include <Urho3D/Input/Input.h>
#include <Urho3D/IO/FileSystem.h>
#include <Urho3D/Physics/CollisionShape.h>
#include <Urho3D/Physics/PhysicsWorld.h>
#include <Urho3D/Physics/RigidBody.h>
#include <Urho3D/Resource/ResourceCache.h>
#include <Urho3D/Scene/Scene.h>
#include <Urho3D/Graphics/DebugRenderer.h>

namespace Urho3D
{
	class Node;
	class Scene;
	class RigidBody;
	class CollisionShape;
	class ResourceCache;
}

using namespace Urho3D;

class Boid
{
	static float Range_FAttract;
	static float Range_FRepel;
	static float Range_FAlign;
	static float FAttract_Factor;
	static float FRepel_Factor;
	static float FAlign_Factor;
	static float FAttract_Vmax;

public:
	Boid()
	{
		pNode = nullptr;
		pRigidBody = nullptr;
		pCollisionShape = nullptr;
		pObject = nullptr;
	};

	~Boid() {};
	void Initialise(ResourceCache *pRes, Scene *pScene);
	void ComputeForce(Boid* boid);
	void Update(float ms);

	void ComputeAttraction(Boid *pBoidList);
	void ComputeSeparation(Boid *pBoidList);
	void ComputeAlignment(Boid *pBoidList);

public:
	Vector3 force;
	Node* pNode;
	RigidBody* pRigidBody;
	CollisionShape* pCollisionShape;
	StaticModel* pObject;
};

class BoidSet
{
public:
	Boid boidList[60];

	BoidSet();
	BoidSet(DebugRenderer* debugRenderer) : debug(debugRenderer) {};
	void Initialise(ResourceCache *pRes, Scene *pScene, DebugRenderer* debug);
	void Update(float ms);
	void DrawDebugInfo();
	bool Initialized = false;

	DebugRenderer* debug;
};