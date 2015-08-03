#ifndef FS_CONSOLE_H
#define FS_CONSOLE_H

typedef struct
{
  int(*printf)(const char * fmt, ...);
  _Bool(*registerCommand)( const char * cmd,
                           _Bool(*callback)( const char * argv, uint8_t argc ) );

}FS_Console_t;

#endif // FS_CONSOLE_H
