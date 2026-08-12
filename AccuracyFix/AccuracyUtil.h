#pragma once

class CAccuracyUtil
{
public:
	cvar_t* CvarRegister(const char* Name, const char* Value);
	void ServerCommand(const char* Format, ...);
	const char* GetPath();
	TraceResult GetUserAiming(edict_t* pEntity, float DistanceLimit);

	// ReHLDS keeps the cvar_t pointer and the cvar name passed to
	// pfnCVarRegister. Both therefore have to remain valid for the entire
	// lifetime of the module.
	static const int MAX_REGISTERED_CVARS = 4 + ((MAX_WEAPONS + 1) * 2);
	static const int MAX_CVAR_NAME_LENGTH = 64;

	cvar_t m_CvarData[MAX_REGISTERED_CVARS] = {};
	char m_CvarNames[MAX_REGISTERED_CVARS][MAX_CVAR_NAME_LENGTH] = {};
	int m_CvarCount = 0;

	std::string m_Path;
};

extern CAccuracyUtil gAccuracyUtil;
