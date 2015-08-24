/**
 *******************************************************************************
 *
 * @file  FS_Console.c
 *
 * @brief Generic IO console for use with FreeRTOS.
 *
 * @author Andy Norris [andy@firmwaresavvy.com]
 *
 *******************************************************************************
 */

/*------------------------------------------------------------------------------
------------------------------ START INCLUDES ----------------------------------
------------------------------------------------------------------------------*/

// Own header.
#include "FS_Console.h"

// Configuration header - project must supply this.
#include "FS_Console_Conf.h"

// FirmwareSavvy library includes.
#include "FS_DT_Conf.h"

// C standard library includes.
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>

// FreeRTOS includes.
#include "FreeRTOS.h"
#include "semphr.h"

/*------------------------------------------------------------------------------
------------------------------- END INCLUDES -----------------------------------
------------------------------------------------------------------------------*/


/*------------------------------------------------------------------------------
--------------------- START PRIVATE TYPE DEFINITIONS ---------------------------
------------------------------------------------------------------------------*/

typedef struct
{
  FS_DT_IOStream_t * interfaces[FS_CONSOLE_MAX_NUM_STORED_IO_STREAMS];
  uint8_t defaultInterfaceIndex;
  SemaphoreHandle_t mutex;

}IOStreams_t;

typedef struct
{
  const char * cmd;
  void(*callback)(const char * argv, FS_DT_IOStream_t * io);

}Command_t;

typedef struct
{
  char buffer[FS_CONSOLE_INPUT_BUFFER_LENGTH_BYTES];
  uint16_t ptr;

}Input_t;

/*------------------------------------------------------------------------------
---------------------- END PRIVATE TYPE DEFINITIONS ----------------------------
------------------------------------------------------------------------------*/


/*------------------------------------------------------------------------------
------------------- START PRIVATE FUNCTION PROTOTYPES --------------------------
------------------------------------------------------------------------------*/

static _Bool registerCommand( const char * cmd,
                             void(*callback)( const char * argv,
                                               FS_DT_IOStream_t * io ) );
static int consolePrintf(const char * fmt, ...);
static void mainLoop(void * params);
static void output(const char * buf, uint8_t numBytes);
static void executeCommand(void);
static void doBufferOverwhelmedActions(void);
static void doBadCommandActions(void);
static void addIOStreamCallback(FS_DT_IOStream_t * newIO);
static void removeIOStreamCallback(FS_DT_IOStream_t * oldIO);

/*------------------------------------------------------------------------------
-------------------- END PRIVATE FUNCTION PROTOTYPES ---------------------------
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
--------------------- START PRIVATE GLOBAL VARIABLES ---------------------------
------------------------------------------------------------------------------*/

static FS_Console_t * instance;
static IOStreams_t io;
static Command_t commandTable[FS_CONSOLE_MAX_NUM_COMMANDS];
static uint16_t numRegisteredCommands;
static Input_t input;
static _Bool echo;
static _Bool echoToAllOutputStreams;

/*------------------------------------------------------------------------------
---------------------- END PRIVATE GLOBAL VARIABLES ----------------------------
------------------------------------------------------------------------------*/


/*------------------------------------------------------------------------------
------------------------ START PUBLIC FUNCTIONS --------------------------------
------------------------------------------------------------------------------*/

void FS_Console_InitStructInit(FS_Console_InitStruct_t * initStruct)
{
  initStruct->echo = false;
  initStruct->echoToAllOutputStreams = false;
  initStruct->instance = NULL;
  initStruct->io = NULL;
}

void FS_Console_InitReturnsStructInit(FS_Console_InitReturnsStruct_t * returnsStruct)
{
  returnsStruct->addIOStreamCallback = NULL;
  returnsStruct->removeIOStreamCallback = NULL;
  returnsStruct->mainLoop = NULL;
}


void FS_Console_Init( FS_Console_InitStruct_t * initStruct,
                      FS_Console_InitReturnsStruct_t * returns )
{
  // Create a mutex to protect the list of IO streams.
  io.mutex = xSemaphoreCreateMutex();

  // Transfer the pertinent fields from the init struct.
  echo = initStruct->echo;
  echoToAllOutputStreams = initStruct->echoToAllOutputStreams;
  instance = initStruct->instance;

  // Copy in the default IO stream interface.
  io.interfaces[0] = initStruct->io;
  io.defaultInterfaceIndex = 0;

  // Bind the instance to the implementation.
  instance->printf = consolePrintf;
  instance->registerCommand = registerCommand;

  // Populate the returns struct.
  returns->addIOStreamCallback = addIOStreamCallback;
  returns->removeIOStreamCallback = removeIOStreamCallback;
  returns->mainLoop = mainLoop;
  returns->success = true;
}

/*------------------------------------------------------------------------------
------------------------- END PUBLIC FUNCTIONS ---------------------------------
------------------------------------------------------------------------------*/


/*------------------------------------------------------------------------------
----------------------- START PRIVATE FUNCTIONS --------------------------------
------------------------------------------------------------------------------*/

static _Bool registerCommand( const char * cmd, void(*callback)( const char * argv,
                                                                  FS_DT_IOStream_t * io ) )
{
  if(numRegisteredCommands < FS_CONSOLE_MAX_NUM_COMMANDS)
  {
    commandTable[numRegisteredCommands].callback = callback;
    commandTable[numRegisteredCommands].cmd = cmd;
    return true;
  }

  else
  {
    return false;
  }
}

static int consolePrintf(const char * fmt, ...)
{
  va_list arg;
  int bytes;
  char outputBuffer[FS_CONSOLE_OUTPUT_BUFFER_LENGTH_BYTES];
  uint16_t length;

  va_start(arg, fmt);
  bytes = vsnprintf(outputBuffer, FS_CONSOLE_OUTPUT_BUFFER_LENGTH_BYTES, fmt, arg);
  va_end(arg);

  length = strlen(outputBuffer);

  output(outputBuffer, length);

  return bytes;
}

static void mainLoop(void * params)
{
  char tempByte;

  // Print the splash screen before going into the processing loop.
  io.interfaces[io.defaultInterfaceIndex]->writeBytes( FS_CONSOLE_SPLASH_SCREEN,
                                                       strlen(FS_CONSOLE_SPLASH_SCREEN) );

  while(true)
  {
    /*
    Another task may be attempting to modify the IO stream list so we must get the mutex
    before doing anything with the streams.
    */
    if( xSemaphoreTake( io.mutex, FS_CONSOLE_IOSTREAM_MUTEX_TIMEOUT_TICKS) )
    {
      // Get a byte from the default IO stream if one is available.
      if( io.interfaces[io.defaultInterfaceIndex]->readBytes( &tempByte, 1 ) )
      {
        if(echo)
        {
          output(&tempByte, 1);
        }

        input.buffer[input.ptr] = tempByte;

        // If we've not encountered a line ending...
        if(FS_CONSOLE_LINE_ENDING != tempByte)
        {
          if( ( FS_CONSOLE_INPUT_BUFFER_LENGTH_BYTES - 1 ) == input.ptr )
          {
            doBufferOverwhelmedActions();
          }

          else
          {
            input.ptr++;
          }
        }

          // If we *have* encountered a line ending, execute the command.
        else
        {
          executeCommand();

          /*
          After the command actions have completed, output the prompt character
          ready for the next line.
          */
          output(FS_CONSOLE_PROMPT_CHARACTER, 1);
        }
      }
    }
  }
}


static void output(const char * buf, uint8_t numBytes)
{
  uint8_t i;

  io.interfaces[io.defaultInterfaceIndex]->writeBytes(buf, numBytes);

  if(echoToAllOutputStreams)
  {
    // Loop over all available output streams except the default and echo the output to each.
    for(i = 0; i < FS_CONSOLE_MAX_NUM_STORED_IO_STREAMS; i++)
    {
      if(i != io.defaultInterfaceIndex)
      {
        if(io.interfaces[i])
        {
          io.interfaces[i]->writeBytes(buf, numBytes);
        }
      }
    }
  }
}

static void executeCommand(void)
{
  uint16_t i, j;
  _Bool commandExtracted;
  void(*callback)(const char * argv, FS_DT_IOStream_t * io);

  /*
  Extract the first token of the input string. We don't use strtok() or strtok_r() here as, from
  my interpretation of the documentation, if the string to tokenise is not in
  dynamically allocated memory, neither function is thread-safe. Perhaps revisit
  this later.
  */
  commandExtracted = false;

  for(i = 0; i <= input.ptr; i++)
  {
    if( ' ' == input.buffer[i] )
    {
      // Replace the space character with a line ending.
      input.buffer[i] = FS_CONSOLE_LINE_ENDING;
      commandExtracted = true;
      break;
    }
  }

  /*
  If we didn't encounter a space character, the entire input buffer is filled with a single token
  and there are no arguments.
  */
  if(!commandExtracted)
  {

  }

  // Iterate over the command table looking for the first token.

  callback = NULL;

  for(j = 0; j < numRegisteredCommands; j++)
  {
    if( !strcmp(commandTable[j].cmd, input.buffer) )
    {
      callback = commandTable[j].callback;
      break;
    }
  }


  if(callback)
  {
    /*
    Call the implementation function, passing everything after the first
    space we found as the argument string. Also supply a pointer to the
    default IO stream to allow the implementation function to receive
    further input.
    */
    callback( &( input.buffer[i+1] ), io.interfaces[io.defaultInterfaceIndex] );
  }

  else
  {
    doBadCommandActions();
  }
}

static void doBufferOverwhelmedActions(void)
{

}

static void doBadCommandActions(void)
{
  output("Bad command - ", 14);
  output(input.buffer, input.ptr + 1);
}

// Callback functions.
static void addIOStreamCallback(FS_DT_IOStream_t * newIO)
{
  // Must make the new stream be used for tx and (if the 'echo to all' flag
  // is set, rx to all stored streams).

  // Output 'this stream is now output only as another console session has arrived' message to non-default streams.
}


static void removeIOStreamCallback(FS_DT_IOStream_t * oldIO)
{

}

/*------------------------------------------------------------------------------
------------------------ END PRIVATE FUNCTIONS ---------------------------------
------------------------------------------------------------------------------*/
