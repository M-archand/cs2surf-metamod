#ifndef SURF_REPLAYCOMMANDS_H
#define SURF_REPLAYCOMMANDS_H

#include "sdk/datatypes.h"

class SurfPlayer;

namespace Surf::replaysystem::commands
{
	// Navigation functions
	void NavigateReplay(SurfPlayer *player, u32 targetTick);

	// Command handlers
	void LoadReplay(SurfPlayer *player, const char *uuid);
	void CheckReplayLoadProgress(SurfPlayer *player);
	void CancelReplayLoad(SurfPlayer *player);
	void JumpToReplayTime(SurfPlayer *player, const char *input);
	void JumpToReplayTick(SurfPlayer *player, const char *input);
	void GetReplayInfo(SurfPlayer *player);
	void ToggleReplayPause(SurfPlayer *player);
	void ListReplays(SurfPlayer *player, const char *input);
	void ToggleLegsVisibility(SurfPlayer *player);
} // namespace Surf::replaysystem::commands

#endif // SURF_REPLAYCOMMANDS_H
