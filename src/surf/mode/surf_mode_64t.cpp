#include "surf_mode_64t.h"

#define MODE_NAME_SHORT "64t"
#define MODE_NAME       "64tick"

void Surf64tModeService::Reset()
{
	this->hasValidDesiredViewAngle = {};
	this->lastValidDesiredViewAngle = vec3_angle;
	this->lastJumpReleaseTime = {};
	this->oldDuckPressed = {};
	this->forcedUnduck = {};
	this->postProcessMovementZSpeed = {};

	this->angleHistory.RemoveAll();
	this->leftPreRatio = {};
	this->rightPreRatio = {};
	this->bonusSpeed = {};
	this->maxPre = {};

	this->didTPM = {};
	this->overrideTPM = {};
	this->tpmVelocity = vec3_origin;
	this->tpmOrigin = vec3_origin;
	this->lastValidPlane = vec3_origin;

	this->airMoving = {};
	this->tpmTriggerFixOrigins.RemoveAll();
}

void Surf64tModeService::Cleanup()
{
	auto pawn = this->player->GetPlayerPawn();
	if (pawn)
	{
		pawn->m_flVelocityModifier(1.0f);
	}
}

const char *Surf64tModeService::GetModeName()
{
	return MODE_NAME;
}

const char *Surf64tModeService::GetModeShortName()
{
	return MODE_NAME_SHORT;
}

const CVValue_t *Surf64tModeService::GetModeConVarValues()
{
	return modeCvarValues;
}
