#ifndef VOIDSTRIKE_GAME_H
#define VOIDSTRIKE_GAME_H
#include "ana.h"
typedef enum VoidstrikeState { VOIDSTRIKE_TITLE, VOIDSTRIKE_PLAYING, VOIDSTRIKE_RESPAWN, VOIDSTRIKE_GAME_OVER, VOIDSTRIKE_VICTORY } VoidstrikeState;
typedef struct VoidstrikeTelemetry { int frame,score,lives,selected_module; unsigned int installed_modules; int enemies_spawned,enemies_destroyed,boss_phase; VoidstrikeState state; } VoidstrikeTelemetry;
void voidstrike_init(void); void voidstrike_load(void); void voidstrike_update(ANA_Time time); void voidstrike_draw(void); void voidstrike_shutdown(void); VoidstrikeTelemetry voidstrike_telemetry(void);
#endif
