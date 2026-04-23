#include "surf_style.h"
#include "version_gen.h"

#define STYLE_NAME       "SlowMo"
#define STYLE_NAME_SHORT "SM"

class SurfSlowMoStylePlugin : public ISmmPlugin, public IMetamodListener
{
public:
	bool Load(PluginId id, ISmmAPI *ismm, char *error, size_t maxlen, bool late);
	bool Unload(char *error, size_t maxlen);
	bool Pause(char *error, size_t maxlen);
	bool Unpause(char *error, size_t maxlen);

public:
	const char *GetAuthor()
	{
		return PLUGIN_AUTHOR;
	}

	const char *GetName()
	{
		return "CS2Surf-Style-SlowMo";
	}

	const char *GetDescription()
	{
		return "Slow Motion style plugin for CS2Surf";
	}

	const char *GetURL()
	{
		return PLUGIN_URL;
	}

	const char *GetLicense()
	{
		return PLUGIN_LICENSE;
	}

	const char *GetVersion()
	{
		return PLUGIN_FULL_VERSION;
	}

	const char *GetDate()
	{
		return __DATE__;
	}

	const char *GetLogTag()
	{
		return PLUGIN_LOGTAG;
	}
};

class SurfSlowMoStyleService : public SurfStyleService
{
	using SurfStyleService::SurfStyleService;

private:
	float initialFrametime;

public:
	virtual const char *GetStyleName() override
	{
		return "SlowMo";
	}

	virtual const char *GetStyleShortName() override
	{
		return "SM";
	}

	virtual const CVValue_t *GetTweakedConvarValue(const char *name) override;
	virtual void Init() override;
	virtual void Cleanup() override;
	virtual void OnProcessMovement() override;
	virtual void OnProcessMovementPost() override;
};
