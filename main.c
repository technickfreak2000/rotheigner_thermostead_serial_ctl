/**
  ******************************************************************************
  * @file    GPIO/GPIO_Toggle/main.c
  * @author  MCD Application Team
  * @version V1.5.2
  * @date    30-September-2014
  * @brief   Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT 2014 STMicroelectronics</center></h2>
  *
  * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
  * You may not use this file except in compliance with the License.
  * You may obtain a copy of the License at:
  *
  *        http://www.st.com/software_license_agreement_liberty_v2
  *
  * Unless required by applicable law or agreed to in writing, software 
  * distributed under the License is distributed on an "AS IS" BASIS, 
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  ******************************************************************************
  */ 

/* Includes ------------------------------------------------------------------*/
#include "stdio.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include "stm8l15x.h"
#include "stm8l15x_it.h"    /* SDCC patch: required by SDCC for interrupts */
#include "stm8l15x_clk.h"

/** @addtogroup STM8L15x_StdPeriph_Examples
  * @{
  */

/** @addtogroup GPIO_Toggle
  * @{
  */

/* Private typedef -----------------------------------------------------------*/
typedef struct char_list {
  char character;
  struct char_list *next;
} char_list_t;

typedef struct motor_config {
  uint8_t max_current;

  // Motor GPIO
  GPIO_TypeDef *GPIO_port_motor_out;
  GPIO_Pin_TypeDef GPIO_pin_motor_out;
  GPIO_TypeDef *GPIO_port_motor_in;
  GPIO_Pin_TypeDef GPIO_pin_motor_in;

  // Motor current
  GPIO_TypeDef *GPIO_port_motor_curr;
  GPIO_Pin_TypeDef GPIO_pin_motor_curr;
} motor_config_t;

typedef enum
{
  STOP,
  IN,
  OUT,
} motor_ctl_t;

/* Private define ------------------------------------------------------------*/
// Reset device name on every startup 
#define RESET_DEVICE_NAME true

/* Define I2C Slave Address --------------------------------------------------*/
#define SLAVE_ADDRESS 0x30

/* define the GPIO port and pins */
// Motor PE7, PE6
#define MOTOR_OUT_GPIO_PORT   GPIOE
#define MOTOR_OUT_GPIO_PINS   GPIO_Pin_6
#define MOTOR_IN_GPIO_PORT    GPIOE
#define MOTOR_IN_GPIO_PINS    GPIO_Pin_7
  // Motor Current PC7
#define MOTOR_CURR_GPIO_PORT  GPIOC
#define MOTOR_CURR_GPIO_PINS  GPIO_Pin_7
#define MOTOR_CURR_END_MAX    450        // Max motor current before either valve or motor end is detected
#define MOTOR_CURR_MAX_DEV    5          // Max deviation between read current and reference before valve is detected

// IR Counter PA3 power, PC4 readout
#define IR_PWR_GPIO_PORT      GPIOA
#define IR_PWR_GPIO_PINS      GPIO_Pin_3
#define IR_REC_GPIO_PORT      GPIOC
#define IR_REC_GPIO_PINS      GPIO_Pin_4
#define COUNTER_MAX_OUT       290        // Max steps the motor can turn out
#define COUNTER_IGNORE_STEP   20         // How many steps are ignored in calculation                                             (0                   -> COUNTER_IGNORE_STEP)
#define COUNTER_REF_STEP      20         // How many steps after the ignored are used to calculate the unloaded reference current (COUNTER_IGNORE_STEP ->    COUNTER_REF_STEP)
#define COUNTER_VAL_MIN_STEP  50         // Minimum steps a 'detected' valve must have before its called a valve
#define COUNTER_SAFE_0        15         // If thermostead should be at 0%, it uses this offset to be sure it is fully closed

// #define STACK_MODEL_LARGE

// USART
  // Define USART Boudrate
#define USART_BOUD            9600
  // Use pullups on USART
// #define USART_PULLUPS

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
char name[11];
bool name_exists = false;
uint16_t counter = 0;
uint16_t counter_start_safe_val = 0;          // Safe start value at which valve should be close to 0% (counter_start_val - COUNTER_SAFE_0)
uint16_t counter_start_val = 0;               // Start value at which valve should be close to 1%;
uint16_t counter_end_val = COUNTER_MAX_OUT;   // End value at which valve should be close to 100%, no offset needet, because it is determined using the current limit

uint16_t counter_current = 0;

uint16_t adc_val = 0;
uint16_t current_map[COUNTER_MAX_OUT + 1];

/* Private function prototypes -----------------------------------------------*/
void init(void);
void calibrate(bool debug);
void console(void);
void console_handle(char *rec);
void print_help(void);
void set_command(char *input_val);
void get_tmp_command(void);
void set_name(char *input_val);
char* getchar_arr(void);
void Delay (uint32_t nCount);
uint16_t get_ADC_data(ADC_TypeDef* ADCx);
uint16_t count_digits(uint16_t number);
void normalize_newlines(char *str);
/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Main program.
  * @param  None
  * @retval None
  */
void main(void)
{
  init();

  printf("\n\rHello! I'm a smarter thermostat now! :)\n\r");

  if (name_exists)
  {
    printf("My name is: %s \n\r\n\r", name);
    printf("Say \"%s help\" if you need some help,\n\r", name);
  }
  else
  {
    printf("\n\rSay \"help\" if you need some help,\n\r");
  }
  
  printf("I will send 'ok' or 'err' with every message, so you can validate :)\n\r");

  printf("\n\rBut first, let me just adjust myself real quick...\n\r\n\r");
  
  calibrate(false);

  printf("Ok, lets go, what should I do?\n\r");
  printf("\n\r");

  while (1)
  {
    console();
  }               
}

void init(void)
{
  // DeInit all IO
  GPIO_DeInit(GPIOA);
  GPIO_DeInit(GPIOB);
  GPIO_DeInit(GPIOC);
  GPIO_DeInit(GPIOD);
  GPIO_DeInit(GPIOE);
  GPIO_DeInit(GPIOF);

  /* system_clock / 1 */
  CLK_SYSCLKDivConfig(CLK_SYSCLKDiv_1);
 
 // I2C
  /* I2C  clock Enable*/
  // CLK_PeripheralClockConfig(CLK_Peripheral_I2C1, ENABLE);

  /* Initialize I2C peripheral */
  // I2C_DeInit(I2C1);
  // I2C_Init(I2C1, 10000, SLAVE_ADDRESS,
  //          I2C_Mode_I2C, I2C_DutyCycle_2,
  //          I2C_Ack_Enable, I2C_AcknowledgedAddress_7bit);
  
  /* Enable Error Interrupt*/
  // I2C_ITConfig(I2C1, (I2C_IT_TypeDef)(I2C_IT_ERR | I2C_IT_EVT | I2C_IT_BUF), ENABLE);

  /* Enable general interrupts */
  // enableInterrupts();

 // USART
  /* Enable USART clock */
  CLK_PeripheralClockConfig((CLK_Peripheral_TypeDef)CLK_Peripheral_USART1, ENABLE);
  Delay(0xFFFF);

#ifdef USART_PULLUPS
  /* Configure USART Tx as alternate function push-pull  (software pull up)*/
  GPIO_ExternalPullUpConfig(GPIOC, GPIO_Pin_3, ENABLE);

  /* Configure USART Rx as alternate function push-pull  (software pull up)*/
  GPIO_ExternalPullUpConfig(GPIOC, GPIO_Pin_2, ENABLE);
#endif
  GPIO_ExternalPullUpConfig(GPIOC, GPIO_Pin_3, ENABLE);
  /* USART configuration */
  USART_Init(USART1, (uint32_t)USART_BOUD,
             USART_WordLength_8b,
             USART_StopBits_1,
             USART_Parity_No,
             (USART_Mode_TypeDef)(USART_Mode_Tx | USART_Mode_Rx));

  // Flash
  FLASH_SetProgrammingTime(FLASH_ProgramTime_Standard);

  /* Unlock flash data eeprom memory */
  FLASH_Unlock(FLASH_MemType_Data);
  /* Wait until Data EEPROM area unlocked flag is set*/
  while (FLASH_GetFlagStatus(FLASH_FLAG_DUL) == RESET)
  {}

  if (RESET_DEVICE_NAME)
  {
    FLASH_ProgramByte(FLASH_DATA_EEPROM_START_PHYSICAL_ADDRESS + ((uint32_t)10), (uint8_t)0);
  }

  name_exists = (bool) FLASH_ReadByte(FLASH_DATA_EEPROM_START_PHYSICAL_ADDRESS + ((uint32_t)10));

  if (name_exists)
  {
    for (size_t i = 0; i < 10; i++)
    {
      name[i] = (char) FLASH_ReadByte(FLASH_DATA_EEPROM_START_PHYSICAL_ADDRESS + ((uint32_t)i));
    }
    
    name[10] = '\0';
  }
  else
  {
    name[0] = '\0';
  }
  
  /* IO */
  // Motor
  GPIO_Init(MOTOR_OUT_GPIO_PORT, MOTOR_OUT_GPIO_PINS, GPIO_Mode_Out_PP_High_Fast);
  GPIO_Init(MOTOR_IN_GPIO_PORT, MOTOR_IN_GPIO_PINS, GPIO_Mode_Out_PP_High_Fast);

  GPIO_Init(MOTOR_CURR_GPIO_PORT, MOTOR_CURR_GPIO_PINS, GPIO_Mode_In_FL_No_IT);

  // IR
  GPIO_Init(IR_PWR_GPIO_PORT, IR_PWR_GPIO_PINS, GPIO_Mode_Out_PP_Low_Fast);
  GPIO_Init(IR_REC_GPIO_PORT, IR_REC_GPIO_PINS, GPIO_Mode_In_FL_IT);
  EXTI_SetPinSensitivity(IR_REC_GPIO_PINS, EXTI_Trigger_Falling);

  /* Enable ADC1 clock */
  CLK_PeripheralClockConfig(CLK_Peripheral_ADC1, ENABLE);

  /* Initialise and configure ADC1 */
  ADC_Init(ADC1, ADC_ConversionMode_Continuous, ADC_Resolution_12Bit, ADC_Prescaler_2);

  /* Enable ADC1 */
  ADC_Cmd(ADC1, ENABLE);

  /* Enable ADC1 Channel 3 */
  ADC_ChannelCmd(ADC1, ADC_Channel_3, ENABLE);

  enableInterrupts();
  disableInterrupts();
  enableInterrupts();
}

uint16_t get_ADC_data(ADC_TypeDef* ADCx)
{
  ADC_SoftwareStartConv(ADCx);
  while (ADC_GetFlagStatus(ADCx, ADC_FLAG_EOC) == FALSE) {};
  ADC_ClearFlag(ADCx, ADC_FLAG_EOC);

  return ADC_GetConversionValue(ADCx);
}

void calibrate(bool debug)
{
  printf("Calibrating...\n\r");
  counter_start_safe_val = 0;
  counter_start_val = 0;
  counter_end_val = COUNTER_MAX_OUT;

  // Retract motor
  if (debug)
  {
    printf("\n\rGoing back in!\n\r");
  }
  GPIO_ResetBits(MOTOR_IN_GPIO_PORT, MOTOR_IN_GPIO_PINS);
  Delay(0xFFF);
  while (get_ADC_data(ADC1) < MOTOR_CURR_END_MAX) {}
  GPIO_SetBits(MOTOR_IN_GPIO_PORT, MOTOR_IN_GPIO_PINS);
  Delay(0xFFFFF); 

  // Create current List
  if (debug)
  {
    printf("\n\rCreating current map!\n\r");
  }
  
  GPIO_SetBits(IR_PWR_GPIO_PORT, IR_PWR_GPIO_PINS);
  Delay(0xFF);
  counter = 0;
  for (uint16_t i = 0; i <= COUNTER_MAX_OUT; i++)
  {
    current_map[i] = 0;
  }
  
  GPIO_ResetBits(MOTOR_OUT_GPIO_PORT, MOTOR_OUT_GPIO_PINS);
  Delay(0xFFF);
  adc_val = get_ADC_data(ADC1);
  if (adc_val > MOTOR_CURR_END_MAX)
  {
    adc_val = MOTOR_CURR_END_MAX / 2;
  }
  
  while (adc_val < MOTOR_CURR_END_MAX && counter < COUNTER_MAX_OUT) {
    adc_val = get_ADC_data(ADC1);
  }
  GPIO_SetBits(MOTOR_OUT_GPIO_PORT, MOTOR_OUT_GPIO_PINS);
  GPIO_ResetBits(IR_PWR_GPIO_PORT, IR_PWR_GPIO_PINS);

  if (debug)
  {
    printf("Current map:\n\r");
    printf("-num-|-cur-|-info-\n\r");
    printf("-----|-----|------\n\r");
  }
  uint16_t ref = 0;
  for (uint16_t j = 0; j <= COUNTER_MAX_OUT; j++)
  {
    uint16_t current_value = current_map[j];

    if (debug)
    {
      for (size_t i = 0; i < (5 - count_digits(j)); i++)
      {
        printf("-");
      }
      printf("%u|", j);
    
      for (size_t i = 0; i < (5 - count_digits(current_value)); i++)
      {
        printf("-");
      }
      printf("%u|", current_value);
    }

    if (j <= COUNTER_IGNORE_STEP)
    {
      if (debug)
      {
        printf("ignore\n\r");
      }
    }
    else if (j < COUNTER_IGNORE_STEP + COUNTER_REF_STEP)
    {
      ref += current_value;
      if (debug)
      {
        printf("ref---\n\r");
      }
    }
    else if (j == COUNTER_IGNORE_STEP + COUNTER_REF_STEP)
    {
      ref += current_value;
      ref /= COUNTER_REF_STEP;
      if (debug)
      {
        printf("ref");
        for (size_t i = 0; i < (3 - count_digits(ref)); i++)
        {
          printf("-");
        }
        printf("%u\n\r", ref);
      }
    }
    else
    {
      if (j > 1 && current_value > 0)
      {
        uint16_t tmp = 0;
        if (current_value < ref)
        {
          tmp = ref - current_value;
        }
        else
        {
          tmp = current_value - ref;
        }
        if (debug)
        {
          printf("%u", tmp);
        }
        uint8_t tmp_i = 6;
        if(tmp > MOTOR_CURR_MAX_DEV)
        {
          counter_end_val = j;
          tmp_i = 5;
        }
        if (debug)
        {
          for (size_t i = 0; i < (tmp_i - count_digits(tmp)); i++)
          {
            printf("-");
          }
          if(tmp > MOTOR_CURR_MAX_DEV)
          {
            printf("f");
          }
          printf("\n\r");
        }
      }
      else if (current_value == 0)
      {
        if (debug)
        {
          printf("end---\n\r");
        }
      }
      else
      {
        if (debug)
        {
          printf("------\n\r");
        }
      }
      
    }
  }
  if (debug)
  {
    printf("------------------\n\r");
  }

  for (uint16_t i = counter_end_val; i > COUNTER_IGNORE_STEP + COUNTER_REF_STEP; i--)
  {
    uint16_t current_value = current_map[i];
    uint16_t tmp = 0;
    if (current_value < ref)
    {
      tmp = ref - current_value;
    }
    else
    {
      tmp = current_value - ref;
    }
    if(tmp > MOTOR_CURR_MAX_DEV)
    {
      counter_start_val = i;
    }
    else
    {
      break;
    }
  }

  if (counter_start_val >= counter_end_val || counter_end_val - counter_start_val < COUNTER_VAL_MIN_STEP)
  {
    counter_start_val = 0;
  }
  
  if (counter_start_val == 0)
  {
    counter_end_val = COUNTER_MAX_OUT;
  }
  
  if (counter_start_val >= COUNTER_SAFE_0)
  {
    counter_start_safe_val = counter_start_val - COUNTER_SAFE_0;
  }
  else
  {
    counter_start_safe_val = 0;
  }

  Delay(0xFFFF); 
  // Retract motor
  if (debug)
  {
    printf("Going back in!\n\r");
  }
  GPIO_ResetBits(MOTOR_IN_GPIO_PORT, MOTOR_IN_GPIO_PINS);
  Delay(0xFFF);
  while (get_ADC_data(ADC1) < MOTOR_CURR_END_MAX) {}
  GPIO_SetBits(MOTOR_IN_GPIO_PORT, MOTOR_IN_GPIO_PINS);
  Delay(0xFFFFF);

  if (debug)
  {
    printf("Safe 0%% valve count: %u, Start valve count: %u, End valve count: %u\n\r", counter_start_safe_val, counter_start_val, counter_end_val);
  }

  printf("Safe 0%% valve: %u%%, Start valve: %u%%, End valve: %u%%\n\r\n\r", (counter_start_safe_val*100)/COUNTER_MAX_OUT, (counter_start_val*100)/COUNTER_MAX_OUT, (counter_end_val*100)/COUNTER_MAX_OUT);
}

void console(void)
{
  char *rec = getchar_arr();

  normalize_newlines(rec);

  // If the input is exactly "RESET", clear the stored name.
  if (strcmp(rec, "RESET") == 0)
  {
      FLASH_ProgramByte(FLASH_DATA_EEPROM_START_PHYSICAL_ADDRESS + ((uint32_t)10), (uint8_t)0);
      name_exists = false;
      printf("Reset done.\n\r");
      
      // Free the memory and exit this function (skip further processing).
      free(rec);
      return;
  }

  if (name_exists)
  {
    char *token_name = strstr(rec, " ");
    if (token_name != NULL && strncmp(rec, name, strlen(name)) == 0 && strlen(name) == (strlen(rec) - strlen(token_name)))
    {
      char *input_without_name = (char *) malloc(sizeof(char) * (strlen(token_name + 1) + 1));
      strcpy(input_without_name, token_name + 1);
      free(rec);
      rec = input_without_name;
      console_handle(rec);
    }
  }
  else
  {
    console_handle(rec);
  }

  free(rec);
}

void console_handle(char *rec)
{
  char *token = strstr(rec, " ");
  char *input_values = NULL;

  if (token != NULL)
  {
    *token = '\0';

    input_values = (char *) malloc(sizeof(char) * (strlen(token + 1) + 1));

    strcpy(input_values, token + 1);
  }

  if (strcmp(rec, "help") == 0)
  {
    print_help();
  }
  else if (strcmp(rec, "recalibrate") == 0)
  {
    calibrate(false);
  }
  else if (strcmp(rec, "recalibrate_db") == 0)
  {
    calibrate(true);
  }
  else if (strcmp(rec, "set") == 0)
  {
    set_command(input_values);
  }
  else if (strcmp(rec, "get_tmp") == 0)
  {
    get_tmp_command();
  }
  else if (strcmp(rec, "name") == 0)
  {
    set_name(input_values);
  }
  else
  {
    printf("Sorry, unknown command :(\n\r");
    printf("Command: %s\n\r", rec);
    printf("---\n\r");
    printf("Hint: You might find your answer in 'help' :)\n\r");
    printf("\n\rerr\n\r");
  }
  free(input_values);
}

void print_help(void)
{
  printf("Oh, you need some help?\n\r");
  printf("\n\r");
  printf("recalibrate      : recalibrate the thermostat\n\r");
  printf("recalibrate_db   : recalibrate the thermostat and show debug output\n\r");
  printf("set x            : this sets the valve to 0%% - 100%%, eg: 'set 30'\n");
  printf("get_tmp          : this gets you the device temperature in degrees celsius, not jet implemented\n\r");
  printf("name x           : give me a name, be sure to remember, I only hear if you call me: 'name command', send nothing to remove\n\r");
  printf("\n\r");
  printf("RESET            : can be sent even with a name to remove the name, be careful on a bus!\n\r");
  printf("---\n\r");
  printf("help             : show this help\n\r");
  printf("\n\rok\n\r");
}

void set_command(char *input_val)
{
  int input_num = 0;
  bool checked_input = true;

  if (input_val != NULL && input_val[0] != '\0' && strlen(input_val) < 4)
  {
    for (int i = 0; input_val[i] != '\0'; i++) {
        // Check if the character is a numeric character
        if (!isdigit(input_val[i])) {
          checked_input = false;
          printf("Charakter provided in x is not a integer\n\r");
        }
    }
  }
  else
  {
    checked_input = false;
    printf("Charakter provided in x is NULL, empty or too long\n\r");
  }

  if (checked_input)
  {
    input_num = atoi(input_val);
    if (input_num > 100 || input_num < 0)
    {
      checked_input = false;
      printf("Charakter provided in x is too large or negative\n\r");
    }
  }
  
  if (checked_input)
  {
    // Calculate new counter value
    uint16_t counter_new = counter_start_val + (uint16_t)((float)(counter_end_val - counter_start_val) * (float)input_num / (float) 100);
    if(input_num == 0)
    {
      counter_new = counter_start_safe_val;
    }

    if (counter_new != counter_current)
    {
      counter_current = counter_new;

      // Retract motor
      GPIO_ResetBits(MOTOR_IN_GPIO_PORT, MOTOR_IN_GPIO_PINS);
      Delay(0xFFF);
      while (get_ADC_data(ADC1) < MOTOR_CURR_END_MAX) {}
      GPIO_SetBits(MOTOR_IN_GPIO_PORT, MOTOR_IN_GPIO_PINS);
      Delay(0xFFFFF); 

      // Set thermo point
      GPIO_SetBits(IR_PWR_GPIO_PORT, IR_PWR_GPIO_PINS);
      Delay(0xFF);
      counter = 0;
      GPIO_ResetBits(MOTOR_OUT_GPIO_PORT, MOTOR_OUT_GPIO_PINS);

      Delay(0xFFF);
      adc_val = get_ADC_data(ADC1);
      if (adc_val > MOTOR_CURR_END_MAX)
      {
        adc_val = MOTOR_CURR_END_MAX / 2;
      }

      while (adc_val <= MOTOR_CURR_END_MAX && counter < counter_new) 
      {
        adc_val = get_ADC_data(ADC1);
      }

      GPIO_SetBits(MOTOR_OUT_GPIO_PORT, MOTOR_OUT_GPIO_PINS);
      GPIO_ResetBits(IR_PWR_GPIO_PORT, IR_PWR_GPIO_PINS);

      printf("\r\nok\n\r");
    }
  }
  else
  {
    printf("Provided character in x: %s\n\r", input_val);
    printf("\r\nerr\n\r");
  }
}

void get_tmp_command(void)
{
  printf("NOT JET IMPLEMENTED!!!!\n\r");
  printf("\n\rerr\n\r");
}

void set_name(char *input_val)
{
  bool checked_input = true;

  if (strlen(input_val) > 10)
  {
    checked_input = false;
    printf("Name is too long, max 10 characters!\n\r");
  }
  if (strstr(input_val, " ") != NULL)
  {
    checked_input = false;
    printf("You cant have spaces!\n\r");
  }

  if (checked_input)
  {
    if (input_val == NULL || input_val[0] == '\0')
    {
      FLASH_ProgramByte(FLASH_DATA_EEPROM_START_PHYSICAL_ADDRESS + ((uint32_t)10), (uint8_t)0);
      name_exists = false;
      printf("No name :(\n\r");
    }
    else
    {
      FLASH_ProgramByte(FLASH_DATA_EEPROM_START_PHYSICAL_ADDRESS + ((uint32_t)10), (uint8_t)1);
      name_exists = true;

      for (size_t i = 0; i < 10; i++)
      {
        FLASH_ProgramByte(FLASH_DATA_EEPROM_START_PHYSICAL_ADDRESS + ((uint32_t)i), (uint8_t)input_val[i]);
        /* Wait until End of high voltage flag is set*/
        while (FLASH_GetFlagStatus(FLASH_FLAG_HVOFF) == RESET)
        {}

        name[i] = (char) FLASH_ReadByte(FLASH_DATA_EEPROM_START_PHYSICAL_ADDRESS + ((uint32_t)i));
      }
      name[10] = '\0';

      printf("My name is: %s \n\r", name);
    }
    
    printf("\r\nok\n\r");
  }
  else
  {
    printf("Provided character in x: %s\n\r", input_val);
    printf("\r\nerr\n\r");
  }
}

uint16_t count_digits(uint16_t number) {
    uint16_t count = 0;

    // Count the number of divisions needed to reduce the number to 0
    do {
        number /= 10;
        count++;
    } while (number != 0);

    return count;
}

char* getchar_arr(void) 
{
  // Receive all chars to new line / none are there and count them
  char tmp_rec = getchar();
  size_t count_rec = 1;

  char_list_t *list_rec = (char_list_t *) malloc(sizeof(char_list_t));
  char_list_t *first_list_rec = list_rec;

  list_rec->character = tmp_rec;

  while ((tmp_rec != NULL) && (tmp_rec != '\n'))
  {
    tmp_rec = getchar();
    list_rec->next = (char_list_t *) calloc(1, sizeof(char_list_t));
    list_rec = list_rec->next;
    list_rec->character = tmp_rec;

    count_rec++;
  }
  
  char *out_arr = (char *) malloc(sizeof(char) * count_rec);
  list_rec = first_list_rec;
  size_t count_out = count_rec;

  while (list_rec != NULL)
  {
    out_arr[count_rec - count_out] = list_rec->character;
    count_out--;
    char_list_t *prev_list_rec = list_rec;
    list_rec = list_rec->next;
    free(prev_list_rec);
  }
  
  //fix potential last character
  out_arr[(count_rec - count_out) - 1] = '\0';
  
  return out_arr;
}

/**
 * Normalize newlines in-place:
 * - Remove any cr and nl
*/
void normalize_newlines(char *str) {
  if (str == NULL) return;
  
  int i = 0;  // read index
  int j = 0;  // write index

  while (str[i] != '\0') {
      if (str[i] != '\r' && str[i] != '\n') {
          str[j++] = str[i];
      }
      i++;
  }
  // Null-terminate the string.
  str[j] = '\0';
}

/**
  * @brief Retargets the C library printf function to the USART.
  * @param[in] c Character to send
  * @retval char Character sent
  * @par Required preconditions:
  * - None
  */
int putchar (int c)
{
  /* Write a character to the USART */
  USART_SendData8(USART1, c);
  /* Loop until the end of transmission */
  while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET);

  return (c);
}

/**
  * @brief Retargets the C library scanf function to the USART.
  * @param[in] None
  * @retval char Character to Read
  * @par Required preconditions:
  * - None
  */
int getchar (void)
{
  int c = 0;
  /* Loop until the Read data register flag is SET */
  while (USART_GetFlagStatus(USART1, USART_FLAG_RXNE) == RESET);
    c = USART_ReceiveData8(USART1);
    return (c);
  }

/**
  * @brief  Inserts a delay time.
  * @param  nCount: specifies the delay time length.
  * @retval None
  */
void Delay(__IO uint32_t nCount)
{
  /* Decrement nCount value */
  while (nCount != 0)
  {
    nCount--;
  }
}

#ifdef  USE_FULL_ASSERT

/**
  * @brief  Reports the name of the source file and the source line number
  *   where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t* file, uint32_t line)
{
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* Infinite loop */
  while (1)
  {}
}
#endif

/**
  * @}
  */

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
