/**
 *******************************************************************************
 *
 * @file  FS_Console.h
 *
 * @brief Generic IO console - header file.
 *
 * @author Andy Norris [andy@firmwaresavvy.com]
 *
 *******************************************************************************
 */

// Preprocessor guard.
#ifndef FS_CONSOLE_H
#define FS_CONSOLE_H

#include "FS_DT_Conf.h"

#include "FS_Console_Conf.h"

#define FS_CONSOLE_VT100_CLEAR_SCREEN  "\033[2J\f"

/*------------------------------------------------------------------------------
---------------------- START PUBLIC TYPE DEFINITIONS ---------------------------
------------------------------------------------------------------------------*/

typedef struct
{
  char buffer[FS_CONSOLE_INPUT_BUFFER_LENGTH_BYTES];
  uint16_t ptr;

}FS_Console_Input_t;

typedef struct
{
  int(*printf)(const char * fmt, ...);
  _Bool(*registerCommand)( const char * cmd,
                           void(*callback)( const char * argv, FS_DT_IOStream_t * io ) );

}FS_Console_t;


typedef struct
{
  // Instance to which this module will be bound.
  FS_Console_t * instance;

  /*
  Default IO stream. Modules such as TelNet or SSH servers can also register/
  deregister their FS_DT_IOStream_t interfaces via callbacks as necessary.
  */
  FS_DT_IOStream_t * io;

  _Bool echo;

  /*
  If set, (and echo also set) output will be copied to all stored IO streams. This might
  mean that although input is being received via an SSH session, output
  is still echoed to the local debug UART in addition to the remote client.
  */
  _Bool echoToAllOutputStreams;

}FS_Console_InitStruct_t;


typedef struct
{
  _Bool success;

  void(*mainLoop)(void * params);

  // Called for example when a TelNet/SSH session starts.
  void(*addIOStreamCallback)(FS_DT_IOStream_t * newIO);

  // Called for example when a TelNet/SSH session ends.
  void(*removeIOStreamCallback)(FS_DT_IOStream_t * oldIO);

}FS_Console_InitReturnsStruct_t;

typedef struct
{
  FS_Console_Input_t * input;
  _Bool(*inputLineAvailable)(void);
  void(*output)(const char * buf, uint8_t numBytes);

}FS_Console_CommandCallbackInterface_t;

/*------------------------------------------------------------------------------
----------------------- END PUBLIC TYPE DEFINITIONS ----------------------------
------------------------------------------------------------------------------*/


/*------------------------------------------------------------------------------
-------------------- START PUBLIC FUNCTION PROTOTYPES --------------------------
------------------------------------------------------------------------------*/

void FS_Console_InitStructInit(FS_Console_InitStruct_t * initStruct);
void FS_Console_InitReturnsStructInit(FS_Console_InitReturnsStruct_t * returnsStruct);
void FS_Console_Init( FS_Console_InitStruct_t * initStruct,
                      FS_Console_InitReturnsStruct_t * returns );

/*------------------------------------------------------------------------------
-------------------- START PUBLIC FUNCTION PROTOTYPES --------------------------
------------------------------------------------------------------------------*/
#endif // FS_CONSOLE_H
