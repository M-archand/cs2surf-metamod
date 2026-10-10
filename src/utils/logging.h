#pragma once
#include <filesystem>

#include "convar.h"
#include "tier0/logging.h"

extern CConVar<bool> surf_log_to_file;

// Common tag applied to every CS2Surf-owned logging channel.
// SurfLoggingListener uses this to filter which channels it processes.
#define SURF_LOG_TAG "CS2Surf"

// Enum for service selection - maps to individual channels
enum class LogChannel
{
	General,
	AC,
	DB,
	Global,
	Language,
	MappingAPI,
	Misc,
	Mode,
	Movement,
	Option,
	Player,
	Profile,
	Recording,
	Replays,
	Style,
	Timer,
	Tip,
	Trigger,
};

// Register CS2Surf logging listener and all CS2Surf channels from the channel table. Also purge old log files.
void InitSurfLogging();

// Get the logging channel for a given service.
LoggingChannelID_t GetServiceChannel(LogChannel service);

// Get the name of a logging channel
const char *GetServiceChannelName(LoggingChannelID_t channelID);

#define SURF_LOG_INFO(service, fmt, ...) \
	do \
	{ \
		const LoggingChannelID_t channel = GetServiceChannel(service); \
		LoggingSystem_Log(channel, LS_MESSAGE, "[%s] [INFO] " fmt, GetServiceChannelName(channel), ##__VA_ARGS__); \
	} while (0)

#define SURF_LOG_DEBUG(service, fmt, ...) \
	do \
	{ \
		const LoggingChannelID_t channel = GetServiceChannel(service); \
		LoggingSystem_Log(channel, LS_DETAILED, "[%s] [DEBUG] " fmt, GetServiceChannelName(channel), ##__VA_ARGS__); \
	} while (0)

#define SURF_LOG_WARN(service, fmt, ...) \
	do \
	{ \
		const LoggingChannelID_t channel = GetServiceChannel(service); \
		LoggingSystem_Log(channel, LS_WARNING, "[%s] [WARN] " fmt, GetServiceChannelName(channel), ##__VA_ARGS__); \
	} while (0)

#define SURF_LOG_ERROR(service, fmt, ...) \
	do \
	{ \
		const LoggingChannelID_t channel = GetServiceChannel(service); \
		LoggingSystem_Log(channel, LS_ERROR, "[%s] [ERROR] " fmt, GetServiceChannelName(channel), ##__VA_ARGS__); \
	} while (0)

// Logging listener that claims every channel tagged with SURF_LOG_TAG and optionally
// mirrors output to a log file.
class SurfLoggingListener : public ILoggingListener
{
public:
	void Log(const LoggingContext_t *pContext, const tchar *pMessage) override;
	void CheckFile();

private:
	void OpenFile();
	void CloseFile();
	FILE *m_pFile = nullptr;
};

inline SurfLoggingListener g_SurfLoggingListener {};
