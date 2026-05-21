#ifndef ANA_CORE_H
#define ANA_CORE_H

#include "ana_result.h"
#include "ana_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ANA_RunStats {
    long frames;
    long elapsed_ticks;
    long ticks_per_second;
    long average_fps_x100;
} ANA_RunStats;

int ana_run(const ANA_Game* game);
void ana_quit(void);
ANA_RunStats ana_last_run_stats(void);

#ifdef __cplusplus
}
#endif

#endif
