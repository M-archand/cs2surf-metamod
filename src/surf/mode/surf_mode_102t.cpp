#include "surf_mode_102t.h"

#define MODE_NAME_SHORT "102t"
#define MODE_NAME       "102tick"

Surf102tModePlugin g_Surf102tModePlugin;

CGameConfig *g_pGameConfig = NULL;
SurfUtils *g_pSurfUtils = NULL;
SurfModeManager *g_pModeManager = NULL;
MappingInterface *g_pMappingApi = NULL;
ModeServiceFactory g_ModeFactory = [](SurfPlayer *player) -> SurfModeService * { return new Surf102tModeService(player); };
PLUGIN_EXPOSE(Surf102tModePlugin, g_Surf102tModePlugin);

bool Surf102tModePlugin::Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();
	// Load mode
	int success;
	g_pModeManager = (SurfModeManager *)g_SMAPI->MetaFactory(SURF_MODE_MANAGER_INTERFACE, &success, 0);
	if (success == META_IFACE_FAILED)
	{
		V_snprintf(error, maxlen, "Failed to find %s interface", SURF_MODE_MANAGER_INTERFACE);
		return false;
	}
	g_pSurfUtils = (SurfUtils *)g_SMAPI->MetaFactory(SURF_UTILS_INTERFACE, &success, 0);
	if (success == META_IFACE_FAILED)
	{
		V_snprintf(error, maxlen, "Failed to find %s interface", SURF_UTILS_INTERFACE);
		return false;
	}
	g_pMappingApi = (MappingInterface *)g_SMAPI->MetaFactory(SURF_MAPPING_INTERFACE, &success, 0);
	if (success == META_IFACE_FAILED)
	{
		V_snprintf(error, maxlen, "Failed to find %s interface", SURF_MAPPING_INTERFACE);
		return false;
	}
	modules::Initialize();
	if (!interfaces::Initialize(ismm, error, maxlen))
	{
		V_snprintf(error, maxlen, "Failed to initialize interfaces");
		return false;
	}

	if (nullptr == (g_pGameConfig = g_pSurfUtils->GetGameConfig()))
	{
		V_snprintf(error, maxlen, "Failed to get game config");
		return false;
	}

	if (!g_pModeManager->RegisterMode(g_PLID, MODE_NAME_SHORT, MODE_NAME, g_ModeFactory))
	{
		V_snprintf(error, maxlen, "Failed to register mode");
		return false;
	}

	ConVar_Register();
	return true;
}

bool Surf102tModePlugin::Unload(char *error, size_t maxlen)
{
	g_pModeManager->UnregisterMode(g_PLID);
	return true;
}

bool Surf102tModePlugin::Pause(char *error, size_t maxlen)
{
	g_pModeManager->UnregisterMode(g_PLID);
	return true;
}

bool Surf102tModePlugin::Unpause(char *error, size_t maxlen)
{
	if (!g_pModeManager->RegisterMode(g_PLID, MODE_NAME_SHORT, MODE_NAME, g_ModeFactory))
	{
		return false;
	}
	return true;
}

CGameEntitySystem *GameEntitySystem()
{
	return g_pSurfUtils->GetGameEntitySystem();
}

void Surf102tModeService::Reset()
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

void Surf102tModeService::Cleanup()
{
	auto pawn = this->player->GetPlayerPawn();
	if (pawn)
	{
		pawn->m_flVelocityModifier(1.0f);
	}
}

const char *Surf102tModeService::GetModeName()
{
	return MODE_NAME;
}

const char *Surf102tModeService::GetModeShortName()
{
	return MODE_NAME_SHORT;
}

const CVValue_t *Surf102tModeService::GetModeConVarValues()
{
	return modeCvarValues;
}
