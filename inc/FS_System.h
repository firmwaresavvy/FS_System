#ifndef FS_SYSTEM_H
#define FS_SYSTEM_H

#include <stdio.h>

#include "FS_Exception.h"
#include "FS_Filesystem.h"
#include "FS_Console.h"
#include "FS_Logging.h"


#include <stdint.h>

#include "FS_DT_USART.h"

typedef struct
{
  _Bool isInitialised; // If flag set, it's safe to use the system binding.
  volatile uint64_t timeMicroseconds; // Application, not FS_System, must increment this.

  FS_Exception_t * exc;
  FS_Filesystem_t * fs;
  FS_Console_t * console;
  FS_Logging_t * log;

}FS_GenericModuleSystemBinding_t;

typedef struct
{
  uint16_t timerIntervalMicroseconds;
  FS_GenericModuleSystemBinding_t * sysInstance;
  FS_DT_IOStream_t * usart;


}FS_System_InitStruct_t;


void FS_System_InitStructInit(FS_System_InitStruct_t * initStruct);
_Bool FS_System_Init(FS_System_InitStruct_t * initStruct);


#endif // FS_SYSTEM_H
