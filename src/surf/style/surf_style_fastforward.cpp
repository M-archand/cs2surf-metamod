#include "surf_style_fastforward.h"
#include "../timer/surf_timer.h"

#include "utils/addresses.h"
#include "utils/interfaces.h"
#include "utils/gameconfig.h"

SurfFastForwardStylePlugin g_SurfFastForwardStylePlugin;

CGameConfig *g_pGameConfig = NULL;
SurfUtils *g_pSurfUtils = NULL;
SurfStyleManager *g_pStyleManager = NULL;
StyleServiceFactory g_StyleFactory = [](SurfPlayer *player) -> SurfStyleService * { return new SurfFastForwardStyleService(player); };
PLUGIN_EXPOSE(SurfFastForwardStylePlugin, g_SurfFastForwardStylePlugin);

const char *incompatibleStyles[] = {"SM"};

bool SurfFastForwardStylePlugin::Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();
	// Load mode
	int success;
	g_pStyleManager = (SurfStyleManager *)g_SMAPI->MetaFactory(SURF_STYLE_MANAGER_INTERFACE, &success, 0);
	if (success == META_IFACE_FAILED)
	{
		V_snprintf(error, maxlen, "Failed to find %s interface", SURF_STYLE_MANAGER_INTERFACE);
		return false;
	}
	g_pSurfUtils = (SurfUtils *)g_SMAPI->MetaFactory(SURF_UTILS_INTERFACE, &success, 0);
	if (success == META_IFACE_FAILED)
	{
		V_snprintf(error, maxlen, "Failed to find %s interface", SURF_UTILS_INTERFACE);
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

	if (!g_pStyleManager->RegisterStyle(g_PLID, STYLE_NAME_SHORT, STYLE_NAME, g_StyleFactory, incompatibleStyles,
										sizeof(incompatibleStyles) / sizeof(incompatibleStyles[0])))
	{
		V_snprintf(error, maxlen, "Failed to register style");
		return false;
	}
	ConVar_Register();

	return true;
}

bool SurfFastForwardStylePlugin::Unload(char *error, size_t maxlen)
{
	g_pStyleManager->UnregisterStyle(g_PLID);
	return true;
}

bool SurfFastForwardStylePlugin::Pause(char *error, size_t maxlen)
{
	g_pStyleManager->UnregisterStyle(g_PLID);
	return true;
}

bool SurfFastForwardStylePlugin::Unpause(char *error, size_t maxlen)
{
	if (!g_pStyleManager->RegisterStyle(g_PLID, STYLE_NAME_SHORT, STYLE_NAME, g_StyleFactory))
	{
		return false;
	}
	return true;
}

CGameEntitySystem *GameEntitySystem()
{
	return g_pSurfUtils->GetGameEntitySystem();
}

void SurfFastForwardStyleService::Init() {}

const CVValue_t *SurfFastForwardStyleService::GetTweakedConvarValue(const char *name)
{
	return nullptr;
}

void SurfFastForwardStyleService::Cleanup() {}

void SurfFastForwardStyleService::OnProcessMovement()
{
	// from CS2Fixes
	// Yes, this is what source1 does to scale player speed
	// Scale frametime during the entire movement processing step and revert right after
	CGlobalVars *globals = g_pSurfUtils->GetGlobals();

	this->initialFrametime = globals->frametime;
	globals->frametime *= 1.5f;
}

void SurfFastForwardStyleService::OnProcessMovementPost()
{
	CGlobalVars *globals = g_pSurfUtils->GetGlobals();
	globals->frametime = this->initialFrametime;
}
