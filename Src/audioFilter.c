/**
  ******************************************************************************
  * @file    audioFilter.c 
  * @author  Hernan Haimovich & Andres Vazquez
  * @version V0.0.1
  * @date    09/11/2015
  * @brief   Filtrado de audio para detección de presencia de tono.
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
#include "audioFilter.h"
#include "arm_math.h"
#include "filters.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
#define BLOCK_SIZE   1
//#define UMBRAL 0xB000
#define UMBRAL 0x20000

/* Private variables ---------------------------------------------------------*/
static const q15_t sin_1khz_FS16khz[80] =
{
  0,12539,23170,30273,32767,30273,23170,12539,0,-12539,-23170,-30273,-32767,-30273,-23170,-12539,
	0,12539,23170,30273,32767,30273,23170,12539,0,-12539,-23170,-30273,-32767,-30273,-23170,-12539,
	0,12539,23170,30273,32767,30273,23170,12539,0,-12539,-23170,-30273,-32767,-30273,-23170,-12539,
	0,12539,23170,30273,32767,30273,23170,12539,0,-12539,-23170,-30273,-32767,-30273,-23170,-12539,
	0,12539,23170,30273,32767,30273,23170,12539,0,-12539,-23170,-30273,-32767,-30273,-23170,-12539
};

static const q15_t cos_1khz_FS16khz[80] =
{
  32767,30273,23170,12539,0,-12539,-23170,-30273,-32767,-30273,-23170,-12539,0,12539,23170,30273,
	32767,30273,23170,12539,0,-12539,-23170,-30273,-32767,-30273,-23170,-12539,0,12539,23170,30273,
	32767,30273,23170,12539,0,-12539,-23170,-30273,-32767,-30273,-23170,-12539,0,12539,23170,30273,
	32767,30273,23170,12539,0,-12539,-23170,-30273,-32767,-30273,-23170,-12539,0,12539,23170,30273,
	32767,30273,23170,12539,0,-12539,-23170,-30273,-32767,-30273,-23170,-12539,0,12539,23170,30273
};
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */
extern void audioFilter_init(void)
{
	// Dejamos en blanco por si en el futuro se necesita inicializar algo
}

extern uint8_t audioFilter_ToneDetect(q15_t *src, uint32_t length)
{
	uint8_t dest;
	q63_t energia_sin;
	q63_t energia_cos;
	q31_t energia_comp[2];
	q31_t energia;
	arm_dot_prod_q15(src, (q15_t *)sin_1khz_FS16khz, length, &energia_sin);
	arm_dot_prod_q15(src, (q15_t *)cos_1khz_FS16khz, length, &energia_cos);
	
	energia_comp[0] = (q31_t)(energia_sin >> 6);
	energia_comp[1] = (q31_t)(energia_cos >> 6);
 	arm_cmplx_mag_squared_q31(energia_comp, &energia, 1);
	if (energia > 0)
	{
		dest = 0;
	}		
	if (energia > UMBRAL)
	{
		dest = 1;
	}
	else
	{
		dest = 0;
	}
	return dest;
}




/* End of file ---------------------------------------------------------------*/
