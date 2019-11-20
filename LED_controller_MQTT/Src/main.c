/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "FreeRTOS.h"
#include "cmsis_os.h"
#include "lwip.h"
#include "socket.h"
#include "string.h"
#include "stdlib.h"
#include "mqtt.h"

#define ABSOLUTE_MAX_PWM		999
#define COOLDOWN_INTERVAL		600
#define DIVIDER_COEF			13.0f
#define TXMESSAGEQUEUE_SIZE		10
#define DATACOOLDOWN_INTERVAL	600
#define PARAMFIELD_LENGTH		50

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
CRC_HandleTypeDef hcrc;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim4;

extern struct netif gnetif;

osThreadId defaultTaskHandle;
osThreadId networkTaskHandle;
osThreadId networkReceiveTaskHandle;

osSemaphoreDef_t *ethTxSem;
osSemaphoreId ethTxSemaphoreId;


struct gParamStruct_t paramStruct;
int switchInPrev = 1;
int auxCtrlInPrev = 1;
int proxInPrev = 0;
int lightInPrev = 0;
int timerVal = 0;
int timerEnable = 0;
//int updatePendingFlag = 0;
int connState = CONNSTATE_IF_NOTREADY;
int sock;
char mqttClientId[PARAMFIELD_LENGTH];
char mqttTopicName[PARAMFIELD_LENGTH];
char mqttValue[PARAMFIELD_LENGTH];

/* Private function prototypes -----------------------------------------------*/
void DefaultTask(void const * argument);
void NetworkTaskFunction(void const * argument);
void NetworkReceiveTaskFunction(void const * argument);
void ParseCommand(char *cmdBuf, char *valBuf);

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();
  /* Configure the system clock */
  SystemClock_Config();
  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_TIM3_Init();
  MX_TIM4_Init();
  MX_ADC1_Init();
  MX_CRC_Init();

  /* Initialize Data structures */
  paramStruct.mode = 0;
  paramStruct.switchIn = 1;
  paramStruct.dimMinus = 1;
  paramStruct.dimPlus = 1;
  paramStruct.auxIn = 1;
  paramStruct.auxStatus = 0;
  paramStruct.proxIn = 0;
  paramStruct.lightIn = 0;
  paramStruct.maxPwm = ABSOLUTE_MAX_PWM;
  paramStruct.pwm1 = 0;
  paramStruct.pwm2 = 0;
  paramStruct.pwm3 = 0;
  paramStruct.pwm4 = 0;
  paramStruct.inputVoltage = 0.0f;
  paramStruct.temperature = -1.0f;

  memset(mqttClientId, 0, PARAMFIELD_LENGTH);
  sprintf(mqttClientId, "led_ctrl_%02X%02X%02X", 0x02, 0x07, 0x01);
  memset(mqttTopicName, 0, PARAMFIELD_LENGTH);
  sprintf(mqttTopicName, "inputVoltage");
  memset(mqttValue, 0, PARAMFIELD_LENGTH);
  sprintf(mqttValue, "%d.%d", (int)paramStruct.inputVoltage, ((int)(paramStruct.inputVoltage*10))%10);

  HAL_TIM_PWM_Start(&htim3,  TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim3,  TIM_CHANNEL_2);
  HAL_TIM_PWM_Start(&htim3,  TIM_CHANNEL_3);
  HAL_TIM_PWM_Start(&htim3,  TIM_CHANNEL_4);
  /* Create the thread(s) */
  /* definition and creation of defaultTask */
  osThreadDef(defaultTask, DefaultTask, osPriorityNormal, 0, 128);
  defaultTaskHandle = osThreadCreate(osThread(defaultTask), NULL);
  osThreadDef(networkTask, NetworkTaskFunction, osPriorityNormal, 0, 512);
  networkTaskHandle = osThreadCreate(osThread(networkTask), NULL);
  osThreadDef(networkReceiveTask, NetworkReceiveTaskFunction, osPriorityNormal, 0, 256);
  networkReceiveTaskHandle = osThreadCreate(osThread(networkReceiveTask), NULL);
  ethTxSemaphoreId = osSemaphoreCreate(ethTxSem, 1);
  /* Start scheduler */
  osKernelStart();
  
  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
  }
  /* USER CODE END 3 */
}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used 
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void DefaultTask(void const * argument)
{
  uint32_t i, tickCount = 0;
  float vTmp;

  osDelay(2000);
  /* USER CODE BEGIN 5 */
  /* Infinite loop */
  while(1)
  {
    HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_11);

	/* Check SWITCH input */
	paramStruct.switchIn = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_9);
	/* If SWITCH is ON - force light ON at max brightness*/
	if(paramStruct.switchIn == 0)
	{
	  paramStruct.pwm1 = ABSOLUTE_MAX_PWM;
	  paramStruct.pwm2 = ABSOLUTE_MAX_PWM;
	  paramStruct.pwm3 = ABSOLUTE_MAX_PWM;
	  paramStruct.pwm4 = ABSOLUTE_MAX_PWM;
	  TIM3->CCR1 = paramStruct.pwm1;
	  TIM3->CCR2 = paramStruct.pwm2;
      TIM3->CCR3 = paramStruct.pwm3;
	  TIM3->CCR4 = paramStruct.pwm4;

	  if(switchInPrev == 1)
	  {
		  memset(mqttTopicName, 0, PARAMFIELD_LENGTH);
		  sprintf(mqttTopicName, "pwm");
		  memset(mqttValue, 0, PARAMFIELD_LENGTH);
		  sprintf(mqttValue, "%d", paramStruct.pwm1);
		  osSemaphoreRelease(ethTxSemaphoreId);
		  tickCount = 0;
	  }
	}
	/* If SWITCH is OFF - set user defined MAX value & check other control inputs*/
	else
	{
	  /* Check MOTION sensor input */
	  paramStruct.proxIn = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_3);
	  /* Motion detected*/
  	  if(paramStruct.proxIn == 1 && proxInPrev == 0)
	  {
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_SET);
		if(paramStruct.pwm1 == 0)
		{
		  paramStruct.pwm1 = paramStruct.maxPwm;
		  paramStruct.pwm2 = paramStruct.maxPwm;
		  paramStruct.pwm3 = paramStruct.maxPwm;
		  paramStruct.pwm4 = paramStruct.maxPwm;
		  TIM3->CCR1 = paramStruct.pwm1;
		  TIM3->CCR2 = paramStruct.pwm2;
		  TIM3->CCR3 = paramStruct.pwm3;
		  TIM3->CCR4 = paramStruct.pwm4;

		  memset(mqttTopicName, 0, PARAMFIELD_LENGTH);
		  sprintf(mqttTopicName, "pwm");
		  memset(mqttValue, 0, PARAMFIELD_LENGTH);
		  sprintf(mqttValue, "%d", paramStruct.pwm1);
		  osSemaphoreRelease(ethTxSemaphoreId);
		  tickCount = 0;
		}
		timerVal = 0;
	  }
	  /* Motion stopped - set cooldown timer */
	  if(paramStruct.proxIn == 0 && proxInPrev == 1)
	  {
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_RESET);
		timerEnable = 1;
	  }
	  if(timerEnable == 1)
	  {
		timerVal++;
		if(timerVal >= COOLDOWN_INTERVAL)
		{
		  timerEnable = 0;
		  timerVal = 0;
		  paramStruct.pwm1 = 0;
		  paramStruct.pwm2 = 0;
		  paramStruct.pwm3 = 0;
		  paramStruct.pwm4 = 0;
		  TIM3->CCR1 = paramStruct.pwm1;
		  TIM3->CCR2 = paramStruct.pwm2;
		  TIM3->CCR3 = paramStruct.pwm3;
		  TIM3->CCR4 = paramStruct.pwm4;

		  memset(mqttTopicName, 0, PARAMFIELD_LENGTH);
		  sprintf(mqttTopicName, "pwm");
		  memset(mqttValue, 0, PARAMFIELD_LENGTH);
		  sprintf(mqttValue, "%d", paramStruct.pwm1);
		  osSemaphoreRelease(ethTxSemaphoreId);
		  tickCount = 0;
		}
	  }
	  proxInPrev = paramStruct.proxIn;

	  /* Check DIM+ input */
	  paramStruct.dimPlus = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_8);
	  if(paramStruct.dimPlus == 0)
	  {
	    paramStruct.pwm1 += 10;
		paramStruct.pwm2 += 10;
		paramStruct.pwm3 += 10;
		paramStruct.pwm4 += 10;
		if(paramStruct.pwm1 > paramStruct.maxPwm) paramStruct.pwm1 = 0;
		if(paramStruct.pwm2 > paramStruct.maxPwm) paramStruct.pwm2 = 0;
		if(paramStruct.pwm3 > paramStruct.maxPwm) paramStruct.pwm3 = 0;
		if(paramStruct.pwm4 > paramStruct.maxPwm) paramStruct.pwm4 = 0;
		TIM3->CCR1 = paramStruct.pwm1;
		TIM3->CCR2 = paramStruct.pwm2;
		TIM3->CCR3 = paramStruct.pwm3;
		TIM3->CCR4 = paramStruct.pwm4;

		memset(mqttTopicName, 0, PARAMFIELD_LENGTH);
		sprintf(mqttTopicName, "pwm");
		memset(mqttValue, 0, PARAMFIELD_LENGTH);
		sprintf(mqttValue, "%d", paramStruct.pwm1);
		osSemaphoreRelease(ethTxSemaphoreId);
		tickCount = 0;
	  }
	  /* Check DIM- input */
	  paramStruct.dimMinus = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_5);
	  if(paramStruct.dimMinus == 0)
	  {
		paramStruct.pwm1 -= 10;
		paramStruct.pwm2 -= 10;
		paramStruct.pwm3 -= 10;
		paramStruct.pwm4 -= 10;
		if(paramStruct.pwm1 < 0) paramStruct.pwm1 = paramStruct.maxPwm;
		if(paramStruct.pwm2 < 0) paramStruct.pwm2 = paramStruct.maxPwm;
		if(paramStruct.pwm3 < 0) paramStruct.pwm3 = paramStruct.maxPwm;
		if(paramStruct.pwm4 < 0) paramStruct.pwm4 = paramStruct.maxPwm;
		TIM3->CCR1 = paramStruct.pwm1;
		TIM3->CCR2 = paramStruct.pwm2;
		TIM3->CCR3 = paramStruct.pwm3;
		TIM3->CCR4 = paramStruct.pwm4;

		memset(mqttTopicName, 0, PARAMFIELD_LENGTH);
		sprintf(mqttTopicName, "pwm");
		memset(mqttValue, 0, PARAMFIELD_LENGTH);
		sprintf(mqttValue, "%d", paramStruct.pwm1);
		osSemaphoreRelease(ethTxSemaphoreId);
		tickCount = 0;
	  }

	}
	switchInPrev = paramStruct.switchIn;

	/* Check AUX input. If switch is ON - turn on Relay */
	paramStruct.auxIn = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_4);
	if(paramStruct.auxIn == 0 && auxCtrlInPrev == 1)
	{
	  memset(mqttTopicName, 0, PARAMFIELD_LENGTH);
	  sprintf(mqttTopicName, "auxIn");
	  memset(mqttValue, 0, PARAMFIELD_LENGTH);
	  sprintf(mqttValue, "%d", paramStruct.auxIn);
	  osSemaphoreRelease(ethTxSemaphoreId);
	  tickCount = 0;

  	  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET);
  	  paramStruct.auxStatus = 1;
	}
	/* If switch is OFF - turn off Relay */
	if(paramStruct.auxIn == 1 && auxCtrlInPrev == 0)
	{
	  memset(mqttTopicName, 0, PARAMFIELD_LENGTH);
	  sprintf(mqttTopicName, "auxIn");
	  memset(mqttValue, 0, PARAMFIELD_LENGTH);
	  sprintf(mqttValue, "%d", paramStruct.auxIn);
	  osSemaphoreRelease(ethTxSemaphoreId);
	  tickCount = 0;

	  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_RESET);
	  paramStruct.auxStatus = 0;
	}
	auxCtrlInPrev = paramStruct.auxIn;
	/* Start ADC to measure Vin*/
	HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1,100);
    i = (uint32_t) HAL_ADC_GetValue(&hadc1);
    vTmp = (float)i*3.3/4096;
    paramStruct.inputVoltage = vTmp * DIVIDER_COEF;

	tickCount++;
	if(tickCount > DATACOOLDOWN_INTERVAL)
	{
		memset(mqttTopicName, 0, PARAMFIELD_LENGTH);
		sprintf(mqttTopicName, "inputVoltage");
		memset(mqttValue, 0, PARAMFIELD_LENGTH);
		sprintf(mqttValue, "%d.%d", (int)paramStruct.inputVoltage, ((int)(paramStruct.inputVoltage*10))%10);
		osSemaphoreRelease(ethTxSemaphoreId);
		tickCount = 0;
	}

    osDelay(100);
  }
  /* USER CODE END 5 */ 
}

void NetworkTaskFunction(void const * argument)
{
  int sock, error, errorCount = 0;
  //unsigned char ipAddr[4] = {0,0,0,0};
  struct sockaddr_in localhost, remotehost;

  /* init code for LWIP */
  MX_LWIP_Init();
  /* Wait for IP address assignment */
  while(gnetif.ip_addr.addr == 0)
  {
	  osDelay(100);
  }
  /* set flag - interface ready */
  connState = CONNSTATE_IF_READY;

  while(1)
  {
	if(connState == CONNSTATE_IF_READY)
	{
		close(sock);
		if((sock = socket(AF_INET,SOCK_STREAM, IPPROTO_TCP)) >= 0)
		 {
			memset(&localhost, 0, sizeof(struct sockaddr_in));
			localhost.sin_family = AF_INET;
			localhost.sin_port = htons(1234);
			localhost.sin_addr.s_addr = INADDR_ANY;
			error = bind(sock, (struct sockaddr *)&localhost, sizeof(struct sockaddr_in));
			if(error >= 0) connState = CONNSTATE_SOCKET_READY;
		 }
	}
	if(connState == CONNSTATE_SOCKET_READY)
	{
		memset(&remotehost, 0, sizeof(struct sockaddr_in));
		remotehost.sin_family = AF_INET;
		remotehost.sin_port = htons(1883);
		ip4addr_aton("192.168.0.100",(ip4_addr_t*)&remotehost.sin_addr);
		connState = CONNSTATE_TCP_NOCONNECT;
	}
    if(connState == CONNSTATE_TCP_NOCONNECT)
    {
    	error = connect(sock, (struct sockaddr *) &remotehost, sizeof(struct sockaddr_in));
    	if(error >= 0)
    	{
    		connState = CONNSTATE_TCP_CONNECTED;
    		//error = MqttConnect(sock, mqttClientId);
    	}
    	else
    	{
    		errorCount++;
    		if(errorCount > 3)
    		{
    			connState = CONNSTATE_IF_READY;
    			errorCount = 0;
    		}
    	}
    	//osDelay(10);
    }
    if(connState == CONNSTATE_TCP_CONNECTED)
    {
    	error = MqttConnect(sock, mqttClientId);
    	if(error < 0) connState = CONNSTATE_IF_READY;
    	osDelay(100);
    }
    if(connState == CONNSTATE_MQTT_CONNECTED)
    {
    	error = MqttSubscribe(sock, mqttClientId, "cmd\/#", 101);
    	connState = CONNSTATE_MQTT_READY;
    }
    if(connState == CONNSTATE_MQTT_READY)
    {
    	if(osSemaphoreWait(ethTxSemaphoreId, 1000) == 0)
    	{
    	  error = MqttPublish(sock, mqttClientId, mqttTopicName, mqttValue, 0);
    	  if(error < 0)
    	  {
    		  connState = CONNSTATE_TCP_NOCONNECT;
    		  shutdown(sock, 2);
    	  }
    	}
    }
    //osDelay(1000);
  }
}

void NetworkReceiveTaskFunction(void const * argument)
{
	int nbytes, nrecv = 0, err;
	unsigned char buffer[200];
	char cmdBuf[50], valBuf[20];
	while(1)
	{
		while(connState < CONNSTATE_TCP_CONNECTED)
		{
			osDelay(10);
		}
		nbytes = recv(sock, buffer, sizeof(buffer),0);
		if(nbytes > 0)
		{
			nrecv++;
			err = MqttParse(buffer, mqttClientId, "cmd", cmdBuf, valBuf);
			if(err == 100) connState = CONNSTATE_MQTT_CONNECTED;
			if(err == 101) ParseCommand(cmdBuf, valBuf);
			memset(buffer, 0, 200);
			memset(cmdBuf, 0, 50);
			memset(valBuf, 0, 20);
		}
	}
}

void ParseCommand(char *cmdBuf, char *valBuf)
{
	/* Command maxPwm - sets maximum brightness for current context (0..999) */
	if(strcmp(cmdBuf, "maxPwm") == 0)
	{
		if(atoi(valBuf) >= 0 && atoi(valBuf) <= ABSOLUTE_MAX_PWM)
		{
			paramStruct.maxPwm = atoi(valBuf);
			/*if(paramStruct.pwm1 != 0 && paramStruct.switchIn != 0)
			{
				updatePendingFlag = 1;
			}*/
		}
	}
	/* Command uaxCtrl - switch ON/OFF aux relay (1 - ON, 0 -OFF) */
	if(strcmp(cmdBuf, "auxCtrl") == 0)
	{
		if(atoi(valBuf) == 1)
		{
			paramStruct.auxStatus = 1;
			HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET);
			memset(mqttTopicName, 0, PARAMFIELD_LENGTH);
			sprintf(mqttTopicName, "auxStatus");
			memset(mqttValue, 0, PARAMFIELD_LENGTH);
			sprintf(mqttValue, "%d", paramStruct.auxStatus);
			osSemaphoreRelease(ethTxSemaphoreId);
		}
		else
		{
			paramStruct.auxStatus = 0;
			HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_RESET);
			memset(mqttTopicName, 0, PARAMFIELD_LENGTH);
			sprintf(mqttTopicName, "auxStatus");
			memset(mqttValue, 0, PARAMFIELD_LENGTH);
			sprintf(mqttValue, "%d", paramStruct.auxStatus);
			osSemaphoreRelease(ethTxSemaphoreId);
		}
	}
}
