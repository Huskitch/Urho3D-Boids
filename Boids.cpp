#include "Boids.h"

float Boid::Range_FAttract = 30.0f;
float Boid::Range_FRepel = 20.0f;
float Boid::Range_FAlign = 5.0f;
float Boid::FAttract_Vmax = 5.0f;
float Boid::FAttract_Factor = 4.0f;
float Boid::FRepel_Factor = 2.0f;
float Boid::FAlign_Factor = 2.0f;

void Boid::Initialise(ResourceCache *pRes, Scene *pScene)
{
	pNode = pScene->CreateChild("Boid");
	pNode->SetPosition(Vector3(0.0f, 0.0f, 0.0f));
	pNode->SetRotation(Quaternion(0.0f, 0.0f, 0.0f));
	pNode->SetScale(Vector3(0.005f, 0.005f, 0.005f));

	pObject = pNode->CreateComponent<StaticModel>();
	pObject->SetModel(pRes->GetResource<Model>("Models/TropicalFish12.mdl"));
	pObject->SetMaterial(pRes->GetResource<Material>("Materials/Fish.xml"));
	pObject->SetCastShadows(true);
	pObject->SetDrawDistance(100);

	pRigidBody = pNode->CreateComponent<RigidBody>();
	pRigidBody->SetCollisionLayer(3);
	pRigidBody->SetUseGravity(false);
	pRigidBody->SetMass(1.0f);
	pRigidBody->SetPosition(Vector3(Random(180.0f) - 90.0f, Random(180.0f) - 0.0f, Random(180.0f) - 90.0f));
	pRigidBody->SetLinearVelocity(Vector3(Random(-20.0f) - 20.0f, 0, Random(-20.0f) - 20.0f));

	pCollisionShape = pNode->CreateComponent<CollisionShape>();
	pCollisionShape->SetTriangleMesh(pObject->GetModel(), 0);
}

void Boid::ComputeForce(Boid *pBoidList)
{
	ComputeAttraction(pBoidList);
	ComputeAlignment(pBoidList);
	ComputeSeparation(pBoidList);
}

void Boid::ComputeSeparation(Boid *pBoidList)
{
	int n = 0;

	for (int i = 0; i < 60; i++)
	{
		if (this == &pBoidList[i])
			continue;

		Vector3 position = pRigidBody->GetPosition();
		Vector3 currBoidPos = pBoidList[i].pRigidBody->GetPosition();

		Vector3 separation = position - currBoidPos;
		float deltaSeparation = separation.Length();

		if (deltaSeparation < Range_FRepel)
		{
			if ((deltaSeparation > 0) && deltaSeparation < 100)
			{
				Vector3 diff = Vector3(0, 0, 0);
				diff = position - currBoidPos;
				diff.Normalize();
				force += diff * FRepel_Factor;
				n++;
			}
		}
	}

	if (n > 0)
	{

	}
}

void Boid::ComputeAlignment(Boid *pBoidList)
{
	int n = 0;
	Vector3 sum = Vector3(0, 0, 0);

	for (int i = 0; i < 60; i++)
	{
		if (this == &pBoidList[i])
			continue;

		Vector3 position = pRigidBody->GetPosition();
		Vector3 currBoidPos = pBoidList[i].pRigidBody->GetPosition();

		Vector3 separation = position - currBoidPos;
		float deltaSeparation = separation.Length();

		if (deltaSeparation < Range_FAttract)
		{
			sum += pBoidList[i].pRigidBody->GetLinearVelocity();
			n++;
		}
	}

	if (n > 0)
	{
		sum /= n;
		sum.Normalize();
		sum * FAlign_Factor;

		force += sum - pRigidBody->GetLinearVelocity();
	}
}

void Boid::ComputeAttraction(Boid *pBoidList)
{
	Vector3 CenterOfMass;
	int n = 0;
	force = Vector3(0, 0, 0);

	for (int i = 0; i < 60; i++)
	{
		if (this == &pBoidList[i])
			continue;

		Vector3 position = pRigidBody->GetPosition();
		Vector3 currBoidPos = pBoidList[i].pRigidBody->GetPosition();

		Vector3 separation = position - currBoidPos;
		float deltaSeparation = separation.Length();

		if (deltaSeparation < Range_FAttract)
		{
			CenterOfMass += pBoidList[i].pRigidBody->GetPosition();
			n++;
		}
	}

	if (n > 0)
	{
		CenterOfMass /= n;
		Vector3 dir = (CenterOfMass - pRigidBody->GetPosition()).Normalized();
		Vector3 vDesired = dir * FAttract_Vmax;
		force += (vDesired - pRigidBody->GetLinearVelocity()) * FAttract_Factor;
	}
}

void Boid::Update(float tm)
{
	pRigidBody->ApplyForce(force);
	Vector3 vel = pRigidBody->GetLinearVelocity();
	float d = vel.Length();
	if (d < 10.0f)
	{
		d = 10.0f;
		pRigidBody->SetLinearVelocity(vel.Normalized()*d);
	}
	else if (d > 50.0f)
	{
		d = 50.0f;
		pRigidBody->SetLinearVelocity(vel.Normalized()*d);
	}
	Vector3 vn = vel.Normalized();
	Vector3 cp = -vn.CrossProduct(Vector3(0.0f, 1.0f, 0.0f));
	float dp = cp.DotProduct(vn);
	pRigidBody->SetRotation(Quaternion(Acos(dp), cp));
	Vector3 p = pRigidBody->GetPosition();
	if (p.y_ < 10.0f)
	{
		p.y_ = 10.0f;
		pRigidBody->SetPosition(p);
	}
	else if (p.y_ > 50.0f)
	{
		p.y_ = 50.0f;
		pRigidBody->SetPosition(p);
	}
}

void BoidSet::DrawDebugInfo()
{
	for (int i = 0; i < 60; i++)
	{
		debug->AddLine(boidList[i].pRigidBody->GetPosition(), boidList[i].pRigidBody->GetRotation().EulerAngles().FORWARD, Color::BLUE, true);
	}
}

void BoidSet::Initialise(ResourceCache *pRes, Scene *pScene, DebugRenderer* debug)
{
	this->debug = debug;

	Initialized = true;

	for (int i = 0; i < 60; i++)
	{
		boidList[i].Initialise(pRes, pScene);
	}
}

void BoidSet::Update(float ms)
{
	//// For each given boid in the scene
	for (Boid boid : boidList)
	{
	//	// If the current boid is in the player view frustum
	//	if (boid.pObject->IsInView())
	//	{
	//		// Freeze the boid in position
	//		boid.pRigidBody->SetPosition(boid.pRigidBody->GetPosition());
	//	}
	//	else
	//	{
			// Continue the normal update procedure
			boid.ComputeForce(&boidList[0]);
			boid.Update(ms);
		}
	}

