#ifndef __UTILS_H
#define __UTILS_H

/* Includes ------------------------------------------------------------------*/


/* Exported types ------------------------------------------------------------*/
typedef void(*PeriodicCalledFunction_type)(void);

/* Exported constants --------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/


/* Exported functions ------------------------------------------------------- */

void TickTock_Init(void);
void TickTock_Start(void);
void TickTock_Start_OneShot(void);
void TickTock_Stop(void);
void PeriodicCaller_Init(void);
void PeriodicCaller_Start(PeriodicCalledFunction_type PeriodicCalledFunction);

#endif /* __UTILS_H */
