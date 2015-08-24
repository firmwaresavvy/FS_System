// Own header.
#include "FS_System.h"

// System components.
#include "FS_Console.h"

// C standard library includes.
#include <stdbool.h>

// FreeRTOS includes.
#include "FreeRTOS.h"
#include "task.h"

// Project must provide this file for all system subcomponent task priority defines.
#include "FS_TaskPriorities_Conf.h"

/*
{
  FS_SystemTime_t * time;
  FS_Exception_t * exc;
  FS_Filesystem_t * fs;
  FS_Console_t * console; // Replace this pointer if a remote session arrives.
  FS_Logging_t * log;

}FS_GenericModuleSystemBinding_t;
*/

static void initConsole(FS_Console_InitReturnsStruct_t * returns, FS_DT_IOStream_t * debugUART);

static FS_GenericModuleSystemBinding_t * sysInstance;

static uint16_t timerIntervalMicroseconds;
static _Bool moduleInitialised = false;

void FS_System_InitStructInit(FS_System_InitStruct_t * initStruct)
{
  initStruct->timerIntervalMicroseconds = 0xFFFF;
  initStruct->sysInstance = NULL;
  initStruct->usart = NULL;
}

_Bool FS_System_Init(FS_System_InitStruct_t * initStruct)
{
  FS_Console_InitReturnsStruct_t consoleReturns;
  TaskHandle_t taskHandle;

  // Get a reference to the instance of the binding struct.
  sysInstance = initStruct->sysInstance;

  // Init the global timer.
  sysInstance->timeMicroseconds = 0;


  timerIntervalMicroseconds = initStruct->timerIntervalMicroseconds;
  // initException()();
  // initFilesystem();
  // initConsole();
  // initLogging();

  moduleInitialised = true;

  // Init the debug console module.
  initConsole(&consoleReturns, initStruct->usart);

  // Start the debug console task.
  if(consoleReturns.success)
  {
    xTaskCreate( consoleReturns.mainLoop,
                 "FS_Console",
                 FS_CONSOLE_STACK_DEPTH,
                 NULL,
                 FS_CONSOLE_TASK_PRIORITY,
                 &taskHandle );

    configASSERT(taskHandle);
  }
}

static void initConsole(FS_Console_InitReturnsStruct_t * returns, FS_DT_IOStream_t * debugUART)
{
  FS_Console_InitStruct_t initStruct;


  // Initialise the data structures.
  FS_Console_InitStructInit(&initStruct);
  FS_Console_InitReturnsStructInit(returns);

  initStruct.echo = true;
  initStruct.echoToAllOutputStreams = true;
  initStruct.instance = &(sysInstance->console);
  initStruct.io = debugUART;

  FS_Console_Init(&initStruct, returns);
}

static void mainLoop(void * params)
{
  while(true)
  {

  }
}
