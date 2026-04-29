#include "surf_recording.h"
#include "utils/simplecmds.h"
#include "surf/timer/surf_timer.h"
#include "filesystem.h"
#include "cs2surf.h"
#include "utils/ctimer.h"
#include "surf/surf.h"
#include "common.h"
#include "sdk/cskeletoninstance.h"
#include "sdk/usercmd.h"
#include "surf/replays/compression.h"

extern CConVar<bool> surf_replay_recording_debug;

ManualRecorder::ManualRecorder(SurfPlayer *player, f32 duration, SurfPlayer *savedBy) : Recorder(player, duration, RP_MANUAL, true)
{
	if (savedBy)
	{
		auto *manual = replayHeader.mutable_manual();
		auto *savedByMsg = manual->mutable_saved_by();
		savedByMsg->set_name(savedBy->GetName());
		savedByMsg->set_steamid64(savedBy->GetSteamId64());
	}
}

i32 ManualRecorder::WriteHeader(FileHandle_t file)
{
	return Recorder::WriteHeader(file);
}

CheaterRecorder::CheaterRecorder(SurfPlayer *player, const char *reason, SurfPlayer *savedBy) : Recorder(player, 120.0f, RP_CHEATER, true)
{
	auto *cheater = replayHeader.mutable_cheater();
	cheater->set_reason(reason);
	if (savedBy)
	{
		auto *reporter = cheater->mutable_reporter();
		reporter->set_name(savedBy->GetName());
		reporter->set_steamid64(savedBy->GetSteamId64());
	}
}

i32 CheaterRecorder::WriteHeader(FileHandle_t file)
{
	return Recorder::WriteHeader(file);
}

// RunRecorder Implementation
RunRecorder::RunRecorder(SurfPlayer *player) : Recorder(player, 5.0f, RP_RUN,  true)
{
	auto *runProto = replayHeader.mutable_run();
	runProto->set_course_name(player->timerService->GetCourse()->GetName().Get());
	auto modeInfo = Surf::mode::GetModeInfo(player->modeService);
	runProto->mutable_mode()->set_name(modeInfo.longModeName.Get());
	runProto->mutable_mode()->set_short_name(modeInfo.shortModeName.Get());
	runProto->mutable_mode()->set_md5(modeInfo.md5);
	FOR_EACH_VEC(player->styleServices, i)
	{
		auto styleInfo = Surf::style::GetStyleInfo(player->styleServices[i]);
		auto *styleMsg = runProto->add_styles();
		styleMsg->set_name(styleInfo.longName);
		styleMsg->set_short_name(styleInfo.shortName);
		styleMsg->set_md5(styleInfo.md5);
	}
}

void RunRecorder::End(f32 time, i32 numTeleports)
{
	auto *runProto = replayHeader.mutable_run();
	runProto->set_time(time);
	this->desiredStopTime = g_pSurfUtils->GetServerGlobals()->curtime + 4.0f;
}

i32 RunRecorder::WriteHeader(FileHandle_t file)
{
	return Recorder::WriteHeader(file);
}

// Recorder Implementation
Recorder::Recorder(SurfPlayer *player, f32 numSeconds, ReplayType type, bool copyTimerEvents)
{
	Recorder::Init(replayHeader, player, type);

	CircularRecorder *circular = player->recordingService->circularRecording;
	if (!circular)
	{
		// No circular recorder initialized, nothing to copy
		return;
	}
	// Go through the events and fetch the first events within the time frame.
	// Iterate backwards to find the earliest event that is still within the time frame.
	i32 earliestWeaponIndex = -1;
	i32 earliestModeEventIndex = -1;
	i32 earliestStyleEventIndex = -1;
	i32 earliestCheckpointEventIndex = -1;

	if (circular->tickData->GetReadAvailable() == 0)
	{
		return;
	}
	i32 numTickData = MIN(circular->tickData->GetReadAvailable(), (i32)(numSeconds * ENGINE_FIXED_TICK_RATE));
	u32 earliestTick = circular->tickData->PeekSingle(circular->tickData->GetReadAvailable() - numTickData)->serverTick;
	for (i32 i = circular->tickData->GetReadAvailable() - numTickData; i < circular->tickData->GetReadAvailable(); i++)
	{
		TickData *tickData = circular->tickData->PeekSingle(i);
		if (!tickData)
		{
			break;
		}
		this->tickData.push_back(*tickData);
		this->subtickData.push_back(*circular->subtickData->PeekSingle(i));
	}
	i32 first = 0;
	bool shouldCopy = false;
	for (; first < circular->rpEvents->GetReadAvailable(); first++)
	{
		shouldCopy = true;
		RpEvent *event = circular->rpEvents->PeekSingle(first);
		if (event->serverTick >= earliestTick)
		{
			break;
		}
		if (event->type == RPEVENT_MODE_CHANGE)
		{
			earliestModeEventIndex = first;
		}
		else if (event->type == RPEVENT_STYLE_CHANGE && event->data.styleChange.clearStyles)
		{
			earliestStyleEventIndex = first;
		}
	}

	if (earliestModeEventIndex == -1)
	{
		RpEvent baseModeEvent = {};
		baseModeEvent.serverTick = 0;
		baseModeEvent.type = RPEVENT_MODE_CHANGE;
		V_strncpy(baseModeEvent.data.modeChange.name, circular->earliestMode.value().name, sizeof(baseModeEvent.data.modeChange.name));
		V_strncpy(baseModeEvent.data.modeChange.md5, circular->earliestMode.value().md5, sizeof(baseModeEvent.data.modeChange.md5));
		this->rpEvents.push_back(baseModeEvent);
	}
	else
	{
		RpEvent baseModeEvent = *circular->rpEvents->PeekSingle(earliestModeEventIndex);
		baseModeEvent.serverTick = 0;
		this->rpEvents.push_back(baseModeEvent);
	}
	if (earliestStyleEventIndex == -1)
	{
		bool firstStyle = true;
		for (auto &style : circular->earliestStyles.value_or(std::vector<RpModeStyleInfo>()))
		{
			RpEvent baseStyleEvent = {};
			baseStyleEvent.serverTick = 0;
			baseStyleEvent.type = RPEVENT_STYLE_CHANGE;
			V_strncpy(baseStyleEvent.data.styleChange.name, style.name, sizeof(baseStyleEvent.data.styleChange.name));
			V_strncpy(baseStyleEvent.data.styleChange.md5, style.md5, sizeof(baseStyleEvent.data.styleChange.md5));
			baseStyleEvent.data.styleChange.clearStyles = firstStyle;
			firstStyle = false;
			this->rpEvents.push_back(baseStyleEvent);
		}
	}
	else
	{
		RpEvent baseStyleEvent = *circular->rpEvents->PeekSingle(earliestStyleEventIndex);
		i32 eventServerTick = baseStyleEvent.serverTick;
		baseStyleEvent.serverTick = 0;
		this->rpEvents.push_back(baseStyleEvent);
		// Copy all style change events with the same server tick (they were part of the same batch)
		for (i32 i = earliestStyleEventIndex + 1; i < circular->rpEvents->GetReadAvailable(); i++)
		{
			RpEvent *event = circular->rpEvents->PeekSingle(i);
			if (!event || event->type != RPEVENT_STYLE_CHANGE || event->serverTick != eventServerTick)
			{
				break;
			}
			this->rpEvents.push_back(*event);
		}
	}
	if (shouldCopy)
	{
		for (i32 i = first; i < circular->rpEvents->GetReadAvailable(); i++)
		{
			if (!copyTimerEvents && circular->rpEvents->PeekSingle(i)->type == RPEVENT_TIMER_EVENT)
			{
				continue;
			}
			this->rpEvents.push_back(*circular->rpEvents->PeekSingle(i));
		}
	}
	shouldCopy = false;
	for (first = 0; first < circular->cmdData->GetReadAvailable(); first++)
	{
		CmdData *cmdData = circular->cmdData->PeekSingle(first);
		if (!cmdData)
		{
			break;
		}
		shouldCopy = true;
		if ((u32)cmdData->serverTick >= earliestTick)
		{
			break;
		}
	}
	if (shouldCopy)
	{
		for (i32 i = first; i < circular->cmdData->GetReadAvailable(); i++)
		{
			this->cmdData.push_back(*circular->cmdData->PeekSingle(i));
			this->cmdSubtickData.push_back(*circular->cmdSubtickData->PeekSingle(i));
		}
	}
}

bool Recorder::WriteToFile()
{
	// Update the replay timestamp before writing.
	time_t unixTime = 0;
	time(&unixTime);
	replayHeader.set_timestamp((u64)unixTime);

	std::string uuidStr = this->uuid.ToString();
	char tempFilename[512];
	char finalFilename[512];
	V_snprintf(tempFilename, sizeof(tempFilename), "%s/%s.replay.tmp", SURF_REPLAY_PATH, uuidStr.c_str());
	V_snprintf(finalFilename, sizeof(finalFilename), "%s/%s.replay", SURF_REPLAY_PATH, uuidStr.c_str());
	g_pFullFileSystem->CreateDirHierarchy(SURF_REPLAY_PATH, "GAME");
	
	// Write to temporary file first
	FileHandle_t file = g_pFullFileSystem->Open(tempFilename, "wb", "GAME");

	if (!file)
	{
		META_CONPRINTF("Failed to open replay file for writing: %s\n", tempFilename);
		return false;
	}

	// Order of writing must match order of reading in surf_replaydata.cpp
	i32 bytesWritten = 0;
	bytesWritten += this->WriteHeader(file);

	bytesWritten += Surf::replaysystem::compression::WriteTickDataCompressed(file, this->tickData, this->subtickData);

	bytesWritten += Surf::replaysystem::compression::WriteWeaponsCompressed(file, this->weaponTable);

	bytesWritten += Surf::replaysystem::compression::WriteEventsCompressed(file, this->rpEvents);

	bytesWritten += Surf::replaysystem::compression::WriteCmdDataCompressed(file, this->cmdData, this->cmdSubtickData);
	
	// Close the file before renaming
	g_pFullFileSystem->Close(file);

	// Rename temp file to final name
	if (!g_pFullFileSystem->RenameFile(tempFilename, finalFilename, "GAME"))
	{
		META_CONPRINTF("Failed to rename replay file from %s to %s\n", tempFilename, finalFilename);
		g_pFullFileSystem->RemoveFile(tempFilename, "GAME");
		return false;
	}
	
	if (surf_replay_recording_debug.Get())
	{
		META_CONPRINTF("surf_replay_recording_debug: Saved replay to %s (%d bytes)\n", finalFilename, bytesWritten);
	}
	return true;
}

i32 Recorder::WriteHeader(FileHandle_t file)
{
	// Serialize unified protobuf header with length prefix
	std::string serialized;
	if (!this->replayHeader.SerializeToString(&serialized))
	{
		META_CONPRINTF("[Surf] Failed to serialize replay header protobuf\n");
		return 0;
	}
	u32 size = static_cast<u32>(serialized.size());
	i32 written = 0;
	written += g_pFullFileSystem->Write(&size, sizeof(size), file);
	written += g_pFullFileSystem->Write(serialized.data(), size, file);
	return written;
}

i32 Recorder::WriteTickData(FileHandle_t file)
{
	return Surf::replaysystem::compression::WriteTickDataCompressed(file, this->tickData, this->subtickData);
}

i32 Recorder::WriteWeapons(FileHandle_t file)
{
	return Surf::replaysystem::compression::WriteWeaponsCompressed(file, this->weaponTable);
}

i32 Recorder::WriteEvents(FileHandle_t file)
{
	return Surf::replaysystem::compression::WriteEventsCompressed(file, this->rpEvents);
}

i32 Recorder::WriteCmdData(FileHandle_t file)
{
	return Surf::replaysystem::compression::WriteCmdDataCompressed(file, this->cmdData, this->cmdSubtickData);
}
