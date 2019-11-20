/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */
struct gParamStruct_t {
	int mode;
	int switchIn;
	int dimPlus;
	int dimMinus;
	int auxIn;
	int auxStatus;
	int proxIn;
	int lightIn;
	int maxPwm;
	int pwm1;
	int pwm2;
	int pwm3;
	int pwm4;
	float inputVoltage;
	float temperature; /*reserved for future use*/
};
/* USER CODE END ET */


void SystemClock_Config(void);
void MX_GPIO_Init(void);
void MX_TIM3_Init(void);
void MX_TIM4_Init(void);
void MX_ADC1_Init(void);
void MX_CRC_Init(void);
void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);
void Error_Handler(void);


/* Private defines -----------------------------------------------------------*/
#define _50MHz_output_Pin GPIO_PIN_8
#define _50MHz_output_GPIO_Port GPIOA
/* USER CODE BEGIN Private defines */
#define CONNSTATE_IF_NOTREADY		0
#define CONNSTATE_IF_READY			1
#define CONNSTATE_SOCKET_READY		2
#define CONNSTATE_TCP_NOCONNECT		3
#define CONNSTATE_TCP_CONNECTED		4
#define CONNSTATE_MQTT_CONNECTED	5
#define CONNSTATE_MQTT_READY		6

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
