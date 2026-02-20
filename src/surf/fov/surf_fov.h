#pragma once
#include "surf/surf.h"
#include "surf/option/surf_option.h"

class SurfFOVService : public SurfBaseService
{
	using SurfBaseService::SurfBaseService;

public:
	static u32 GetMinFOV()
	{
		return SurfOptionService::GetOptionInt("minFOV", 80);
	}

	static u32 GetMaxFOV()
	{
		return SurfOptionService::GetOptionInt("maxFOV", 130);
	}

	static u32 GetDefaultFOV()
	{
		return SurfOptionService::GetOptionInt("defaultFOV", 90);
	}

	void SetFOV(u32 newFOV)
	{
		this->player->optionService->SetPreferenceInt("fov", newFOV);
	}

	u32 GetFOV()
	{
		return this->player->optionService->GetPreferenceInt("fov", this->GetDefaultFOV());
	}

	void OnPhysicsSimulate();
};
