#include "precompiled.h"

CAccuracyFix gAccuracyFix;

bool g_bIsShooting = false;
edict_t* g_pShootingPlayer = nullptr;

void CAccuracyFix::ServerActivate()
{	
	this->m_af_accuracy_all = gAccuracyUtil.CvarRegister("af_accuracy_all", "-1.0");
	this->m_af_distance_all = gAccuracyUtil.CvarRegister("af_distance_all", "-1.0");
	this->m_af_jump_fix = gAccuracyUtil.CvarRegister("af_jump_fix", "0.0");
	this->m_af_speed_limit_all = gAccuracyUtil.CvarRegister("af_speed_limit_all", "-1.0");

	if (g_ReGameApi)
	{
		char cvarName[32] = { 0 };

		for (int WeaponID = WEAPON_P228; WeaponID <= WEAPON_P90; WeaponID++)
		{
			auto SlotInfo = g_ReGameApi->GetWeaponSlot((WeaponIdType)WeaponID);

			if (SlotInfo && ((SlotInfo->slot == PRIMARY_WEAPON_SLOT) || (SlotInfo->slot == PISTOL_SLOT)))
			{
				if (SlotInfo->weaponName && SlotInfo->weaponName[0u] != '\0')
				{
					Q_snprintf(cvarName, sizeof(cvarName), "af_distance_%s", SlotInfo->weaponName);
					this->m_af_distance[WeaponID] = gAccuracyUtil.CvarRegister(cvarName, "8192.0");

					Q_snprintf(cvarName, sizeof(cvarName), "af_accuracy_%s", SlotInfo->weaponName);
					this->m_af_accuracy[WeaponID] = gAccuracyUtil.CvarRegister(cvarName, "9999.0");
				}
			}
		}
	}

	auto Path = gAccuracyUtil.GetPath();

	if (Path && Path[0u] != '\0')
	{
		gAccuracyUtil.ServerCommand("exec %s/accuracyfix.cfg", Path);
	}
}

void CAccuracyFix::TraceLine(const float* vStart, const float* vEnd, int fNoMonsters, edict_t* pentToSkip, TraceResult* ptr)
{
	if (!g_bIsShooting || pentToSkip != g_pShootingPlayer)
		return;

	if (fNoMonsters != dont_ignore_monsters || gpGlobals->trace_flags == FTRACE_FLASH)
		return;

	if (FNullEnt(pentToSkip))
		return;

	auto EntityIndex = ENTINDEX(pentToSkip);
	if (EntityIndex < 1 || EntityIndex > gpGlobals->maxClients)
		return;

	auto Player = UTIL_PlayerByIndexSafe(EntityIndex);
	if (!Player || !Player->IsAlive() || !Player->m_pActiveItem)
		return;

	float speedLimit = this->m_af_speed_limit_all->value;
	if (speedLimit > 0.0f && Player->pev->velocity.Length2D() > speedLimit)
		return;

	if (!(Player->pev->flags & FL_ONGROUND) && this->m_af_jump_fix->value <= 0.0f)
		return;

	auto itemSlot = Player->m_pActiveItem->iItemSlot();
	if (itemSlot != PRIMARY_WEAPON_SLOT && itemSlot != PISTOL_SLOT)
		return;

	int weaponId = Player->m_pActiveItem->m_iId;

	float distanceLimit = this->m_af_distance_all->value;
	if (distanceLimit <= 0.0f)
		distanceLimit = this->m_af_distance[weaponId]->value;

	if (distanceLimit <= 0.0f)
		return;

	Vector vecForward;
	g_engfuncs.pfnAngleVectors(pentToSkip->v.v_angle, vecForward, NULL, NULL);

	auto trResult = gAccuracyUtil.GetUserAiming(pentToSkip, distanceLimit, vecForward);

	if (FNullEnt(trResult.pHit))
		return;

	auto TargetIndex = ENTINDEX(trResult.pHit);
	if (TargetIndex < 1 || TargetIndex > gpGlobals->maxClients)
		return;

	float accuracyLimit = this->m_af_accuracy_all->value;
	if (accuracyLimit <= 0.0f)
		accuracyLimit = this->m_af_accuracy[weaponId]->value;

	if (accuracyLimit <= 0.0f)
		return;

	auto vEndRes = (Vector)vStart + vecForward * accuracyLimit;

	g_engfuncs.pfnTraceLine(vStart, vEndRes, fNoMonsters, pentToSkip, ptr);
}
