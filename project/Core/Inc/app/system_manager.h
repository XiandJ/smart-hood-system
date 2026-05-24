#ifndef __SYSTEM_MANAGER_H
#define __SYSTEM_MANAGER_H

#include "system_state.h"
#include "config.h"

system_state_t SystemManager_GetState(void);
void           SystemManager_SetAIMotorLevel(uint8_t level);
void           SystemManager_Update(void);

#endif /* __SYSTEM_MANAGER_H */
