#ifndef FS_SYSTEM_H
#define FS_SYSTEM_H

#include <stdio.h>

#include "FS_Exception.h"
#include "FS_Filesystem.h"
#include "FS_Console.h"
#include "FS_Logging.h"
#include "FS_Kernel.h"

#include <stdint.h>

#include "FS_DT_USART.h"

typedef struct
{
  volatile uint64_t timeMicroseconds;
  FS_Exception_t * exc;
  FS_Filesystem_t * fs;
  FS_Console_t * console; // Replace this pointer if a remote session arrives.
  FS_Logging_t * log;
  FS_KernelAPI_t * kernel;

}FS_GenericModuleSystemBinding_t;

typedef struct
{
  FS_DT_USARTDriver_t * usart;


}FS_System_InitStruct_t;


void FS_System_InitStructInit(FS_System_InitStruct_t * initStruct);
_Bool FS_System_Init(FS_System_InitStruct_t * initStruct);

#endif // FS_SYSTEM_H
