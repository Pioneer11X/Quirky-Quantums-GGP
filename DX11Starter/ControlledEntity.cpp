#include "ControlledEntity.h"



ControlledEntity::ControlledEntity(Mesh * Object, Material* materialInput, float _posX, float _posY, float _posZ, SpotLightWrapper* newSpotLight, b2World* world, bool isDynamic, float _sizeX, float _sizeY) : Entity(Object, materialInput, _posX, _posY, _posZ, world, isDynamic, _sizeX, _sizeY)
{
	lightIsOn = true;
	mySpotLight = newSpotLight;
	canJump = false;
}


ControlledEntity::~ControlledEntity()
{
}

void ControlledEntity::UpdateSpotLightPosition()
{
	XMFLOAT3 newLightPos = this->GetPosition();
	newLightPos.y += .02f;
	mySpotLight->UpdateLightPoistion(newLightPos);
}

void ControlledEntity::CheckForCollisions()
{
	//If the player is flagged as "cant jump", and they are currently falling
	if (!canJump && this->GetPhysicsObject()->GetPhysicsBody()->GetLinearVelocity().y < 0) {
		//Check all the things it is in contact with
		for (b2ContactEdge* edge = this->GetPhysicsObject()->GetPhysicsBody()->GetContactList(); edge; edge = edge->next) {
			if (edge->contact->IsTouching()) {
				//Get the collision fixtures of the two objects
				b2Fixture* a = edge->contact->GetFixtureA(); //Collision object
				b2Fixture* b = edge->contact->GetFixtureB(); //This object

				//Calculate their centerpoints (in the y), and their Y halfwidths
				float aY = a->GetBody()->GetPosition().y;
				float aYExtends = a->GetAABB(0).GetExtents().y;
				float bY = b->GetBody()->GetPosition().y;
				float bYExtends = b->GetAABB(0).GetExtents().y;

				//Calculate their summed centers and halfwidths
				float aCheck = abs(aY + aYExtends);
				float bCheck = abs((bY - bYExtends) - 0.1); //-0.1 to account for minute "bobbing" of an object sitting on another

				//If a is below b, then we can jump again
				if (aCheck <= bCheck) {
					canJump = true;
				}
			}
		}
	}
}



void ControlledEntity::HandleKeyboardInput(float moveSpeed)
{

	b2Vec2 tempImpulse = b2Vec2(0.0f, 0.0f);
	//b2Vec2 zeroImpulse = b2Vec2(0.0f, 0.0f);
	if (InputManager::Instance()->GetKeyHolding(KeyPressed::LEFT))
	{
		tempImpulse = b2Vec2(-1 * moveSpeed, 0);
	}

	if (InputManager::Instance()->GetKeyHolding(KeyPressed::RIGHT))
	{
		tempImpulse = b2Vec2(moveSpeed, 0);
	}

	if (InputManager::Instance()->GetKeyDown(KeyPressed::UP))
	{
		if (canJump) {
			tempImpulse = b2Vec2(0, moveSpeed * 400);
			canJump = false;
		}
	}
	// TODO: Need to add Raycast or something to check for the player if they are actually on the ground
	if ( !(tempImpulse.x == 0 && tempImpulse.y == 0) ) {
		this->GetPhysicsObject()->GetPhysicsBody()->ApplyLinearImpulseToCenter(tempImpulse, true);
	}
	if (InputManager::Instance()->GetKeyDown(KeyPressed::FORWARD))
	{
		lightIsOn = !lightIsOn;
	}

	CheckForCollisions();
}
