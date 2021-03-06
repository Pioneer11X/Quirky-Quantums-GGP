#pragma once
#include "Entity.h"
#include "InputManager.h"
#include "Lights.h"

class ControlledEntity :
	public Entity
{
public:
	bool lightIsOn;
	ControlledEntity(Mesh * Object, Material* materialInput, float _posX, float _posY, float _posZ, SpotLightWrapper* newSpotLight, b2World* world = nullptr, bool isDynamic = true, float _sizeX = 1, float _sizeY = 1);
	~ControlledEntity();
	void HandleKeyboardInput(float moveSpeed);
	void UpdateSpotLightPosition();
	void UpdateLightState();
	void CheckForCollisions();
	SpotLight& GetLight();
	void UpdateSpawnPosition(float _posX, float _posY, float _posZ);
	
private:
	SpotLightWrapper* mySpotLight;
	bool canJump;
	float jumpHeight;
	float32 maxSpeed;
};
 
