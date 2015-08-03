#ifndef FS_LOGGING_H
#define FS_LOGGING_H

typedef struct
{
  int(*printf)(const char * fmt, ...);

}FS_Logging_t;

#endif // FS_LOGGING_H
