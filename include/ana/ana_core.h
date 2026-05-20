#ifndef ANA_CORE_H
#define ANA_CORE_H

#include "ana_result.h"
#include "ana_types.h"

#ifdef __cplusplus
extern "C" {
#endif

int ana_run(const ANA_Game* game);
void ana_quit(void);

#ifdef __cplusplus
}
#endif

#endif

