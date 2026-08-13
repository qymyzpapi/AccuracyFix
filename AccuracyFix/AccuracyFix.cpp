#include "precompiled.h"

CAccuracyFix gAccuracyFix;

void CAccuracyFix::ServerActivate()
{	
	this->m_af_accuracy_all = gAccuracyUtil.CvarRegister("af_accuracy_all", "-1.0");

	this->m_af_distance_all = gAccuracyUtil.CvarRegister("af_distance_all", "-1.0");

	this->m_af_jump_fix = gAccuracyUtil.CvarRegister("af_jump_fix", "0.0");

	this->m_af_max_speed = gAccuracyUtil.CvarRegister("af_max_speed", "10.0");

	if (g_ReGameApi)
	{
		char cvarName[32] = { 0 };

		for (int WeaponID = WEAPON_P228; WeaponID <= WEAPON_P90; WeaponID++)
		{
			auto SlotInfo = g_ReGameApi->GetWeaponSlot((WeaponIdType)WeaponID);

			if (SlotInfo)
			{
				if ((SlotInfo->slot == PRIMARY_WEAPON_SLOT) || (SlotInfo->slot == PISTOL_SLOT))
				{
					if (SlotInfo->weaponName)
					{
						if (SlotInfo->weaponName[0u] != '\0')
						{
							Q_snprintf(cvarName, sizeof(cvarName), "af_distance_%s", SlotInfo->weaponName);

							this->m_af_distance[WeaponID] = gAccuracyUtil.CvarRegister(cvarName, "8192.0");

							Q_snprintf(cvarName, sizeof(cvarName), "af_accuracy_%s", SlotInfo->weaponName);

							this->m_af_accuracy[WeaponID] = gAccuracyUtil.CvarRegister(cvarName, "9999.0");
						}
					}
				}
			}
		}
	}

	auto Path = gAccuracyUtil.GetPath();

	if (Path)
	{
		if (Path[0u] != '\0')
		{
			gAccuracyUtil.ServerCommand("exec %s/accuracyfix.cfg", Path);
		}
	}
}

void CAccuracyFix::TraceLine(const float* vStart, const float* vEnd, int fNoMonsters, edict_t* pentToSkip, TraceResult* ptr)
{
	const auto TraceFlags = gpGlobals->trace_flags;

	if (fNoMonsters != dont_ignore_monsters)
	{
		return;
	}

	const float dx = vEnd[0] - vStart[0];
	const float dy = vEnd[1] - vStart[1];
	const float dz = vEnd[2] - vStart[2];
	const float sqDistance = (dx * dx) + (dy * dy) + (dz * dz);

	if (sqDistance < 16000000.0f)
	{
		return;
	}

	if (FNullEnt(pentToSkip))
	{
		return;
	}

	auto EntityIndex = g_engfuncs.pfnIndexOfEdict(pentToSkip);

	if (EntityIndex <= 0 || EntityIndex > gpGlobals->maxClients)
	{
		return;
	}

	auto Player = UTIL_PlayerByIndexSafe(EntityIndex);

	if (!Player || !Player->IsAlive())
	{
		return;
	}

	auto ActiveItem = Player->m_pActiveItem;

	if (!ActiveItem)
	{
		return;
	}

	if ((ActiveItem->iItemSlot() != PRIMARY_WEAPON_SLOT) &&
		(ActiveItem->iItemSlot() != PISTOL_SLOT))
	{
		return;
	}

	const auto WeaponID = ActiveItem->m_iId;

	if (WeaponID <= WEAPON_NONE || WeaponID > MAX_WEAPONS)
	{
		return;
	}

	auto DistanceCvar = this->m_af_distance[WeaponID];
	auto AccuracyCvar = this->m_af_accuracy[WeaponID];

	if (!DistanceCvar || !AccuracyCvar || !this->m_af_distance_all ||
		!this->m_af_accuracy_all || !this->m_af_jump_fix || !this->m_af_max_speed)
	{
		return;
	}

	auto DistanceLimit = DistanceCvar->value;

	if (this->m_af_distance_all->value > 0.0f)
	{
		DistanceLimit = this->m_af_distance_all->value;
	}

	// A non-positive distance disables AccuracyFix for this weapon.
	if (DistanceLimit <= 0.0f)
	{
		return;
	}

	if ((this->m_af_jump_fix->value <= 0.0f) && !(Player->pev->flags & FL_ONGROUND))
	{
		return;
	}

	const auto MaxSpeed = this->m_af_max_speed->value;

	// A negative value disables the speed restriction. Squared 2D velocity is
	// used to avoid a square root in the hot TraceLine path.
	if (MaxSpeed >= 0.0f)
	{
		const auto VelocityX = Player->pev->velocity.x;
		const auto VelocityY = Player->pev->velocity.y;
		const auto Speed2DSquared = (VelocityX * VelocityX) + (VelocityY * VelocityY);

		if (Speed2DSquared > (MaxSpeed * MaxSpeed))
		{
			return;
		}
	}

	auto trResult = gAccuracyUtil.GetUserAiming(pentToSkip, DistanceLimit);

	if (FNullEnt(trResult.pHit))
	{
		return;
	}

	auto TargetIndex = ENTINDEX(trResult.pHit);

	if (TargetIndex <= 0 || TargetIndex > gpGlobals->maxClients)
	{
		return;
	}

	auto ForwardDistance = AccuracyCvar->value;

	if (this->m_af_accuracy_all->value > 0.0f)
	{
		ForwardDistance = this->m_af_accuracy_all->value;
	}

	if (ForwardDistance <= 0.0f)
	{
		return;
	}

	g_engfuncs.pfnMakeVectors(pentToSkip->v.v_angle);

	auto vEndRes = (Vector)vStart + gpGlobals->v_forward * ForwardDistance;

	g_engfuncs.pfnTraceLine(vStart, vEndRes, fNoMonsters, pentToSkip, ptr);
}
