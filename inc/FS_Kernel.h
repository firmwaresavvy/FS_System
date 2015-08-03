#ifndef FS_KERNEL_H
#define FS_KERNEL_H

typedef struct
{
  // Scheduler control.
  void(*startScheduler)(void);

  void(*enterCritical)(void);
  void(*exitCritical)(void);

}FS_KernelAPI_t;

#endif // FS_KERNEL_H
