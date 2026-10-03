#ifndef RANDO_TRAP_H
#define RANDO_TRAP_H

#include "Rando/Rando.h"
typedef enum {
    TRAP_FREEZE,
    TRAP_BLAST,
    TRAP_SHOCK,
    TRAP_JINX,
    TRAP_WALLET,
    TRAP_ENEMY,
    TRAP_TIME,
    TRAP_FIRE,
    TRAP_KNOCKBACK,
    TRAP_MAX
} TrapTypes;

using MMVR_TrapAction = void (*)();
// Exact-state adapters accept only these known callbacks, never arbitrary closures.
extern "C" MMVR_TrapAction MMVR_RandoTrapAction(int id);
extern "C" int MMVR_RandoTrapActionId(MMVR_TrapAction action);
extern "C" const char* MMVR_RandoTrapActionName(MMVR_TrapAction action);
extern "C" MMVR_TrapAction MMVR_RandoTrapActionByName(const char* name);

extern int RollTrapType();
extern std::string GetTrapMessage();

#endif