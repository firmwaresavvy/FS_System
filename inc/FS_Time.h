#ifndef FS_TIME_H
#define FS_TIME_H


typedef enum
{
  FS_Time_InvalidMonth  = 0
  FS_Time_January       = 1,
  FS_Time_February      = 2,
  FS_Time_March         = 3,
  FS_Time_April         = 4,
  FS_Time_May           = 5,
  FS_Time_June          = 6,
  FS_Time_July          = 7,
  FS_Time_August        = 8,
  FS_Time_September     = 9,
  FS_Time_October       = 10,
  FS_Time_November      = 11,
  FS_Time_December      = 12

}FS_Time_Month;



typedef struct
{
  volatile uint64_t us;


}FS_SystemTime_t;

#endif // FS_TIME_H
