#include "Game.h"
#include "Vertex.h"

// For the DirectX Math library
using namespace DirectX;

// Initialize static variables
Game* Game::instance;
b2Vec2 gravity(0.0f, -10.0f);
b2World world(gravity);

// --------------------------------------------------------
// Constructor
//
// DXCore (base class) constructor will set up underlying fields.
// DirectX itself, and our window, are not ready yet!
//
// hInstance - the application's OS-level handle (unique ID)
// --------------------------------------------------------
Game::Game(HINSTANCE hInstance)
	: DXCore( 
		hInstance,		   // The application's handle
		"Quarky Quantum",  // Text for the window's title bar
		1280,			   // Width of the window's client area
		720,			   // Height of the window's client area
		true)			   // Show extra stats (fps) in title bar?
{
	if (instance != nullptr)
	{
		delete instance;
		instance = NULL;
	}

	instance = this;

	// Initialize fields
	vertexShader = 0;
	pixelShader = 0;
	skyVertShader = 0;
	skyVertShader = 0;

	// Initialize InputMgr
	inputMgr = new InputManager();

	// Initialize Camera
	camera = new Camera();

#if defined(DEBUG) || defined(_DEBUG)
	// Do we want a console window?  Probably only in debug mode
	CreateConsoleWindow(500, 120, 32, 120);
	printf("Console window created successfully.  Feel free to printf() here.");
#endif
}

// --------------------------------------------------------
// Destructor - Clean up anything our game has created:
//  - Release all DirectX objects created here
//  - Delete any objects to prevent memory leaks
// --------------------------------------------------------
Game::~Game()
{
	// Delete our simple shader objects, which
	// will clean up their own internal DirectX stuff
	delete vertexShader;
	delete pixelShader;
	delete skyVertShader;
	delete skyPixShader;

	//Release texture D3D resources
	if (borderSRV) { borderSRV->Release(); }
	if (platformSRV) { platformSRV->Release(); }
	if (fadeOutSRV) { fadeOutSRV->Release(); }
	if (fadeInSRV) { fadeInSRV->Release(); }
	if (goalSRV) { goalSRV->Release(); }
	if (sampler) { sampler->Release(); }
	if (skyBox) { skyBox->Release(); }

	// Delete renderer
	delete renderer;

	// Delete InputMgr
	delete inputMgr;

	// Delete Camera
	delete camera;

	// Delete our custom spotlight
	delete spotLightEntity;

	////Lambda function that deletes a Mesh pointer and sets it to NULL
	//auto deleteAndSetToNull = [](void* x) { delete x; x = NULL; };

	// Delete Mesh objs
	for each (Mesh* mesh in meshObjs)
	{
		delete mesh;
		mesh = NULL;
	}

	meshObjs.clear();

	delete skyObject;

	// Delete Material Objs
	for (Material* mat : materials)
	{
		delete mat;
		mat = NULL;
	}

	materials.clear();

	DeleteEntities();

	delete skyMaterial;

	delete mapLoader;
}

void Game::DeleteEntities()
{
	// Delete Entity Objs
	for each (Entity* entity in entities)
	{
		delete entity;
		entity = NULL;
	}

	entities.clear();
}

void Game::LoadNewLevel()
{
	string levelToLoad = "Level";
	levelToLoad += std::to_string(currentLevel);
	levelToLoad += ".txt";
	mapLoader->LoadLevel(levelToLoad);
	for each (Entity* ent in mapLoader->GetLevelEntities()) {
		entities.push_back(ent);
	}

	playerChar = new ControlledEntity(meshObjs[2], materials[2], mapLoader->GetPlayerSpawnLocationX(), mapLoader->GetPlayerSpawnLocationY(), 0.0f, spotLightEntity, &world, true, 0.5f, 0.5f);
	entities.push_back(playerChar);
}

// --------------------------------------------------------
// Called once per program, after DirectX and the window
// are initialized but before the game loop.
// --------------------------------------------------------
void Game::Init()
{	
	// Initialize renderer
	renderer = new Renderer(device, context);

	// Helper methods for loading shaders, creating some basic
	// geometry to draw and some simple camera matrices.
	//  - You'll be expanding and/or replacing these later
	LoadShaders();
	CreateMatrices();

	auto LoadTexture = [&](const wchar_t* string, ID3D11ShaderResourceView** srvAddr) {

		if (S_OK !=
			CreateWICTextureFromFile(
				device,
				context,
				string,
				0, // We don't need a reference to the raw pixels
				srvAddr))
			return false;

		return true;

	};

	//Load all textures
	LoadTexture(L"./Assets/Textures/border.png", &borderSRV);
	LoadTexture(L"./Assets/Textures/platform.png", &platformSRV);
	LoadTexture(L"./Assets/Textures/transparent.png", &fadeOutSRV);
	LoadTexture(L"./Assets/Textures/solid.png", &fadeInSRV);
	LoadTexture(L"./Assets/Textures/goal.png", &goalSRV);

	//Reference: SkyMap retrieved from http://www.custommapmakers.org/skyboxes/zips/mp_met.zip
	if (S_OK !=
		CreateDDSTextureFromFile(
			device,
			context,
			L"Assets/Textures/skyBox.dds",
			0,
			&skyBox)
		)
	{
		return;
	}

	// Create a sampler decription
	D3D11_SAMPLER_DESC sampDesc = {};
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

	// Create sampler from description
	device->CreateSamplerState(&sampDesc, &sampler);

	CreateBasicGeometry();
	//InitBox2D();
	
	//Init Light
	DirectionalLight light;
	light.AmbientColor = XMFLOAT4(0.15f, 0.15f, 0.15f, 1.0f);
	light.DiffuseColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	light.Direction = XMFLOAT3(0.0f, 0.0f, 1.0f);
	light.isOn = 1;

	//Init Light 2
	DirectionalLight light2;
	light2.AmbientColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	light2.DiffuseColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	light2.Direction = XMFLOAT3(1.0f, 1.0f, 0.0f);
	light2.isOn = 0;

	dirLights.push_back(light);
	dirLights.push_back(light2);

	//Init Point Light 1
	PointLight pLight;
	pLight.Color = XMFLOAT4(1.0f, 0.57f, 0.17f, 1.0f);
	pLight.Position = XMFLOAT3(0, 5, -5);
	pLight.isOn = 0;

	pointLights.push_back(pLight);

	// Tell the input assembler stage of the pipeline what kind of
	// geometric primitives (points, lines or triangles) we want to draw.  
	// Essentially: "What kind of shape should the GPU draw with our data?"
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

// --------------------------------------------------------
// Loads shaders from compiled shader object (.cso) files using
// my SimpleShader wrapper for DirectX shader manipulation.
// - SimpleShader provides helpful methods for sending
//   data to individual variables on the GPU
// --------------------------------------------------------
void Game::LoadShaders()
{
	vertexShader = new SimpleVertexShader(device, context);
	if (!vertexShader->LoadShaderFile(L"Assets/ShaderObjs/x86/VertexShader.cso"))
		vertexShader->LoadShaderFile(L"Assets/ShaderObjs/x64/VertexShader.cso");		

	pixelShader = new SimplePixelShader(device, context);
	if(!pixelShader->LoadShaderFile(L"Assets/ShaderObjs/x86/PixelShader.cso"))
		pixelShader->LoadShaderFile(L"Assets/ShaderObjs/x64/PixelShader.cso");

	skyVertShader = new SimpleVertexShader(device, context);
	if (!skyVertShader->LoadShaderFile(L"Assets/ShaderObjs/x86/VertexShaderSky.cso"))
		skyVertShader->LoadShaderFile(L"Assets/ShaderObjs/x64/VertexShaderSky.cso");

	skyPixShader = new SimplePixelShader(device, context);
	if (!skyPixShader->LoadShaderFile(L"Assets/ShaderObjs/x86/PixelShaderSky.cso"))
		skyPixShader->LoadShaderFile(L"Assets/ShaderObjs/x64/PixelShaderSky.cso");

	// You'll notice that the code above attempts to load each
	// compiled shader file (.cso) from two different relative paths.

	// This is because the "working directory" (where relative paths begin)
	// will be different during the following two scenarios:
	//  - Debugging in VS: The "Project Directory" (where your .cpp files are) 
	//  - Run .exe directly: The "Output Directory" (where the .exe & .cso files are)

	// Checking both paths is the easiest way to ensure both 
	// scenarios work correctly, although others exist
}



// --------------------------------------------------------
// Initializes the matrices necessary to represent our geometry's 
// transformations and our 3D camera
// --------------------------------------------------------
void Game::CreateMatrices()
{

}


// --------------------------------------------------------
// Creates the geometry we're going to draw - a single triangle for now
// --------------------------------------------------------
void Game::CreateBasicGeometry()
{


	PlaySound(TEXT("./Assets/Audio/Background.wav"), NULL, SND_ASYNC | SND_LOOP);

	std::string pathModifier = "./Assets/Models/";

	materials.push_back(new Material(vertexShader, pixelShader, borderSRV, sampler));	//0
	materials.push_back(new Material(vertexShader, pixelShader, platformSRV, sampler));	//1
	materials.push_back(new Material(vertexShader, pixelShader, fadeOutSRV, sampler));	//2
	materials.push_back(new Material(vertexShader, pixelShader, fadeInSRV, sampler));//3
	materials.push_back(new Material(vertexShader, pixelShader, goalSRV, sampler));//3

	meshObjs.push_back(new Mesh(pathModifier + "sphere.obj", device));
	meshObjs.push_back(new Mesh(pathModifier + "cone.obj", device));
	meshObjs.push_back(new Mesh(pathModifier + "cylinder.obj", device));
	meshObjs.push_back(new Mesh(pathModifier + "Plane.obj", device));
	meshObjs.push_back(new Mesh(pathModifier + "torus.obj", device));
	meshObjs.push_back(new Mesh(pathModifier + "cube.obj", device));



	// Init Spot Light for the player
	SpotLight spotLight;
	spotLight.AmbientColor = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
	spotLight.DiffuseColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	spotLight.Direction = XMFLOAT3(1.0f, 0.0f, 0.0f);
	// Roughly 40 degree spread.
	spotLight.AngleRads = 0.3587f;
	spotLight.Position = XMFLOAT3(-3.0f, 0.0f, 0.0f);
	spotLight.isOn = 1;
	spotLight.SpotIntensity = 15.0f;
	spotLight.ConstAtten = 0.01f;
	spotLight.LinearAtten = 0.3f;
	spotLight.ExpoAtten = 0.005f;

	// For a collider ( Sensor / Trigger to be valid, there needs to be some volume. So, no 0.0f in any dimension.
	Entity* spotlightEnt = new Entity(meshObjs[1], materials[2], 0.0f, 0.0f, 0.0f, &world, "SpotLight" , true, true, 1.0f, 1.0f, 6.5f, 9.0f, 6.75f);
	spotlightEnt->SetAlpha(0.5f);
	spotLightEntity = new SpotLightWrapper(spotLight, 2.5f, spotlightEnt);

	mapLoader = new MapLoader(device, 2.0f, materials, meshObjs, &world);
	LoadNewLevel();

	skyMaterial = new Material(skyVertShader, skyPixShader, skyBox, sampler);
	skyObject = new Entity(meshObjs[0], skyMaterial, 0.0f, 0.0f, 0.0f, nullptr, "SkyBox", false);
	skyObject->SetTranslation(camera->GetPosition().x, camera->GetPosition().y, camera->GetPosition().z);
	skyObject->SetScale(200.0f, 200.0f, 200.0f);

}


// --------------------------------------------------------
// Handle resizing DirectX "stuff" to match the new window size.
// For instance, updating our projection matrix's aspect ratio.
// --------------------------------------------------------
void Game::OnResize()
{
	// Handle base-level DX resize stuff
	DXCore::OnResize();

	// Set projection matrix in camera
	camera->SetProjectionMatrix();

	// Update backbuffers in renderer
	renderer->UpdateBuffers(backBufferRTV, depthStencilView);
}

// --------------------------------------------------------
// Update your game here - user input, move objects, AI, etc.
// --------------------------------------------------------
void Game::Update(float deltaTime, float totalTime)
{
	// Quit if the escape key is pressed
	if (GetAsyncKeyState(VK_ESCAPE))
		Quit();

	skyObject->SetTranslation(camera->GetPosition().x, camera->GetPosition().y, camera->GetPosition().z);

#pragma region EnitityUpdates
	world.Step(deltaTime, velocityIterations, positionIterations);

	spotLightEntity->UpdateCone();
	playerChar->UpdateBounds();

	for each (Entity* ent in entities) {

		
		// Collision Check
		{

			//if ( spotLightEntity->GetEntity()->GetBounds().Intersects(ent->GetBounds()) ){
			if (spotLightEntity->coneBounds.Intersects(ent->GetBounds())) {
				if (
					(ent->GetPhysicsObject()->_physicsName != "Player") &&
					(ent->GetPhysicsObject()->_physicsName != "BasicPlatform") &&
					(ent->GetPhysicsObject()->_physicsName != "SpotLight")
					) {
					//std::cout << ent->GetPhysicsObject()->_physicsName << std::endl;
					//std::cout << ent->GetPosition().x << "," << ent->GetPosition().y << "," << ent->GetPosition().z << std::endl;
					if ("TransparentPlatform" == ent->GetPhysicsObject()->_physicsName) {
						ent->GetPhysicsObject()->ReactivatePhysicsObject();
						ent->SetAlpha(1.0f);
					}
					else if ("WeirdPlatform" == ent->GetPhysicsObject()->_physicsName) {
						ent->GetPhysicsObject()->DeactivatePhysicsObject();
						ent->SetAlpha(0.25f);
					}
					
				}
			}
			// Reset any transparent blocks that are not being intersected
			else {
				// Add a buffer between reactivating and deactivating objects.
				if (
					(ent->GetPhysicsObject()->_physicsName != "Player") &&
					(ent->GetPhysicsObject()->_physicsName != "BasicPlatform") &&
					(ent->GetPhysicsObject()->_physicsName != "SpotLight")
					) {
					if ("WeirdPlatform" == ent->GetPhysicsObject()->_physicsName)
					{
						ent->SetAlpha(1.0f);
						ent->GetPhysicsObject()->ReactivatePhysicsObject();
					}
					else if ("TransparentPlatform" == ent->GetPhysicsObject()->_physicsName) {
						ent->GetPhysicsObject()->DeactivatePhysicsObject();
						ent->SetAlpha(0.25f);
					}
						
				}
			}
		}	
		// End Collision Check.

		if (ent->NeedsPhysicsUpdate()) {
			ent->UpdatePhysicsTick();
		}
	}
#pragma endregion

#pragma region CheckEndGameCondition
	if (playerChar->GetBounds().Intersects(mapLoader->GetEndOfLevel()->GetBounds()))
	{
		switch (currentLevel)
		{
			case 3:
				currentLevel = 0;
				curScene = Menu;
				break;
			default:
				currentLevel++;
				DeleteEntities();
				LoadNewLevel();
				playerChar->UpdateSpawnPosition(mapLoader->GetPlayerSpawnLocationX(), mapLoader->GetPlayerSpawnLocationY(), 0.0f);
				break;
		}
	}
#pragma endregion

	playerChar->HandleKeyboardInput(deltaTime);
	playerChar->UpdateSpotLightPosition();
	playerChar->UpdateLightState();

	spotLightEntity->HandleKeyboardInputs(deltaTime);

	XMFLOAT3 temp;
	float camOffset = 6.5f;
	XMStoreFloat3(&temp, XMVectorSet(playerChar->GetPosition().x, playerChar->GetPosition().y + camOffset, playerChar->GetPosition().z, 0.0f));
	camera->LerpToPosition(temp, deltaTime);
	
	camera->Update(deltaTime, totalTime);
}

// --------------------------------------------------------
// Clear the screen, redraw everything, present to the user
// --------------------------------------------------------
void Game::Draw(float deltaTime, float totalTime)
{
	// Background color (Cornflower Blue in this case) for clearing
	const float color[4] = {0.1f, 0.1f, 0.1f, 0.0f};

	// Clear the render target and depth buffer (erases what's on the screen)
	//  - Do this ONCE PER FRAME
	//  - At the beginning of Draw (before drawing *anything*)
	context->ClearRenderTargetView(backBufferRTV, color);
	context->ClearDepthStencilView(
		depthStencilView,
		D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
		1.0f,
		0);

	if ( curScene == GameLevel)
		renderer->Draw(entities, skyObject, camera->GetViewMatrix(), camera->GetProjectionMatrix(), &dirLights[0], &pointLights[0], &spotLightEntity->GetSpotLight());

	if (curScene == Menu)
	{
		GUI::instance().Draw();
	}

	// Present the back buffer to the user
	//  - Puts the final frame we're drawing into the window so the user can see it
	//  - Do this exactly ONCE PER FRAME (always at the very end of the frame)
	swapChain->Present(0, 0);
}


#pragma region Mouse Input

// --------------------------------------------------------
// Helper method for mouse clicking.  We get this information
// from the OS-level messages anyway, so these helpers have
// been created to provide basic mouse input if you want it.
// --------------------------------------------------------
void Game::OnMouseDown(WPARAM buttonState, int x, int y)
{
	// Add any custom code here...

	// Save the previous mouse position, so we have it for the future
	prevMousePos.x = x;
	prevMousePos.y = y;

	// Caputure the mouse so we keep getting mouse move
	// events even if the mouse leaves the window.  we'll be
	// releasing the capture once a mouse button is released
	SetCapture(hWnd);
}

// --------------------------------------------------------
// Helper method for mouse release
// --------------------------------------------------------
void Game::OnMouseUp(WPARAM buttonState, int x, int y)
{
	// Add any custom code here...

	// We don't care about the tracking the cursor outside
	// the window anymore (we're not dragging if the mouse is up)
	ReleaseCapture();
}

// --------------------------------------------------------
// Helper method for mouse movement.  We only get this message
// if the mouse is currently over the window, or if we're 
// currently capturing the mouse.
// --------------------------------------------------------
void Game::OnMouseMove(WPARAM buttonState, int x, int y)
{
	// Save the previous mouse position, so we have it for the future
	prevMousePos.x = x;
	prevMousePos.y = y;
}

// --------------------------------------------------------
// Helper method for mouse wheel scrolling.  
// WheelDelta may be positive or negative, depending 
// on the direction of the scroll
// --------------------------------------------------------
void Game::OnMouseWheel(float wheelDelta, int x, int y)
{
	// Add any custom code here...

	camera->MoveAlongDirection(wheelDelta * 0.1f);
}

std::vector<Entity*> Game::GetEntities()
{
	return entities;
}
Game * Game::Instance()
{
	return instance;
}
unsigned int Game::GetScreenWidth()
{
	return width;
}
unsigned int Game::GetScreenHeight()
{
	return height;
}
XMFLOAT3 & Game::GetCameraPostion()
{
	// TODO: insert return statement here
	return camera->GetPosition();
}

ID3D11RenderTargetView * Game::GetBackBufferRTV()
{
	return backBufferRTV;
}

ID3D11DepthStencilView * Game::GetDSV()
{
	return depthStencilView;
}

void Game::InitBox2D()
{
	b2BodyDef groundBodyDef;
	groundBodyDef.position.Set(0.0f, -5.0f);

	groundBody = world.CreateBody(&groundBodyDef);

	b2PolygonShape groundBox;
	// X as 10 and Y as 1. No Z for Box2D
	groundBox.SetAsBox(10.0f, 0.001f);
	groundBody->CreateFixture(&groundBox, 0.0f);

	b2BodyDef playerBodyDef;
	playerBodyDef.type = b2_dynamicBody;
	playerBodyDef.position.Set(0.0f, 0.0f);

	playerBody = world.CreateBody(&playerBodyDef);

	b2PolygonShape playerBox;
	playerBox.SetAsBox(1.0f, 1.5f); // A Cylinders Projection in 2D Space.

	b2FixtureDef fixDef;
	fixDef.shape = &playerBox;
	fixDef.density = 1.0f;
	fixDef.friction = 0.3f;

	playerBody->CreateFixture(&fixDef);

}

#pragma endregion
