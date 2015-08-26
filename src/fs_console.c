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
  void(*callback)(const char * argv, FS_Console_CommandCallbackInterface_t * console);
  const char * helpString;

}Command_t;



/*------------------------------------------------------------------------------
---------------------- END PRIVATE TYPE DEFINITIONS ----------------------------
------------------------------------------------------------------------------*/


/*------------------------------------------------------------------------------
------------------- START PRIVATE FUNCTION PROTOTYPES --------------------------
------------------------------------------------------------------------------*/

static _Bool registerCommand( const char * cmd,
                              void(*callback)( const char * argv,
                                               FS_DT_IOStream_t * io ),
                              const char * helpString );
static int consolePrintf(const char * fmt, ...);
static void mainLoop(void * params);
static _Bool inputLineAvailable(void);
static void output(const char * buf, uint16_t numBytes);
static void executeCommand(void);
static void doBufferOverwhelmedActions(void);
static void doBadCommandActions(void);
static void addIOStreamCallback(FS_DT_IOStream_t * newIO);
static void removeIOStreamCallback(FS_DT_IOStream_t * oldIO);
static void help(const char * argv, FS_Console_CommandCallbackInterface_t * console);

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
static FS_Console_Input_t input;
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

  // Add the built-in commands to the command table.
  registerCommand("help", help, "TEST HELP STRING");
}

/*------------------------------------------------------------------------------
------------------------- END PUBLIC FUNCTIONS ---------------------------------
------------------------------------------------------------------------------*/


/*------------------------------------------------------------------------------
----------------------- START PRIVATE FUNCTIONS --------------------------------
------------------------------------------------------------------------------*/

static _Bool registerCommand( const char * cmd,
                              void(*callback)( const char * argv,
                                               FS_DT_IOStream_t * io ),
                              const char * helpString )
{
  if(numRegisteredCommands < FS_CONSOLE_MAX_NUM_COMMANDS)
  {
    commandTable[numRegisteredCommands].callback = callback;
    commandTable[numRegisteredCommands].cmd = cmd;
    commandTable[numRegisteredCommands].helpString = helpString;
    numRegisteredCommands++;
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
  uint16_t tempInt;

  // Clear the screen.
  output( FS_CONSOLE_VT100_CLEAR_SCREEN, strlen( FS_CONSOLE_VT100_CLEAR_SCREEN ) );

  // Print the splash screen.
  output( FS_CONSOLE_SPLASH_SCREEN, strlen( FS_CONSOLE_SPLASH_SCREEN ) );

  // Print the prompt character prior to going in to the processing loop.
  output(FS_CONSOLE_PROMPT_CHARACTER, 1);

  while(true)
  {
    while(!inputLineAvailable());
    executeCommand();

    /*
    After the command actions have completed, output the prompt character
    ready for the next line.
    */
    output(FS_CONSOLE_PROMPT_CHARACTER, 1);

    // Flush the input buffer.
    input.ptr = 0;
  }
}


static _Bool inputLineAvailable(void)
{
  _Bool retVal;
  char tempByte, tempByte2;

  /*
  Another task may be attempting to modify the IO stream list so we must get the mutex
  before doing anything with the streams.
  */
  if( xSemaphoreTake( io.mutex, FS_CONSOLE_IOSTREAM_MUTEX_TIMEOUT_TICKS ) )
  {
    // Get a byte from the default IO stream if one is available.
    if( io.interfaces[io.defaultInterfaceIndex]->readBytes( &tempByte, 1 ) )
    {
      if(echo)
      {
        output(&tempByte, 1);

        if('\r' == tempByte)
        {
          // If a CR was detected, send a LF to keep the console looking correct.
          tempByte2 = 10;
          output(&tempByte2, 1);
        }

        /*
        // If a backspace was detected, decrement the input buffer pointer.
        if(8 == tempByte)
        {
          input.ptr--;
        }
        */
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

        retVal = false;
      }

      /*
      If we have encountered a line ending, replace it with a NULL terminator
      so that the command implementation function can use string operations.
      */
      else
      {
        input.buffer[input.ptr] = 0;
        retVal = true;
      }
    }

    // Give the mutex back.
    xSemaphoreGive(io.mutex);
  }

  // Couldn't get the mutex.
  else
  {
    retVal = false;
  }

  return retVal;
}

static void output(const char * buf, uint16_t numBytes)
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
  FS_Console_CommandCallbackInterface_t callbackInterface;
  void(*callback)( const char * argv,
                   FS_Console_CommandCallbackInterface_t * callbackInterface );

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
      // Replace the space character with a NULL terminator.
      input.buffer[i] = 0;
      commandExtracted = true;
      break;
    }
  }

  /*
  If we didn't encounter a space character, the entire input buffer is filled with a single token
  and there are no arguments. Append a NULL terminator in order to use string functions.
  */
  if(!commandExtracted)
  {
    // If there's no room to append a NULL terminator, error!
    if( ( FS_CONSOLE_INPUT_BUFFER_LENGTH_BYTES - 1) == input.ptr )
    {
      doBufferOverwhelmedActions();
    }

    else
    {
      input.buffer[input.ptr++] = 0;
    }
  }

  // Iterate over the command table looking for the first token:
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
    callbackInterface.inputLineAvailable = inputLineAvailable;
    callbackInterface.output = output;
    callbackInterface.input = &input;

    /*
    Call the implementation function, passing everything after the first
    space we found as the argument string. Also supply a pointer to the
    callback interface to allow the implementation function to receive
    further input.
    */
    callback( &( input.buffer[i+1] ), &callbackInterface );
  }

  else
  {
    doBadCommandActions();
  }
}

static void doBufferOverwhelmedActions(void)

{
  output( "\r\n\nWARNING - Console input buffer was overwhelmed and will be flushed!!!\r\n\n"
          FS_CONSOLE_PROMPT_CHARACTER,
          76);
  input.ptr = 0;
}

static void doBadCommandActions(void)
{
  output("Bad command - ", 14);
  output(input.buffer, input.ptr + 1);
  output("\r\n\r\n", 4);

  // Flush the contents of the input buffer.
  input.ptr = 0;
}


// Callback functions.
static void addIOStreamCallback(FS_DT_IOStream_t * newIO)
{
  // Must make the new stream be used for tx and (if the 'echo to all' flag
  // is set, echo rx to all stored streams).

  // Output 'this stream is now output only as another console session has arrived' message to non-default streams.
}


static void removeIOStreamCallback(FS_DT_IOStream_t * oldIO)
{

}

// Built in commands.
static void help(const char * argv, FS_Console_CommandCallbackInterface_t * console)
{
  int16_t i;

  // If arguments were supplied, we need to supply help for a particular command.
  if(strlen(argv))
  {

  }

  // If no arguments were supplied, output generic help text for the system.
  else
  {
    console->output("\r\nAvailable Commands: \r\n\n", 27);

    for(i = 0; i < numRegisteredCommands; i++)
    {
      console->output(commandTable[i].cmd, strlen(commandTable[i].cmd));
      console->output("\r\n", 2);
    }

    console->output("\r\n - Type a command name and hit <Enter> for further information. ", 65);
    console->output("\r\n - Type \'exit\' and hit <Enter> to quit.\r\n\n", 44);

    // Flush the console input buffer and then wait for a line.
    console->input->ptr = 0;
    while( !console->inputLineAvailable() );

    // If the input line wasn't 'exit'
    if(strcmp(console->input->buffer, "exit"))
    {
      for(i = 0; i < numRegisteredCommands; i++)
      {
        if( !strcmp( console->input->buffer, commandTable[i].cmd ) )
        {
          output( commandTable[i].helpString,
                  strlen( commandTable[i].helpString ) );
          output("\r\n\n", 3);
        }
      }
    }

    else
    {
      output("\r\n", 2);
    }
  }

  /*
  console->output("\r\nhelp command invoked - type something...\r\n\r\n", 45);



  console->output("\r\nGot an input line: ", 21);
  console->output(input.buffer, input.ptr - 1);
  console->output("\r\n", 2);
  */

}

/*------------------------------------------------------------------------------
------------------------ END PRIVATE FUNCTIONS ---------------------------------
------------------------------------------------------------------------------*/
