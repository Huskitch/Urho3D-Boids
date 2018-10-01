#include <Urho3D/Core/CoreEvents.h>
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
#include <Urho3D/UI/Font.h>
#include <Urho3D/UI/Text.h>
#include <Urho3D/UI/UI.h>
#include <Urho3D/Network/Connection.h>
#include <Urho3D/Network/Network.h>
#include <Urho3D/Network/NetworkEvents.h>
#include <Urho3D/IO/Log.h>
#include <Urho3D/UI/LineEdit.h>
#include <Urho3D/UI/Button.h>
#include <Urho3D/UI/UIEvents.h>
#include <Urho3D/UI/Window.h>
#include <Urho3D/UI/CheckBox.h>
#include <Urho3D/Graphics/Terrain.h>
#include <Urho3D/Graphics/Skybox.h>

#include "Character.h"
#include "CharacterDemo.h"
#include "Touch.h"

#include <Urho3D/DebugNew.h>

URHO3D_DEFINE_APPLICATION_MAIN(CharacterDemo)

static DebugRenderer* debugRenderer;

static const StringHash E_CLIENTOBJECTAUTHORITY("ClientObjectAuthority");
static const StringHash PLAYER_ID("IDENTITY");
static const StringHash E_CLIENTISREADY("ClientReadyToStart");
static const StringHash E_ADDSCORE("AddScore");

static const unsigned short SERVER_PORT = 2345;

CharacterDemo::CharacterDemo(Context* context) :
    Sample(context),
    firstPerson_(false),
	boidSet(debugRenderer)
{

}

CharacterDemo::~CharacterDemo()
{

}

void CharacterDemo::Start()
{
    Sample::Start();
    if (touchEnabled_)
        touch_ = new Touch(context_, TOUCH_SENSITIVITY);

	CreateMainMenu();
	CreateClientScene();

	

	SubscribeToEvents();
	Sample::InitMouseMode(MM_RELATIVE);
}

Button* CharacterDemo::CreateButton(const String& text, int pHeight, Urho3D::Window* window)
{
	ResourceCache* cache = GetSubsystem<ResourceCache>();
	Font* font = cache->GetResource<Font>("Fonts/Anonymous Pro.ttf");

	Button* button = window->CreateChild<Button>();
	button->SetMinHeight(pHeight);
	button->SetStyleAuto();

	Text* buttonText = button->CreateChild<Text>();
	buttonText->SetFont(font, 12);
	buttonText->SetAlignment(HA_CENTER, VA_CENTER);
	buttonText->SetText(text);

	window->AddChild(button);

	return button;
}

LineEdit* CharacterDemo::CreateLineEdit(const String& text, int pHeight, Urho3D::Window* window)
{
	LineEdit* lineEdit = window->CreateChild<LineEdit>();
	lineEdit->SetMinHeight(pHeight);
	lineEdit->SetAlignment(HA_CENTER, VA_CENTER);
	lineEdit->SetText(text);
	window->AddChild(lineEdit);
	lineEdit->SetStyleAuto();

	return lineEdit;
}

void CharacterDemo::CreateMainMenu()
{
	ResourceCache* cache = GetSubsystem<ResourceCache>();
	UI* ui = GetSubsystem<UI>();
	UIElement* root = ui->GetRoot();
	XMLFile* uiStyle = cache->GetResource<XMLFile>("UI/DefaultStyle.xml");
	root->SetDefaultStyle(uiStyle);

	SharedPtr<Cursor> cursor(new Cursor(context_));
	cursor->SetStyleAuto(uiStyle);
	ui->SetCursor(cursor);

	window_ = new Window(context_);
	root->AddChild(window_);

	window_->SetMinWidth(384);
	window_->SetLayout(LM_VERTICAL, 6, IntRect(6, 6, 6, 6));
	window_->SetAlignment(HA_CENTER, VA_CENTER);
	window_->SetName("Window");
	window_->SetStyleAuto();

	Button* connectButton = CreateButton("CONNECT", 24, window_);
	addressInput = CreateLineEdit("localhost", 24, window_);
	Button* clientStartGameButton = CreateButton("CLIENT: START GAME", 24, window_);
	Button* disconnectButton = CreateButton("DISCONNECT", 24, window_);
	Button* startServerButton = CreateButton("START SERVER", 24, window_);
	Button* quitButton = CreateButton("QUIT", 24, window_);

	instructionText = root->CreateChild<Text>();
	instructionText->SetText("Score: 0");
	instructionText->SetFont(cache->GetResource<Font>("Fonts/Anonymous Pro.ttf"), 15);

	instructionText->SetHorizontalAlignment(HA_CENTER);
	instructionText->SetVerticalAlignment(VA_CENTER);
	instructionText->SetPosition(0, ui->GetRoot()->GetHeight() / 2 - 700);

	SubscribeToEvent(connectButton, E_RELEASED, URHO3D_HANDLER(CharacterDemo, HandleConnect));
	SubscribeToEvent(clientStartGameButton, E_RELEASED, URHO3D_HANDLER(CharacterDemo, HandleClientStartGame));
	SubscribeToEvent(disconnectButton, E_RELEASED, URHO3D_HANDLER(CharacterDemo, HandleDisconnect));
	SubscribeToEvent(startServerButton, E_RELEASED, URHO3D_HANDLER(CharacterDemo, HandleStartServer));
	SubscribeToEvent(quitButton, E_RELEASED, URHO3D_HANDLER(CharacterDemo, HandleQuit));
}

void CharacterDemo::CreateScene()
{
	ResourceCache* cache = GetSubsystem<ResourceCache>();

	scene_ = new Scene(context_);
	scene_->CreateComponent<Octree>();
	scene_->CreateComponent<PhysicsWorld>();
	scene_->CreateComponent<DebugRenderer>();

	debugRenderer = scene_->GetComponent<DebugRenderer>();

	cameraNode_ = new Node(context_);
	cameraNode_->SetPosition(Vector3(0.0f, 5.0f, 0.0f));

	Camera* camera = cameraNode_->CreateComponent<Camera>();
	camera->SetFarClip(300.0f);

	GetSubsystem<Renderer>()->SetViewport(0, new Viewport(context_, scene_, camera));

	Node* zoneNode = scene_->CreateChild("Zone");
	Zone* zone = zoneNode->CreateComponent<Zone>();
	zone->SetAmbientColor(Color(0.15f, 0.15f, 0.15f));
	zone->SetFogColor(Color(0.5f, 0.5f, 0.7f));
	zone->SetFogStart(10.0f);
	zone->SetFogEnd(150.0f);
	zone->SetBoundingBox(BoundingBox(-1000.0f, 1000.0f));

	Node* lightNode = scene_->CreateChild("DirectionalLight");
	lightNode->SetDirection(Vector3(0.3f, -0.5f, 0.425f));
	Light* light = lightNode->CreateComponent<Light>();
	light->SetLightType(LIGHT_DIRECTIONAL);
	light->SetCastShadows(true);
	light->SetShadowBias(BiasParameters(0.00025f, 0.5f));
	light->SetShadowCascade(CascadeParameters(10.0f, 50.0f, 200.0f, 0.0f,
		0.8f));
	light->SetSpecularIntensity(0.5f);

	Node* floorNode = scene_->CreateChild("Floor");
	floorNode->SetPosition(Vector3(0.0f, -0.5f, 0.0f));
	floorNode->SetScale(Vector3(23.0f, 23.0f, 23.0f));
	StaticModel* object = floorNode->CreateComponent<StaticModel>();
	object->SetModel(cache->GetResource<Model>("Models/Dome.mdl"));
	object->SetMaterial(cache->GetResource<Material>("Materials/Water.xml"));

	// TERRAIN
	Node* terrainNode = scene_->CreateChild("Terrain");
	terrainNode->SetPosition(Vector3::ZERO);
	terrain = terrainNode->CreateComponent<Terrain>();
	terrain->SetPatchSize(16);
	terrain->SetSpacing(Vector3(0.5f, 0.1f, 0.5f));
	terrain->SetSmoothing(true);
	terrain->SetHeightMap(cache->GetResource<Image>("Textures/MoonMap.png"));
	terrain->SetMaterial(cache->GetResource<Material>("Materials/Terrain.xml"));

	terrain->SetOccluder(true);
	RigidBody* Terrainbody = terrainNode->CreateComponent<RigidBody>();
	Terrainbody->SetCollisionLayer(2);
	CollisionShape* Terrainshape = terrainNode->CreateComponent<CollisionShape>();
	Terrainshape->SetTerrain();
	RigidBody* bodyTerrain = terrainNode->CreateComponent<RigidBody>();
	bodyTerrain->SetCollisionLayer(2);
	CollisionShape* shapeTerrain = terrainNode->CreateComponent<CollisionShape>();
	shapeTerrain->SetTerrain();

	cameraNode_->SetPosition(Vector3(0.0f, terrain->GetHeight(cameraNode_->GetPosition()) + 2.25f, 0.0f));

	// WATER

	Graphics* graphics = GetSubsystem<Graphics>();
	waterNode_ = scene_->CreateChild("Water");
	waterNode_->SetScale(Vector3(2048.0f, 1.0f, 2048.0f));
	waterNode_->SetPosition(Vector3(0.0f, 60.55f, 0.0f));
	waterNode_->SetRotation(Quaternion(180.0f, 0.0, 0.0f));
	StaticModel* water = waterNode_->CreateComponent<StaticModel>();
	water->SetModel(cache->GetResource<Model>("Models/Plane.mdl"));
	water->SetMaterial(cache->GetResource<Material>("Materials/Water.xml"));
	// Set a different viewmask on the water plane to be able to hide it from the reflection camera
	water->SetViewMask(0x80000000);

	// Create a mathematical plane to represent the water in calculations
	waterPlane_ = Plane(waterNode_->GetWorldRotation() * Vector3(0.0f, 1.0f, 0.0f), waterNode_->GetWorldPosition());
	//Create a downward biased plane for reflection view clipping. Biasing is necessary to avoid too aggressive //clipping
	waterClipPlane_ = Plane(waterNode_->GetWorldRotation() * Vector3(0.0f, 1.0f, 0.0f),
		waterNode_->GetWorldPosition() - Vector3(0.0f, 0.01f, 0.0f));

	reflectionCameraNode_ = cameraNode_->CreateChild();
	Camera* reflectionCamera = reflectionCameraNode_->CreateComponent<Camera>();
	reflectionCamera->SetFarClip(50.0);
	reflectionCamera->SetViewMask(0x7fffffff); // Hide objects with only bit 31 in the viewmask (the water plane)
	reflectionCamera->SetAutoAspectRatio(true);
	reflectionCamera->SetUseReflection(true);
	reflectionCamera->SetReflectionPlane(waterPlane_);
	reflectionCamera->SetUseClipping(true); // Enable clipping of geometry behind water plane
	reflectionCamera->SetClipPlane(waterClipPlane_);
	// The water reflection texture is rectangular. Set reflection camera aspect ratio to match
	reflectionCamera->SetAspectRatio((float)graphics->GetWidth() / (float)graphics->GetHeight());
	// View override flags could be used to optimize reflection rendering. For example disable shadows
	//reflectionCamera->SetViewOverrideFlags(VO_DISABLE_SHADOWS);
	// Create a texture and setup viewport for water reflection. Assign the reflection texture to the diffuse
	// texture unit of the water material
	int texSize = 1024;
	SharedPtr<Texture2D> renderTexture(new Texture2D(context_));
	renderTexture->SetSize(texSize, texSize, Graphics::GetRGBFormat(), TEXTURE_RENDERTARGET);
	renderTexture->SetFilterMode(FILTER_BILINEAR);
	RenderSurface* surface = renderTexture->GetRenderSurface();
	SharedPtr<Viewport> rttViewport(new Viewport(context_, scene_, reflectionCamera));
	surface->SetViewport(0, rttViewport);
	Material* waterMat = cache->GetResource<Material>("Materials/Water.xml");
	waterMat->SetTexture(TU_DIFFUSE, renderTexture);

	Node* skyNode = scene_->CreateChild("Sky");
	skyNode->SetScale(500.0f); // The scale actually does not matter
	Skybox* skybox = skyNode->CreateComponent<Skybox>();
	skybox->SetModel(cache->GetResource<Model>("Models/Box.mdl"));
	skybox->SetMaterial(cache->GetResource<Material>("Materials/Skybox.xml"));

	unsigned NUM_OBJECTS = 1000;
	for (unsigned i = 0; i < NUM_OBJECTS; ++i)
	{
		Node* objectNode = scene_->CreateChild("Box");
		Vector3 position(Random(1500.0f) - 1000.0f, 0.0f, Random(1500.0f) - 1000.0f);
		position.y_ = terrain->GetHeight(position);
		objectNode->SetPosition(position);
		// Create a rotation quaternion from up vector to terrain normal
		objectNode->SetRotation(Quaternion(Vector3(0.0f, 1.0f, 0.0f), terrain->GetNormal(position)));
		objectNode->SetScale(3.0f);
		StaticModel* object = objectNode->CreateComponent<StaticModel>();
		object->SetModel(cache->GetResource<Model>("Models/Mushroom.mdl"));
		object->SetMaterial(cache->GetResource<Material>("Materials/Mushroom.xml"));
		object->SetCastShadows(true);
	}

	boidSet.Initialise(cache, scene_, debugRenderer);
}

void CharacterDemo::CreateClientScene()
{
	ResourceCache* cache = GetSubsystem<ResourceCache>();

	scene_ = new Scene(context_);
	scene_->CreateComponent<Octree>(LOCAL);
	scene_->CreateComponent<PhysicsWorld>(LOCAL);

	cameraNode_ = new Node(context_);
	cameraNode_->SetPosition(Vector3(0.0f, 5.0f, 0.0f));

	Camera* camera = cameraNode_->CreateComponent<Camera>();
	camera->SetFarClip(300.0f);

	GetSubsystem<Renderer>()->SetViewport(0, new Viewport(context_, scene_, camera));

	Node* zoneNode = scene_->CreateChild("Zone", LOCAL);
	Zone* zone = zoneNode->CreateComponent<Zone>();
	zone->SetAmbientColor(Color(0.15f, 0.15f, 0.15f));
	zone->SetFogColor(Color(0.5f, 0.5f, 0.7f));
	zone->SetFogStart(10.0f);
	zone->SetFogEnd(150.0f);
	zone->SetBoundingBox(BoundingBox(-1000.0f, 1000.0f));

	Node* lightNode = scene_->CreateChild("DirectionalLight", LOCAL);
	lightNode->SetDirection(Vector3(0.3f, -0.5f, 0.425f));
	Light* light = lightNode->CreateComponent<Light>();
	light->SetLightType(LIGHT_DIRECTIONAL);
	light->SetCastShadows(true);
	light->SetShadowBias(BiasParameters(0.00025f, 0.5f));
	light->SetShadowCascade(CascadeParameters(10.0f, 50.0f, 200.0f, 0.0f,
		0.8f));
	light->SetSpecularIntensity(0.5f);

	Node* floorNode = scene_->CreateChild("Floor", LOCAL);
	floorNode->SetPosition(Vector3(0.0f, -0.5f, 0.0f));
	floorNode->SetScale(Vector3(23.0f, 23.0f, 23.0f));
	StaticModel* object = floorNode->CreateComponent<StaticModel>();
	object->SetModel(cache->GetResource<Model>("Models/Dome.mdl"));
	object->SetMaterial(cache->GetResource<Material>("Materials/Water.xml"));
}

// CLIENT
void CharacterDemo::HandleConnect(StringHash eventType, VariantMap& eventData)
{
	Log::WriteRaw("HandleConnect called");

	CreateClientScene();

	Network* network = GetSubsystem<Network>();
	String address = addressInput->GetText().Trimmed();

	if (address.Empty()) 
	{ 
		address = "localhost"; 
	}

	network->Connect(address, SERVER_PORT, scene_);

}

// CLIENT
void CharacterDemo::HandleDisconnect(StringHash eventType, VariantMap& eventData)
{
	Log::WriteRaw("HandleDisconnect called");
}

// SERVER
void CharacterDemo::HandleStartServer(StringHash eventType, VariantMap& eventData)
{
	Log::WriteRaw("HandleStartServer called");

	CreateScene();

	Network* network = GetSubsystem<Network>();
	network->StartServer(SERVER_PORT);

	menuVisible = !menuVisible;
}

// CLIENT + SERVER
void CharacterDemo::HandleQuit(StringHash eventType, VariantMap& eventData)
{
	Log::WriteRaw("HandleQuit called");
	engine_->Exit();
}

// SERVER
void CharacterDemo::HandleClientConnected(StringHash eventType, VariantMap& eventData)
{
	Log::WriteRaw("(HandleClientConnected) A client has connected!");

	Connection* newConnection = static_cast<Connection*>(eventData[ClientConnected::P_CONNECTION].GetPtr());
	newConnection->SetScene(scene_);
}

// SERVER
void CharacterDemo::HandleClientStartGame(StringHash eventType, VariantMap & eventData)
{
	Log::WriteRaw("Client has pressed START GAME \n");

	if (clientObjectID_ == 0)
	{
		Network* network = GetSubsystem<Network>();
		Connection* serverConnection = network->GetServerConnection();
		if (serverConnection)
		{
			VariantMap remoteEventData;
			remoteEventData[PLAYER_ID] = 0;
			serverConnection->SendRemoteEvent(E_CLIENTISREADY, true, remoteEventData);
		}
	}
}


//SERVER
void CharacterDemo::HandleClientToServerReadyToStart(StringHash eventType, VariantMap& eventData)
{
	using namespace ClientConnected;
	Connection* newConnection = static_cast<Connection*>(eventData[P_CONNECTION].GetPtr());

	Node* newObject = CreateControllableObject();
	serverObjects_[newConnection] = newObject;

	VariantMap remoteEventData;
	remoteEventData[PLAYER_ID] = newObject->GetID();
	newConnection->SendRemoteEvent(E_CLIENTOBJECTAUTHORITY, true, remoteEventData);
}

void CharacterDemo::CheckCollisions()
{
	Network* network = GetSubsystem<Network>();
	const Vector<SharedPtr<Connection> >& connections = network->GetClientConnections();
	//Server: go through every client connected
	for (unsigned i = 0; i < connections.Size(); ++i)
	{
		Connection* connection = connections[i];
		// Get the object this connection is controlling
		Node* ballNode = serverObjects_[connection];

		for (Boid boid : boidSet.boidList)
		{
			if ((ballNode->GetComponent<RigidBody>()->GetPosition() - boid.pRigidBody->GetPosition()).LengthSquared() < 30)
			{
				Log::WriteRaw("ADDING SCORE");
				VariantMap remoteEventData;
				remoteEventData[PLAYER_ID] = ballNode->GetID();
				connection->SendRemoteEvent(E_ADDSCORE, true, remoteEventData);
			}
		}
	}
}

void CharacterDemo::AddScore(StringHash eventType, VariantMap& eventData)
{
	Score += 1;
	instructionText->SetText("SCORE: 1");
}

//SERVER
void CharacterDemo::HandleServerToClientObjectID(StringHash eventType, VariantMap & eventData)
{
	clientObjectID_ = eventData[PLAYER_ID].GetUInt();
	Log::WriteRaw("Client ID: " + clientObjectID_);
}

//SERVER
Node* CharacterDemo::CreateControllableObject()
{
	ResourceCache* cache = GetSubsystem<ResourceCache>();
	// Create the scene node & visual representation. This will be a replicated object
	Node* ballNode = scene_->CreateChild("AClientBall");
	ballNode->SetPosition(Vector3(0.0f, terrain->GetHeight(Vector3(0.0f, 2.0f, 0.0f)) + 2.25f, 0.0f));
	ballNode->SetRotation(Quaternion(90.0f, 0.0f, 0.0f));
	ballNode->SetScale(0.3f);
	StaticModel* ballObject = ballNode->CreateComponent<StaticModel>();
	ballObject->SetModel(cache->GetResource<Model>("Models/great_white_shark.mdl"));
	ballObject->SetMaterial(cache->GetResource<Material>("Materials/StoneSmall.xml"));
	// Create the physics components
	RigidBody* body = ballNode->CreateComponent<RigidBody>();
	body->SetMass(1.0f);
	body->SetUseGravity(false);
	body->SetLinearDamping(0.5f);

	CollisionShape* shape = ballNode->CreateComponent<CollisionShape>();
	shape->SetTriangleMesh(ballObject->GetModel(), 0);

	Log::WriteRaw("Created player object");
	return ballNode;

}

// SERVER
void CharacterDemo::HandleClientDisconnected(StringHash eventType, VariantMap& eventData)
{
	using namespace ClientConnected;
}

void CharacterDemo::CreateCharacter()
{

}

void CharacterDemo::MoveCamera()
{
	if (clientObjectID_)
	{
		Node* ballNode = this->scene_->GetNode(clientObjectID_);
		if (ballNode)
		{
			const float CAMERA_DISTANCE = 15.0f;
			cameraNode_->SetPosition(ballNode->GetPosition() + cameraNode_->GetRotation()
				* Vector3::BACK * CAMERA_DISTANCE);
		}
	}
}

void CharacterDemo::ProcessClientControls()
{
	Network* network = GetSubsystem<Network>();
	const Vector<SharedPtr<Connection> >& connections = network->GetClientConnections();
	//Server: go through every client connected
	for (unsigned i = 0; i < connections.Size(); ++i)
	{
		Connection* connection = connections[i];
		// Get the object this connection is controlling
		Node* ballNode = serverObjects_[connection];
		// Client has no item connected
		if (!ballNode) continue;
		RigidBody* body = ballNode->GetComponent<RigidBody>();
		// Get the last controls sent by the client
		const Controls& controls = connection->GetControls();
		// Torque is relative to the forward vector
		Quaternion rotation(controls.pitch_, controls.yaw_, 0.0f);
		const float FORCE = 15.0f;
		Quaternion rot2(0.0f, controls.yaw_ - 90.0f, -controls.pitch_ -90.0f);

		body->SetRotation(rot2);
		if (controls.buttons_ & CTRL_FORWARD)
			body->ApplyForce(rotation * Vector3::FORWARD * FORCE);

		//if (controls.buttons_ & CTRL_BACK)
		//	body->ApplyForce(rotation * Vector3::BACK * MOVE_TORQUE);
		//if (controls.buttons_ & CTRL_LEFT)
		//	body->ApplyForce(rotation * Vector3::LEFT * MOVE_TORQUE);
		//if (controls.buttons_ & CTRL_RIGHT)
		//	body->ApplyForce(rotation * Vector3::RIGHT * MOVE_TORQUE);
	}
}


void CharacterDemo::SubscribeToEvents()
{
	SubscribeToEvent(E_UPDATE, URHO3D_HANDLER(CharacterDemo, HandleUpdate));

	SubscribeToEvent(E_CLIENTCONNECTED, URHO3D_HANDLER(CharacterDemo, HandleClientConnected));
	SubscribeToEvent(E_CLIENTDISCONNECTED, URHO3D_HANDLER(CharacterDemo, HandleClientDisconnected));

	SubscribeToEvent(E_CLIENTISREADY, URHO3D_HANDLER(CharacterDemo, HandleClientToServerReadyToStart));
	GetSubsystem<Network>()->RegisterRemoteEvent(E_CLIENTISREADY);

	SubscribeToEvent(E_CLIENTOBJECTAUTHORITY, URHO3D_HANDLER(CharacterDemo, HandleServerToClientObjectID));
	GetSubsystem<Network>()->RegisterRemoteEvent(E_CLIENTOBJECTAUTHORITY);

	SubscribeToEvent(E_ADDSCORE, URHO3D_HANDLER(CharacterDemo, AddScore));
	GetSubsystem<Network>()->RegisterRemoteEvent(E_ADDSCORE);
}

// CLIENT
Controls CharacterDemo::FromClientToServerControls()
{
	MoveCamera();

	Input* input = GetSubsystem<Input>();
	Controls controls;

	controls.Set(CTRL_FORWARD, input->GetKeyDown(KEY_W));
	controls.Set(CTRL_BACK, input->GetKeyDown(KEY_S));
	controls.Set(CTRL_LEFT, input->GetKeyDown(KEY_A));
	controls.Set(CTRL_RIGHT, input->GetKeyDown(KEY_D));
	controls.Set(1024, input->GetKeyDown(KEY_E));

	controls.yaw_ = yaw_;
	controls.pitch_ = pitch_;
	return controls;
}


void CharacterDemo::HandleUpdate(StringHash eventType, VariantMap& eventData)
{
	Network* network = GetSubsystem<Network>();
	Connection* serverConnection = network->GetServerConnection();

	FrameInfo frameInfo = GetSubsystem<Renderer>()->GetFrameInfo();
	//instructionText->SetText("FPS: " + String(1.0 / frameInfo.timeStep_));
	instructionText->SetText("SCORE: " + Score);

	if (serverConnection)
	{
		serverConnection->SetPosition(cameraNode_->GetPosition()); // send camera position too
		serverConnection->SetControls(FromClientToServerControls()); // send controls to server
	}

	else if (network->IsServerRunning())
	{
		ProcessClientControls(); // take data from clients, process it
	}

	const float MOVE_SPEED = 20.0f;
	const float MOUSE_SENSITIVITY = 0.1f;

	using namespace Update;
	float timeStep = eventData[P_TIMESTEP].GetFloat();

	if (GetSubsystem<UI>()->GetFocusElement()) 
		return;

	Input* input = GetSubsystem<Input>();
	IntVector2 mouseMove = input->GetMouseMove();

	yaw_ += MOUSE_SENSITIVITY * mouseMove.x_;
	pitch_ += MOUSE_SENSITIVITY * mouseMove.y_;
	pitch_ = Clamp(pitch_, -90.0f, 90.0f);

	if (!menuVisible)
	{
		cameraNode_->SetRotation(Quaternion(pitch_, yaw_, 0.0f));

		if (input->GetKeyDown(KEY_W))
			cameraNode_->Translate(Vector3::FORWARD * MOVE_SPEED * timeStep);
		if (input->GetKeyDown(KEY_S))
			cameraNode_->Translate(Vector3::BACK * MOVE_SPEED * timeStep);
		if (input->GetKeyDown(KEY_A))
			cameraNode_->Translate(Vector3::LEFT * MOVE_SPEED * timeStep);
		if (input->GetKeyDown(KEY_D))
			cameraNode_->Translate(Vector3::RIGHT * MOVE_SPEED * timeStep);

		if (boidSet.Initialized)
		{
			//CheckCollisions();
			boidSet.Update(timeStep);
		}

	}

	if (input->GetKeyPress(KEY_M))
		menuVisible = !menuVisible;

	GetSubsystem<UI>()->GetCursor()->SetVisible(menuVisible);
	window_->SetVisible(menuVisible);
}

void CharacterDemo::HandlePostUpdate(StringHash eventType, VariantMap& eventData)
{

}
