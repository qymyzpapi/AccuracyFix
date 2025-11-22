#include "precompiled.h"

CAccuracyFix gAccuracyFix;

// Бит доступа (29-й бит). 
// Используется для безопасной проверки прав, не конфликтуя с обычными счетчиками в iuser4.
#define ACCURACY_FIX_BIT (1 << 29)

float CAccuracyFix::GetSpeed2DLimitSqr()
{
    const float val = (this->m_af_speed_limit_all ? this->m_af_speed_limit_all->value : 0.0f);
    if (val != this->m_speed2d_limit)
    {
        this->m_speed2d_limit     = val;
        this->m_speed2d_limit_sqr = (val > 0.0f) ? (val * val) : 0.0f;
    }
    return this->m_speed2d_limit_sqr;
}

void CAccuracyFix::ServerActivate()
{	
    this->m_af_accuracy_all = gAccuracyUtil.CvarRegister("af_accuracy_all", "-1.0");
    this->m_af_distance_all = gAccuracyUtil.CvarRegister("af_distance_all", "-1.0");
    this->m_af_jump_fix = gAccuracyUtil.CvarRegister("af_jump_fix", "0.0");

    // Квар: лимит 2D-скорости (юн/с) для отключения корректировки
    this->m_af_speed_limit_all = gAccuracyUtil.CvarRegister("af_speed_limit_all", "210.0");
    this->m_speed2d_limit      = this->m_af_speed_limit_all ? this->m_af_speed_limit_all->value : 0.0f;
    this->m_speed2d_limit_sqr  = (this->m_speed2d_limit > 0.0f) ? (this->m_speed2d_limit * this->m_speed2d_limit) : 0.0f;

    // Квар: режим доступа (0 = для всех, 1 = только VIP с флагом от AMXX)
    this->m_af_vip_only = gAccuracyUtil.CvarRegister("af_vip_only", "0");

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
    if ((fNoMonsters == dont_ignore_monsters) && (gpGlobals->trace_flags != FTRACE_FLASH))
    {
        if (!FNullEnt(pentToSkip))
        {
            auto EntityIndex = g_engfuncs.pfnIndexOfEdict(pentToSkip);

            if (EntityIndex > 0 && EntityIndex <= gpGlobals->maxClients)
            {
                auto Player = UTIL_PlayerByIndexSafe(EntityIndex);

                if (Player)
                {
                    if (Player->IsAlive())
                    {
                        if (Player->m_pActiveItem)
                        {
                            if ((Player->m_pActiveItem->iItemSlot() == PRIMARY_WEAPON_SLOT) || (Player->m_pActiveItem->iItemSlot() == PISTOL_SLOT))
                            {
                                // Игнорируем снайперские винтовки без зума (FOV 90)
                                if ((Player->m_pActiveItem->m_iId == WEAPON_SG550) || (Player->m_pActiveItem->m_iId == WEAPON_AWP) || (Player->m_pActiveItem->m_iId == WEAPON_G3SG1))
                                {
                                    if (Player->m_iFOV == 90)
                                    {
                                        return;
                                    }
                                }

                                // --- 1. ACCESS CHECK (Проверка прав через iuser4) ---
                                if (this->m_af_vip_only && this->m_af_vip_only->value > 0.0f)
                                {
                                    // Проверяем наличие нашего уникального бита
                                    if (!(Player->pev->iuser4 & ACCURACY_FIX_BIT))
                                    {
                                        return; // Бит не установлен -> нет прав -> стоп
                                    }
                                }
                                // --- /ACCESS CHECK ---

                                // --- 2. SPEED GATE (Проверка скорости 2D) ---
                                {
                                    const float limitSqr = this->GetSpeed2DLimitSqr();
                                    if (limitSqr > 0.0f)
                                    {
                                        const float vx  = Player->pev->velocity.x;
                                        const float vy  = Player->pev->velocity.y;
                                        // Считаем квадрат скорости, чтобы не извлекать корень (оптимизация)
                                        const float sp2 = vx * vx + vy * vy; 

                                        if (sp2 >= limitSqr)
                                        {
                                            return; // Игрок движется слишком быстро -> стоп
                                        }
                                    }
                                }
                                // --- /SPEED GATE ---

                                auto DistanceLimit = this->m_af_distance[Player->m_pActiveItem->m_iId]->value;

                                if (this->m_af_distance_all->value > 0)
                                {
                                    DistanceLimit = this->m_af_distance_all->value;
                                }

                                if (DistanceLimit > 0.0f)
                                {
                                    if ((this->m_af_jump_fix->value > 0) || (Player->pev->flags & FL_ONGROUND))
                                    {
                                        auto trResult = gAccuracyUtil.GetUserAiming(pentToSkip, DistanceLimit);
    
                                        if (!FNullEnt(trResult.pHit))
                                        {
                                            auto TargetIndex = ENTINDEX(trResult.pHit);
    
                                            if (TargetIndex > 0 && TargetIndex <= gpGlobals->maxClients)
                                            {
                                                auto fwdVelocity = this->m_af_accuracy[Player->m_pActiveItem->m_iId]->value;
    
                                                if (this->m_af_accuracy_all->value > 0.0f)
                                                {
                                                    fwdVelocity = this->m_af_accuracy_all->value;
                                                }
    
                                                g_engfuncs.pfnMakeVectors(pentToSkip->v.v_angle);
    
                                                auto vEndRes = (Vector)vStart + gpGlobals->v_forward * fwdVelocity;

                                                g_engfuncs.pfnTraceLine(vStart, vEndRes, fNoMonsters, pentToSkip, ptr);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
