#ifndef FS_EXCEPTION_H
#define FS_EXCEPTION_H

#include "stdint.h"

typedef struct
{
  int16_t(*registerModule)( const char * description,
                            void(*fatalHandlerCallback)(void) );
  void(*raiseFatal)(int16_t moduleLabel, const char * fmt, ...);
  void(*raiseWarning)(int16_t moduleLabel, const char * fmt, ...);

}FS_Exception_t;

#endif // FS_EXCEPTION_H
