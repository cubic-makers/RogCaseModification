/*********************************************************************
*                   (c) SEGGER Microcontroller GmbH                  *
*                        The Embedded Experts                        *
*                           www.segger.com                           *
**********************************************************************
*                                                                    *
*        SEGGER RTT * Real Time Transfer for embedded targets        *
*                  https://github.com/SEGGERMicro/RTT                *
*                                                                    *
**********************************************************************

---------------------------END-OF-HEADER------------------------------
Purpose : User configuration file for RTT.
          For available configuration,
          refer to SEGGER_RTT_ConfDefaults.h.

----------------------------------------------------------------------
*/

#ifndef SEGGER_RTT_CONF_H
#define SEGGER_RTT_CONF_H
#define SEGGER_RTT_MAX_NUM_UP_BUFFERS   1
#define SEGGER_RTT_MAX_NUM_DOWN_BUFFERS 1
#define BUFFER_SIZE_UP                 256
#define BUFFER_SIZE_DOWN               16
#define SEGGER_RTT_MODE_DEFAULT        SEGGER_RTT_MODE_NO_BLOCK_SKIP
#define RTT_USE_ASM                    0
/* Application RTT has one main-loop writer and no ISR writers. Keep DMA IRQs live. */
#define SEGGER_RTT_LOCK()
#define SEGGER_RTT_UNLOCK()


/*********************************************************************
*
*       Defines, configurable
*
**********************************************************************
*/

#endif
/*************************** End of file ****************************/
