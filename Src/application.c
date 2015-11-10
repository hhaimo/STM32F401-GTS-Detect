/**
  ******************************************************************************
  * @file    application.c 
  * @author  Hernan Haimovich & Andres Vazquez
  * @version V0.0.1
  * @date    09/11/2015
  * @brief   Archivo de aplicación para detección de señal GTS.
  ******************************************************************************
  * @attention
  *
  * Redistribution and use in source and binary forms, with or without
  * modification, are permitted provided that the following conditions are met:
  *
  * 1. Redistributions of source code must retain the above copyright notice,
  *    this list of conditions and the following disclaimer.
  *
  * 2. Redistributions in binary form must reproduce the above copyright notice,
  *    this list of conditions and the following disclaimer in the documentation
  *    and/or other materials provided with the distribution.
  *
  * 3. Neither the name of the copyright holder nor the names of its
  *    contributors may be used to endorse or promote products derived from this
  *    software without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
  * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  * POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */ 

/* Includes ------------------------------------------------------------------*/
#include "application.h"
#include "ff.h"
#include "waveplayer.h"
#include "waverecorder.h"
#include "ff.h"    
#include "ff_gen_drv.h"
#include "usbh_diskio.h"
#include "main.h"
#include "utils.h"
#include "audioFilter.h"

/* Private typedef -----------------------------------------------------------*/
typedef enum
{
  APPSTATE_IDLE = 0,
  APPSTATE_MOUNT_FS,
  APPSTATE_UMOUNT_FS,
  APPSTATE_PLAY,
	APPSTATE_READ,
}appState_enum;

/* Private define ------------------------------------------------------------*/

// Tiempos en mili segundos
#define TIEMPO_TONO_CORTO 100
#define TIEMPO_TONO_LARGO 500
#define TIEMPO_ESPERA 900
// Frecuencia en kHz
#define FREQ_TONO 1
#define FREQ_MUESTREO 16
// Longitud de la ventana temporal en cantidad de ciclos del tono a detectar
//   Valores admitidos: 1, 2, 4
//   Si se cambia esta longitud, debe cambiarse UMBRAL (en audiofilter.c)
#define CANT_CICLOS_VENTANA 4
// Este código sólo sirve cuando CANT_MUESTRAS_CICLO es 16
#define CANT_MUESTRAS_CICLO (FREQ_MUESTREO / FREQ_TONO)

// Longitud de la ventana en muestras
#define LONGITUD_VENT (CANT_CICLOS_VENTANA * CANT_MUESTRAS_CICLO)
// Cantidad minima y maxima de ventanas que puede tener un tono corto, largo o espera
#define MIN_VENT_CORTO ((TIEMPO_TONO_CORTO * FREQ_TONO / CANT_CICLOS_VENTANA)-2)
#define MAX_VENT_CORTO ((TIEMPO_TONO_CORTO * FREQ_TONO / CANT_CICLOS_VENTANA)+2)
#define MIN_VENT_LARGO ((TIEMPO_TONO_LARGO * FREQ_TONO / CANT_CICLOS_VENTANA)-2)
#define MAX_VENT_LARGO ((TIEMPO_TONO_LARGO * FREQ_TONO / CANT_CICLOS_VENTANA)+2)
#define MIN_VENT_ESPACIO ((TIEMPO_ESPERA * FREQ_TONO / CANT_CICLOS_VENTANA)-2)
#define MAX_VENT_ESPACIO ((TIEMPO_ESPERA * FREQ_TONO / CANT_CICLOS_VENTANA)+2)
// Cantidad minima y maxima de ventanas que puede haber entre
//    la finalizacion de un tono corto y la finalizacion del siguiente
#define MIN_VENT_SEC (((TIEMPO_ESPERA + TIEMPO_TONO_CORTO) * FREQ_TONO / CANT_CICLOS_VENTANA)-2)
#define MAX_VENT_SEC (((TIEMPO_ESPERA + TIEMPO_TONO_CORTO) * FREQ_TONO / CANT_CICLOS_VENTANA)+2)
// Cantidad minima y maxima de ventanas que puede haber entre
//    la finalizacion de un tono corto y la finalización del tono largo
#define MIN_VENT_SEC_L (((TIEMPO_ESPERA + TIEMPO_TONO_LARGO) * FREQ_TONO / CANT_CICLOS_VENTANA)-2)
#define MAX_VENT_SEC_L (((TIEMPO_ESPERA + TIEMPO_TONO_LARGO) * FREQ_TONO / CANT_CICLOS_VENTANA)+2)

// Cantidad de ventanas que entran en un bloque de lectura de archivo
#define CANT_VENTANAS_BLOQUE (512 / LONGITUD_VENT)

/* Private variables ---------------------------------------------------------*/
static FATFS USBDISKFatFs;           /* File system object for USB disk logical drive */
static char USBDISKPath[4];          /* USB Host logical drive path */
static appState_enum appState = APPSTATE_IDLE;
static uint8_t usbConnected = 0;

/* Variable used by FatFs*/
static FIL FileRead;
//static FIL FileWrite;



/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/
/**
  * @brief  Función llamada cada 1 segundo por el timer TIM2
  * @param  None
  * @retval None
  */
void FuegosArtificiales(void)
{
	static int i=0;
	
	i++;
	if (i==1){
		TickTock_Start();
		BSP_LED_On(LED5);
	}else if (i==2){
		TickTock_Stop();
		BSP_LED_Off(LED5);
	}else if (i==60){
		i=0;
	}
}

/**
  * @brief  WavePlayer CallBack. Reproduce y detecta señal GTS
	* @param  *pBuff: Audio buffer
  * @param  length: Buffer length
  * @retval Bytes read
  */
int32_t getDataCB(int16_t *pBuff, int32_t length)
{	
  UINT bytesread = 0;
	unsigned int i;
	typedef enum
	{	ESPERA_TONO = 0,
		CUENTA_TONO_CORTO,
		CUENTA_ESPACIO,
		CUENTA_TONO_LARGO,
		SECUENCIA_DETECTADA,
		SEC_DETECTADA_IDLE
	} lista_estados;
	static lista_estados estado=ESPERA_TONO;
	static int cant_secuencias=0;
	static int cuenta_vent=0;
	static int cuenta_tiempo=0;
  uint8_t buf_vent;
	int16_t *pBuff_vent16= pBuff;
	
  f_read(&FileRead, pBuff, length*sizeof(int16_t), &bytesread); 
  
	for (i=0 ; i<CANT_VENTANAS_BLOQUE ; i++)
	{
		buf_vent = audioFilter_ToneDetect(pBuff_vent16, LONGITUD_VENT);
		//buf_vent = 0;
		pBuff_vent16 += LONGITUD_VENT;
  
		cuenta_tiempo++;
		switch (estado)
		{
			case ESPERA_TONO:
				cuenta_tiempo=0;
				BSP_LED_Off(LED5);
				//BSP_LED_Off(LED5);
				if (buf_vent==1)
				{
					cuenta_vent++;
					estado = CUENTA_TONO_CORTO;
				}
				break;
			
			case CUENTA_TONO_CORTO:
				if (buf_vent==1)
				{
					cuenta_vent++;
					// ENCENDER LED
					BSP_LED_On(LED5);
				}
				else
				{
					if (cuenta_vent>=MIN_VENT_CORTO && cuenta_vent<=MAX_VENT_CORTO)
					{
						if (cant_secuencias == 0 || (cuenta_tiempo >= MIN_VENT_SEC && cuenta_tiempo <= MAX_VENT_SEC)){
							estado = CUENTA_ESPACIO;
							cuenta_vent=1;
							cuenta_tiempo = 1;
						} else {
							estado = ESPERA_TONO;
							cuenta_vent = 0;
							cant_secuencias = 0;
						}
					}
					else
					{
						estado = ESPERA_TONO;
						cuenta_vent=0;
					}
				}
				break;
			
			case CUENTA_ESPACIO:
				BSP_LED_Off(LED5);
				cuenta_vent++;
				if (cuenta_vent >= MIN_VENT_ESPACIO)
				{
					if (buf_vent==1)
					{
						// Secuencia de tono corto y espera detectada
						cant_secuencias++;
						if (cant_secuencias == 5)
						{
							cant_secuencias = 0;
							estado = CUENTA_TONO_LARGO;
						}
						else
						{
							estado = CUENTA_TONO_CORTO;
						}
						cuenta_vent = 1;
					}
					else if (cuenta_vent > MAX_VENT_ESPACIO)
					{
						// Espera demasiado larga
						estado = ESPERA_TONO;
						cuenta_vent=0;
						cant_secuencias = 0;
					}
				}
				break;
			
			case CUENTA_TONO_LARGO:
				BSP_LED_On(LED5);
				if (buf_vent==1)
				{
					cuenta_vent++;
				}
				else
				{
					if (cuenta_vent>=MIN_VENT_LARGO && cuenta_vent<=MAX_VENT_LARGO && cuenta_tiempo >= MIN_VENT_SEC_L && cuenta_tiempo <= MAX_VENT_SEC_L)
					{
						estado = SECUENCIA_DETECTADA;
						cuenta_vent=0;
					}
					else
					{
						estado = ESPERA_TONO;
						cuenta_vent=0;
						cant_secuencias = 0;
					}
				}				
				break;
			
			case SECUENCIA_DETECTADA:
				// Fuegos artificiales
				BSP_LED_On(LED4);
				BSP_LED_Off(LED5);
				estado = SEC_DETECTADA_IDLE;
				cant_secuencias = 0;
				PeriodicCaller_Start(FuegosArtificiales);
				break;
			
			case SEC_DETECTADA_IDLE:
				break;
			
		}
	}	
	return bytesread;
	}



/* Exported functions ------------------------------------------------------- */

extern void application_init(void)
{
  /*##-1- Link the USB Host disk I/O driver ##################################*/
  if(FATFS_LinkDriver(&USBH_Driver, USBDISKPath) != 0)
  {
    Error_Handler();
  }
  
  TickTock_Init();
	
	PeriodicCaller_Init();
  
  audioFilter_init();
}

extern void application_task(void)
{
  UINT bytesread = 0;
  WAVE_FormatTypeDef waveformat;
  
  switch (appState)
  {
    case APPSTATE_IDLE:
      if (usbConnected)
      {
        appState = APPSTATE_MOUNT_FS;
      }
      break;
    
    case APPSTATE_MOUNT_FS:
      if (f_mount(&USBDISKFatFs, (TCHAR const*)USBDISKPath, 0 ) != FR_OK ) 
      {
        /* FatFs initialization fails */
        Error_Handler();
      }
      else
      {
        appState = APPSTATE_PLAY;
      }
      break;
    
    case APPSTATE_UMOUNT_FS:
      f_mount(NULL, (TCHAR const*)"", 1);
      appState = APPSTATE_IDLE;
      break;
    
    case APPSTATE_PLAY:
			BSP_LED_Off(LED4);
      if (f_open(&FileRead, WAVE_NAME_COMPLETO, FA_READ) != FR_OK)
      {
        Error_Handler();
      }
      else
      {
        /* Read sizeof(WaveFormat) from the selected file */
        f_read (&FileRead, &waveformat, sizeof(waveformat), &bytesread);
        WavePlayerStart(waveformat, getDataCB, 70);
        f_close (&FileRead);
				usbConnected = 0; // Hace de cuenta que se desconecta el Pendrive
													// Se puede comenzar nuevamente desconectandolo y volviendo a conectarlo
				appState = APPSTATE_IDLE;
      }
      break;
    
    default:
      appState = APPSTATE_IDLE;
      break;
  }
}

extern void application_conect(void)
{
  usbConnected = 1;
}
extern void application_disconect(void)
{
  usbConnected = 0;
}

/* End of file ---------------------------------------------------------------*/

