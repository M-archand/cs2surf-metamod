#include "surf_trigger.h"
#include "surf/checkpoint/surf_checkpoint.h"
#include "surf/language/surf_language.h"
#include "surf/mode/surf_mode.h"
#include "surf/style/surf_style.h"
#include "surf/timer/surf_timer.h"

void SurfTriggerService::TouchModifierTrigger(TriggerTouchTracker tracker)
{
	const SurfTrigger *trigger = tracker.surfTrigger;

	if (trigger->modifier.gravity != 1)
	{
		// No gravity while paused.
		if (this->player->timerService->GetPaused())
		{
			this->player->GetPlayerPawn()->SetGravityScale(0);
			return;
		}
		this->player->GetPlayerPawn()->SetGravityScale(trigger->modifier.gravity);
	}
	this->modifiers.jumpFactor = trigger->modifier.jumpFactor;
}

bool SurfTriggerService::TouchTeleportTrigger(TriggerTouchTracker tracker)
{
	bool shouldTeleport = false;

	CEntityHandle destinationHandle = GameEntitySystem()->FindFirstEntityHandleByName(tracker.surfTrigger->teleport.destination);
	CBaseEntity *destination = dynamic_cast<CBaseEntity *>(GameEntitySystem()->GetEntityInstance(destinationHandle));
	if (!destinationHandle.IsValid() || !destination)
	{
		META_CONPRINTF("Invalid teleport destination \"%s\" on trigger with hammerID %i.\n", tracker.surfTrigger->teleport.destination,
					   tracker.surfTrigger->hammerId);
		return false;
	}

	Vector destOrigin = destination->m_CBodyComponent()->m_pSceneNode()->m_vecAbsOrigin();
	QAngle destAngles = destination->m_CBodyComponent()->m_pSceneNode()->m_angRotation();
	CBaseEntity *trigger = dynamic_cast<CBaseTrigger *>(GameEntitySystem()->GetEntityInstance(tracker.surfTrigger->entity));
	Vector triggerOrigin = Vector(0, 0, 0);
	if (trigger)
	{
		triggerOrigin = trigger->m_CBodyComponent()->m_pSceneNode()->m_vecAbsOrigin();
	}

	// NOTE: We only use the landmarks's origin if we're using a relative destination, so if
	// we're not using a relative destination and don't have it, then it's fine.
	CEntityHandle landmarkHandle = GameEntitySystem()->FindFirstEntityHandleByName(tracker.surfTrigger->teleport.landmark);
	CBaseEntity *landmark = dynamic_cast<CBaseEntity *>(GameEntitySystem()->GetEntityInstance(landmarkHandle));
	if (!landmark && tracker.surfTrigger->teleport.relative)
	{
		return false;
	}

	if (tracker.surfTrigger->type == SURFTRIGGER_TELEPORT)
	{
		f32 touchingTime = g_pSurfUtils->GetServerGlobals()->curtime - tracker.startTouchTime;
		shouldTeleport = touchingTime > tracker.surfTrigger->teleport.delay || tracker.surfTrigger->teleport.delay <= 0;
	}

	if (!shouldTeleport)
	{
		return false;
	}

	bool shouldReorientPlayer = tracker.surfTrigger->teleport.reorientPlayer && destAngles[YAW] != 0;
	Vector finalOrigin = destOrigin;

	if (tracker.surfTrigger->teleport.relative)
	{
		Vector playerOrigin;
		this->player->GetOrigin(&playerOrigin);
		Vector landmarkOrigin = landmark->m_CBodyComponent()->m_pSceneNode()->m_vecAbsOrigin();
		Vector playerOffsetFromLandmark = playerOrigin - landmarkOrigin;

		if (shouldReorientPlayer)
		{
			VectorRotate(playerOffsetFromLandmark, QAngle(0, destAngles[YAW], 0), playerOffsetFromLandmark);
		}
		finalOrigin = destOrigin + playerOffsetFromLandmark;
	}
	QAngle finalPlayerAngles;
	this->player->GetAngles(&finalPlayerAngles);
	Vector finalVelocity;
	this->player->GetVelocity(&finalVelocity);
	if (shouldReorientPlayer)
	{
		// TODO: BUG: sometimes when getting reoriented and holding a movement key
		//  the player's speed will get reduced, almost like velocity rotation
		//  and angle rotation is out of sync leading to counterstrafing.
		// Maybe we should check m_nHighestGeneratedServerViewAngleChangeIndex for angles overridding...
		VectorRotate(finalVelocity, QAngle(0, destAngles[YAW], 0), finalVelocity);
		finalPlayerAngles[YAW] -= destAngles[YAW];
		this->player->SetAngles(finalPlayerAngles);
	}
	else if (!tracker.surfTrigger->teleport.reorientPlayer && tracker.surfTrigger->teleport.useDestinationAngles)
	{
		this->player->SetAngles(destAngles);
	}

	if (tracker.surfTrigger->teleport.resetSpeed)
	{
		this->player->SetVelocity(vec3_origin);
	}
	else
	{
		this->player->SetVelocity(finalVelocity);
	}

	// We need to call teleport hook because we don't use teleport function directly.
	if (this->player->processingMovement && this->player->currentMoveData)
	{
		this->player->OnTeleport(&finalOrigin, nullptr, nullptr);
	}
	this->player->SetOrigin(finalOrigin);

	if (Surf::mapapi::IsPositionInOrAboveStartZone(finalOrigin))
	{
		this->player->SetVelocity(vec3_origin);
		this->player->timerService->TimerStop(false);
	}

	return true;
}

void SurfTriggerService::TouchPushTrigger(TriggerTouchTracker tracker)
{
	u32 pushConditions = tracker.surfTrigger->push.pushConditions;
	// clang-format off
	if (pushConditions & SurfMapPush::SURF_PUSH_TOUCH
		|| (this->player->IsButtonNewlyPressed(IN_ATTACK) && pushConditions & SurfMapPush::SURF_PUSH_ATTACK)
		|| (this->player->IsButtonNewlyPressed(IN_ATTACK2) && pushConditions & SurfMapPush::SURF_PUSH_ATTACK2)
		|| (this->player->IsButtonNewlyPressed(IN_JUMP) && pushConditions & SurfMapPush::SURF_PUSH_JUMP_BUTTON)
		|| (this->player->IsButtonNewlyPressed(IN_USE) && pushConditions & SurfMapPush::SURF_PUSH_USE))
	// clang-format on
	{
		this->AddPushEvent(tracker.surfTrigger);
	}
}

void SurfTriggerService::ApplyJumpFactor(bool replicate)
{
	const CVValue_t *impulseModeValue = player->GetCvarValueFromModeStyles("sv_jump_impulse");
	const CVValue_t newImpulseValue = (impulseModeValue->m_fl32Value * this->modifiers.jumpFactor);
	utils::SetConVarValue(player->GetPlayerSlot(), "sv_jump_impulse", &newImpulseValue, replicate);

	const CVValue_t *jumpCostValue = player->GetCvarValueFromModeStyles("sv_staminajumpcost");
	const CVValue_t newJumpCostValue = (jumpCostValue->m_fl32Value / this->modifiers.jumpFactor);
	utils::SetConVarValue(player->GetPlayerSlot(), "sv_staminajumpcost", &newJumpCostValue, replicate);
}
