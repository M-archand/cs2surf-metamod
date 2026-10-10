#include "common.h"
#include "cs2surf.h"
#include "utils/logging.h"
#include "surf/option/surf_option.h"

void OnLogToFileChanged(CConVar<bool> *, CSplitScreenSlot, const bool *, const bool *)
{
	if (g_SurfPlugin.loading)
	{
		return;
	}
	g_SurfLoggingListener.CheckFile();
}

CConVar<bool> surf_log_to_file("surf_log_to_file", FCVAR_NONE, "Whether to mirror CS2Surf log output to a file in addons/cs2surf/logs.", true);

struct SurfChannel_t
{
	LogChannel channel;
	const char *name;
	LoggingChannelID_t handle;
};

static void RegisterSurfChannelTags(LoggingChannelID_t channelID)
{
	LoggingSystem_AddTagToChannel(channelID, SURF_LOG_TAG);
}

static SurfChannel_t g_SurfChannels[] = {
	// Handles are set in function below
	{LogChannel::General, "CS2Surf::General", INVALID_LOGGING_CHANNEL_ID},
	{LogChannel::AC, "CS2Surf::AC", INVALID_LOGGING_CHANNEL_ID},
	{LogChannel::DB, "CS2Surf::DB", INVALID_LOGGING_CHANNEL_ID},
	{LogChannel::Global, "CS2Surf::Global", INVALID_LOGGING_CHANNEL_ID},
	{LogChannel::Language, "CS2Surf::Language", INVALID_LOGGING_CHANNEL_ID},
	{LogChannel::MappingAPI, "CS2Surf::MappingAPI", INVALID_LOGGING_CHANNEL_ID},
	{LogChannel::Misc, "CS2Surf::Misc", INVALID_LOGGING_CHANNEL_ID},
	{LogChannel::Mode, "CS2Surf::Mode", INVALID_LOGGING_CHANNEL_ID},
	{LogChannel::Movement, "CS2Surf::Movement", INVALID_LOGGING_CHANNEL_ID},
	{LogChannel::Option, "CS2Surf::Option", INVALID_LOGGING_CHANNEL_ID},
	{LogChannel::Player, "CS2Surf::Player", INVALID_LOGGING_CHANNEL_ID},
	{LogChannel::Profile, "CS2Surf::Profile", INVALID_LOGGING_CHANNEL_ID},
	{LogChannel::Recording, "CS2Surf::Recording", INVALID_LOGGING_CHANNEL_ID},
	{LogChannel::Replays, "CS2Surf::Replays", INVALID_LOGGING_CHANNEL_ID},
	{LogChannel::Style, "CS2Surf::Style", INVALID_LOGGING_CHANNEL_ID},
	{LogChannel::Timer, "CS2Surf::Timer", INVALID_LOGGING_CHANNEL_ID},
	{LogChannel::Tip, "CS2Surf::Tip", INVALID_LOGGING_CHANNEL_ID},
	{LogChannel::Trigger, "CS2Surf::Trigger", INVALID_LOGGING_CHANNEL_ID},
};

static void PurgeLogs()
{
	int retentionDays = (int)SurfOptionService::GetOptionInt("logRetentionDays", 30);
	if (retentionDays <= 0)
	{
		return;
	}

	std::filesystem::path logDir = std::filesystem::path(g_SMAPI->GetBaseDir()) / "addons" / "cs2surf" / "logs";

	std::error_code ec;
	if (!std::filesystem::exists(logDir, ec) || ec)
	{
		return;
	}

	auto cutoff = std::filesystem::file_time_type::clock::now() - std::chrono::hours(24 * retentionDays);

	for (const auto &entry : std::filesystem::directory_iterator(logDir, ec))
	{
		if (ec)
		{
			break;
		}
		if (!entry.is_regular_file())
		{
			continue;
		}
		const auto &path = entry.path();
		if (path.extension() != ".log" || path.stem().string().rfind("cs2surf", 0) != 0)
		{
			continue;
		}
		auto writeTime = entry.last_write_time(ec);
		if (ec)
		{
			ec.clear();
			continue;
		}
		if (writeTime < cutoff)
		{
			std::filesystem::remove(path, ec);
			ec.clear();
		}
	}
}

void InitSurfLogging()
{
	LoggingSystem_RegisterLoggingListener(&g_SurfLoggingListener);
	LoggingVerbosity_t verbosity = CommandLine()->FindParm("-debug") ? LV_MAX : LV_DEFAULT;
	for (auto &entry : g_SurfChannels)
	{
		if (LoggingSystem_FindChannel(entry.name) == -1)
		{
			entry.handle = LoggingSystem_RegisterLoggingChannel(entry.name, RegisterSurfChannelTags, 0, verbosity, Color(37, 162, 255, 255));
		}
	}
	PurgeLogs();
}

LoggingChannelID_t GetServiceChannel(LogChannel channel)
{
	for (const auto &entry : g_SurfChannels)
	{
		if (entry.channel == channel)
		{
			return entry.handle;
		}
	}

	return g_SurfChannels[0].handle;
}

const char *GetServiceChannelName(LoggingChannelID_t channelID)
{
	for (const auto &entry : g_SurfChannels)
	{
		if (entry.handle == channelID)
		{
			return entry.name;
		}
	}

	return g_SurfChannels[0].name;
}

void SurfLoggingListener::Log(const LoggingContext_t *pContext, const tchar *pMessage)
{
	if (!LoggingSystem_HasTag(pContext->m_ChannelID, SURF_LOG_TAG))
	{
		return;
	}

	if (surf_log_to_file.Get())
	{
		CheckFile();
		if (!m_pFile)
		{
			return;
		}
		std::time_t t = std::time(nullptr);
		std::tm tm {};
#ifdef _WIN32
		localtime_s(&tm, &t);
#else
		localtime_r(&t, &tm);
#endif
		size_t msgLen = V_strlen(pMessage);
		bool needsNewline = (msgLen == 0 || pMessage[msgLen - 1] != '\n');
		char ts[32];
		std::strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm);

		char logMessage[1024];
		V_snprintf(logMessage, sizeof(logMessage), "[%s] %s%s", ts, pMessage, needsNewline ? "\n" : "");
		fputs(logMessage, m_pFile);
		fflush(m_pFile);
	}
}

void SurfLoggingListener::CheckFile()
{
	if (surf_log_to_file.Get())
	{
		OpenFile();
	}
	else
	{
		CloseFile();
	}
}

void SurfLoggingListener::OpenFile()
{
	if (m_pFile)
	{
		return;
	}

	std::filesystem::path logDir = std::filesystem::path(g_SMAPI->GetBaseDir()) / "addons" / "cs2surf" / "logs";

	std::error_code ec;
	if (!std::filesystem::exists(logDir))
	{
		std::filesystem::create_directories(logDir, ec);
		if (ec)
		{
			return;
		}
	}

	char szFileName[256];
	if (SurfOptionService::GetOptionInt("logNewFileOnStartup", true))
	{
		std::time_t now = std::time(nullptr);
		std::tm tm {};
#ifdef _WIN32
		localtime_s(&tm, &now);
#else
		localtime_r(&now, &tm);
#endif
		V_snprintf(szFileName, sizeof(szFileName), "cs2surf_%04d-%02d-%02d.log", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
	}
	else
	{
		V_strncpy(szFileName, "cs2surf.log", sizeof(szFileName));
	}

	std::filesystem::path fullPath = logDir / szFileName;
	m_pFile = fopen(fullPath.string().c_str(), "a");
}

void SurfLoggingListener::CloseFile()
{
	if (m_pFile)
	{
		fclose(m_pFile);
		m_pFile = nullptr;
	}
}
