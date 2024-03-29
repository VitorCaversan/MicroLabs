// main.c
// Desenvolvido para a placa EK-TM4C1294XL
// Verifica o estado das chaves USR_SW1 e USR_SW2, acende os LEDs 1 e 2 caso estejam pressionadas independentemente
// Caso as duas chaves estejam pressionadas ao mesmo tempo pisca os LEDs alternadamente a cada 500ms.
// Prof. Guilherme Peron

#include <stdint.h>
#include "tm4c1294ncpdt.h"

///////// DEFINES AND MACROS //////////

#define MAX_DELTA_AD_VAL 64
#define MAX_CLOCKWISE_ADC_VAL 0x7FF
#define MAX_ADC_VALUE 0xFFF
#define INVALID_ADC_VALUE 0xF000

#define TICKS_FOR_1MS 80000

typedef enum SysState
{
   WAITING_CTRL_MODE,
   WAITING_DIRECTION,
   ROTATING_POTENT_AND_KEY,
   ROTATING_POTENT_ONLY,
} SysState;

typedef enum sentido
{
   CLOCKWISE,
   COUNTER_CLOCKWISE,
} Sentido;

typedef enum controle
{
   KEYBOARD_AND_POTENT,
   POTENT_ONLY
} Controle;

typedef enum bool
{
   false,
   true
} bool;

typedef struct DC_MotorRotation
{
   Sentido dcRotDirection;
   Controle dcRotType;
   bool bRunNow;
   unsigned short dcAdVal;
   unsigned char dcPwmPercent;
} DC_MotorRotation;

///////// EXTERNAL FUNCTIONS INCLUSIONS //////////
// Since there is no .h in most files, their functions must be included by hand.
// Same as if we were using IMPORT from assembly

void PLL_Init(void);
void SysTick_Init(void);
void SysTick_Wait1ms(uint32_t delay);
void SysTick_Wait1us(uint32_t delay);

void GPIO_Init(void);
uint32_t PortJ_Input(void);
void PortN_Output(uint32_t leds);
void PortF_Output(uint32_t valor);

void timerInit(void);

void uart_uartInit(void);
unsigned char uart_uartRx(void);
void uart_uartTx(unsigned char txMsg);

extern void adc_adcInit(void);
extern void adc_startAdcConversion(void);
extern void adc_stopAdc3Conversion(void);
extern unsigned short adc_readAdc3Value(void);

extern void dcMotor_init(void);
extern void dcMotor_rotateMotor(Sentido enMotorDirection, bool bRotate);

void LCD_GPIOinit();
void LCD_init();
void LCD_printArrayInLcd(uint8_t *str, uint8_t size);
void LCD_ResetLCD();
void LCD_PulaCursorSegundaLinha();
void LCD_SetCursorPos(unsigned char ucPos);

void MKBOARD_GPIOinit();
unsigned char MKEYBOARD_readKeyboard();

///////// LOCAL FUNCTIONS DECLARATIONS //////////

/**
 * @brief Initializes all static variables
 */
static void initVars(void);

static void askWayOfMotorCtrl(void);

static void askMotorDirection(void);

/**
 * @brief Treats everything needed to rotate the motor with the keyboard and the potentiometer
 *
 * The keyboard is used to change the direction of rotation, while the potentiometer is used to
 * change the speed of rotation.
 * The speed is changed by changing the PWM duty cycle: adVal 0xFFF = 100% duty cycle,
 *                                                      adVal 0x000 = 0% duty cycle.
 *
 * @param pstDcMotorRotation Pointer to the DC_MotorRotation struct
 */
static void rotateWithPotentAndKeyboard(DC_MotorRotation *pstDcMotorRotation);

/**
 * @brief Treats everything needed to rotate the motor with the potentiometer only
 *
 * The potentiometer is used to change the speed and direction of rotation.
 *  - adVal [0x000, 0x7FF] = CLOCKWISE rotation with adVal 0x000 = 100% duty cycle,
 *                                                   adVal 0x7FF = 0% duty cycle.
 * - adVal [0x800, 0xFFF] = COUNTER_CLOCKWISE rotation with adVal 0x800 = 0% duty cycle,
 *                                                          adVal 0xFFF = 100% duty cycle.
 *
 * @param pstDcMotorRotation Pointer to the DC_MotorRotation struct
 */
static void rotateWithPotentOnly(DC_MotorRotation *pstDcMotorRotation);

/**
 * @brief Changes the direction of rotation of the motor
 *
 * It changes the direction of rotation with a slow decline ramp, so that the motor doesn't
 * change direction abruptly. It starts and ends with the same PWM duty cycle,
 * but changing directions.
 *
 * @param pstDcMotorRotation Pointer to the DC_MotorRotation struct
 */
static void changeMotorDirectionSwiftly(DC_MotorRotation *pstDcMotorRotation);

/**
 * @brief Limits the delta between the current ADC value and the previous one
 *
 * @param pstDcMotorRotation Pointer to the DC_MotorRotation struct
 * @param usReadAdcValue     Currently read ADC value
 */
static void changeAdcValueSwiftly(DC_MotorRotation *pstDcMotorRotation,
                          unsigned short usReadAdcValue);

/**
 * @brief Uses LCD_SetCursorPos to set the cursor position and then prints the duty cycle
 * 
 * @param ucDutyCycle A value from 0 to 100
 */
static void printDutyCycleVal(unsigned char ucDutyCycle);

/**
 * @brief Checks if the value in the structure is in the valid range
 * 
 * @param pstDcMotorRotation Pointer to the DC_MotorRotation struct
 */
static void checkStoredAdcValue(DC_MotorRotation *pstDcMotorRotation);

static void Pisca_leds(void);

///////// STATIC VARIABLES DECLARATIONS //////////
static SysState sysState;
static DC_MotorRotation dc_MotorRotation;
uint8_t       inicio[]        = {"Motor Parado!"};
uint8_t       controle[]      = {"Control: 0 | 1"};
uint8_t       potenciometro[] = {"Potenciometro"};
uint8_t       teclado[]       = {"Teclado"};
uint8_t       sentido[]       = {"Sentido: 0 | 1"};
uint8_t       horario[]       = {"Horario"};
uint8_t       antiHorario[]   = {"Anti-horario"};
unsigned char dutyCycleStr[]  = {"Duty Cycle: "};

///////// LOCAL FUNCTIONS IMPLEMENTATIONS //////////

unsigned long msg = INVALID_ADC_VALUE;

int main(void)
{
   PLL_Init();
   SysTick_Init();
   GPIO_Init();
   LCD_GPIOinit();
   LCD_init();
   MKBOARD_GPIOinit();
   timerInit();
   adc_adcInit();
   dcMotor_init();
   initVars();

   while (1)
   {
      switch (sysState)
      {
      case WAITING_CTRL_MODE:
         askWayOfMotorCtrl();
         break;
      case WAITING_DIRECTION:
         askMotorDirection();
         break;
      case ROTATING_POTENT_AND_KEY:
         rotateWithPotentAndKeyboard(&dc_MotorRotation);
         break;
      case ROTATING_POTENT_ONLY:
         rotateWithPotentOnly(&dc_MotorRotation);
         break;
      default:
         LCD_ResetLCD();
         break;
      }
   }
}

static void initVars(void)
{
   adc_stopAdc3Conversion();

   sysState = WAITING_CTRL_MODE;

   dc_MotorRotation.dcRotDirection = CLOCKWISE;
   dc_MotorRotation.dcRotType = KEYBOARD_AND_POTENT;
   dc_MotorRotation.bRunNow = false;
   dc_MotorRotation.dcAdVal = INVALID_ADC_VALUE;
   dc_MotorRotation.dcPwmPercent = 0;

   // Stops the motor
   dcMotor_rotateMotor(dc_MotorRotation.dcRotDirection, dc_MotorRotation.bRunNow);

   SysTick_Wait1ms(1000);
   LCD_ResetLCD();
   LCD_printArrayInLcd(inicio, 13);
   SysTick_Wait1ms(1000);
   TIMER2_TAILR_R = 40000; // COUNTER = (500us / (1/80MHz))
   TIMER2_ICR_R = 1; // ACKS the interruption
   TIMER2_CTL_R = 0; // Disables timer
   return;
}

static void askWayOfMotorCtrl(void)
{
   LCD_ResetLCD();
   LCD_printArrayInLcd(controle, 14);
   static char c;
   do
   {
      c = MKEYBOARD_readKeyboard();
   } while (c != '0' && c != '1');
   if (c == '0')
   {
      sysState = WAITING_DIRECTION;
      dc_MotorRotation.dcRotType = KEYBOARD_AND_POTENT;
      LCD_PulaCursorSegundaLinha();
      LCD_printArrayInLcd(teclado, 7);
      SysTick_Wait1ms(1000);
   }
   else if (c == '1')
   {
      sysState = ROTATING_POTENT_ONLY;
      dc_MotorRotation.dcRotType = POTENT_ONLY;
      LCD_PulaCursorSegundaLinha();
      LCD_printArrayInLcd(potenciometro, 13);
      SysTick_Wait1ms(1000);
      LCD_ResetLCD();
      LCD_PulaCursorSegundaLinha();
      LCD_printArrayInLcd(dutyCycleStr, 11);
      adc_startAdcConversion();
      TIMER2_CTL_R = 1; // Enables timer
      SysTick_Wait1ms(1000);
   }

   return;
}

static void askMotorDirection(void)
{
   LCD_ResetLCD();
   LCD_printArrayInLcd(sentido, 14);
   unsigned char c;
   do
   {
      c = MKEYBOARD_readKeyboard();
   } while (c != '0' && c != '1');

   if (c == '0')
   {
      dc_MotorRotation.dcRotDirection = CLOCKWISE;
      sysState = ROTATING_POTENT_AND_KEY;
      LCD_ResetLCD();
      LCD_printArrayInLcd(horario, 7);
      LCD_PulaCursorSegundaLinha();
      LCD_printArrayInLcd(dutyCycleStr, 11);
   }
   else if (c == '1')
   {
      dc_MotorRotation.dcRotDirection = COUNTER_CLOCKWISE;
      sysState = ROTATING_POTENT_AND_KEY;
      LCD_ResetLCD();
      LCD_printArrayInLcd(antiHorario, 12);
      LCD_PulaCursorSegundaLinha();
      LCD_printArrayInLcd(dutyCycleStr, 11);
   }

   SysTick_Wait1ms(1000);
   adc_startAdcConversion();
   TIMER2_CTL_R = 1; // Enables timer
   return;
}

static void rotateWithPotentAndKeyboard(DC_MotorRotation *pstDcMotorRotation)
{
   unsigned char ucRotDirection = (MKEYBOARD_readKeyboard() - '0');
   unsigned short usAdcValue = adc_readAdc3Value();

   checkStoredAdcValue(pstDcMotorRotation);

   if ((ucRotDirection <= COUNTER_CLOCKWISE) &&
      (ucRotDirection != pstDcMotorRotation->dcRotDirection))
   {
      changeMotorDirectionSwiftly(pstDcMotorRotation);

      if (pstDcMotorRotation->dcRotDirection == CLOCKWISE)
      {
         LCD_ResetLCD();
         LCD_printArrayInLcd(horario, 7);
      }
      else if (pstDcMotorRotation->dcRotDirection == COUNTER_CLOCKWISE)
      {
         LCD_ResetLCD();
         LCD_printArrayInLcd(antiHorario, 12);
      }

      LCD_PulaCursorSegundaLinha();
      LCD_printArrayInLcd(dutyCycleStr, 11);
   }

   if (INVALID_ADC_VALUE != usAdcValue)
   {
      changeAdcValueSwiftly(pstDcMotorRotation, usAdcValue);

      pstDcMotorRotation->dcPwmPercent = (pstDcMotorRotation->dcAdVal * 100) / MAX_ADC_VALUE;

      printDutyCycleVal(pstDcMotorRotation->dcPwmPercent);
   }

   return;
}

static void rotateWithPotentOnly(DC_MotorRotation *pstDcMotorRotation)
{
   unsigned short usAdcValue = adc_readAdc3Value();

   checkStoredAdcValue(pstDcMotorRotation);

   if (INVALID_ADC_VALUE != usAdcValue)
   {
      changeAdcValueSwiftly(pstDcMotorRotation, usAdcValue);

      if (usAdcValue <= MAX_CLOCKWISE_ADC_VAL)
      {
         if (pstDcMotorRotation->dcRotDirection != CLOCKWISE)
         {
            pstDcMotorRotation->dcRotDirection = CLOCKWISE;
            LCD_ResetLCD();
            LCD_printArrayInLcd(horario, 7);
            LCD_PulaCursorSegundaLinha();
            LCD_printArrayInLcd(dutyCycleStr, 11);
         }

         pstDcMotorRotation->dcPwmPercent = (((MAX_CLOCKWISE_ADC_VAL - pstDcMotorRotation->dcAdVal) * 100) /
                                 MAX_CLOCKWISE_ADC_VAL);
      }
      else
      {
         if (pstDcMotorRotation->dcRotDirection != COUNTER_CLOCKWISE)
         {
            pstDcMotorRotation->dcRotDirection = COUNTER_CLOCKWISE;
            LCD_ResetLCD();
            LCD_printArrayInLcd(antiHorario, 12);
            LCD_PulaCursorSegundaLinha();
            LCD_printArrayInLcd(dutyCycleStr, 11);
         }

         pstDcMotorRotation->dcPwmPercent = (((pstDcMotorRotation->dcAdVal - MAX_CLOCKWISE_ADC_VAL) * 100) /
                                    (MAX_ADC_VALUE - MAX_CLOCKWISE_ADC_VAL));
      }

      printDutyCycleVal(pstDcMotorRotation->dcPwmPercent);
   }

   return;
}

static void changeMotorDirectionSwiftly(DC_MotorRotation *pstDcMotorRotation)
{
   unsigned char ucInitialPwmPercent = pstDcMotorRotation->dcPwmPercent;

   for (unsigned char ucPwmPercent = ucInitialPwmPercent; ucPwmPercent > 0; ucPwmPercent--)
   {
      pstDcMotorRotation->dcPwmPercent = ucPwmPercent;

      SysTick_Wait1ms(100);
      printDutyCycleVal(pstDcMotorRotation->dcPwmPercent);
   }

   pstDcMotorRotation->dcRotDirection = (pstDcMotorRotation->dcRotDirection == CLOCKWISE) ? COUNTER_CLOCKWISE : CLOCKWISE;

   for (unsigned char ucPwmPercent = 0; ucPwmPercent < ucInitialPwmPercent; ucPwmPercent++)
   {
      pstDcMotorRotation->dcPwmPercent = ucPwmPercent;

      SysTick_Wait1ms(100);
      printDutyCycleVal(pstDcMotorRotation->dcPwmPercent);
   }

   return;
}

static void changeAdcValueSwiftly(DC_MotorRotation *pstDcMotorRotation,
                          unsigned short usReadAdcValue)
{
   if (((short)usReadAdcValue - (short)pstDcMotorRotation->dcAdVal) > MAX_DELTA_AD_VAL)
   {
      pstDcMotorRotation->dcAdVal += MAX_DELTA_AD_VAL;
   }
   else if (((short)usReadAdcValue - (short)pstDcMotorRotation->dcAdVal) < (-MAX_DELTA_AD_VAL))
   {
      pstDcMotorRotation->dcAdVal -= MAX_DELTA_AD_VAL;
   }
   else
   {
      pstDcMotorRotation->dcAdVal = usReadAdcValue;
   }

   return;
}

static void printDutyCycleVal(unsigned char ucDutyCycle)
{
   unsigned char cDutyCycle[3];
   cDutyCycle[0] = (ucDutyCycle / 100) + '0';
   cDutyCycle[1] = ((ucDutyCycle % 100) / 10) + '0';
   cDutyCycle[2] = (ucDutyCycle % 10) + '0';

   LCD_SetCursorPos(0xCB);
   LCD_printArrayInLcd(cDutyCycle, 3);

   return;
}

static void Pisca_leds(void)
{
   PortN_Output(0x2);
   SysTick_Wait1ms(250);
   PortN_Output(0x1);
   SysTick_Wait1ms(250);
}

static void checkStoredAdcValue(DC_MotorRotation *pstDcMotorRotation)
{
   if (pstDcMotorRotation->dcAdVal > MAX_ADC_VALUE)
   {
      pstDcMotorRotation->dcAdVal = MAX_ADC_VALUE;
   }

   return;

}

///////// HANDLERS IMPLEMENTATIONS //////////

void GPIOPortJ_Handler(void)
{
   GPIO_PORTJ_AHB_ICR_R = 0x3;

   if (sysState > WAITING_DIRECTION)
   {
      initVars();
   }

   return;
}

void Timer2A_Handler(void)
{
   TIMER2_ICR_R = 1; // ACKS the interruption

   if (true == dc_MotorRotation.bRunNow)
   {
      dc_MotorRotation.bRunNow = false;

      TIMER2_TAILR_R = TICKS_FOR_1MS - ((dc_MotorRotation.dcPwmPercent * TICKS_FOR_1MS) / 100);
   }
   else
   {
      dc_MotorRotation.bRunNow = true;

      TIMER2_TAILR_R = (dc_MotorRotation.dcPwmPercent * TICKS_FOR_1MS) / 100;
   }

   dcMotor_rotateMotor(dc_MotorRotation.dcRotDirection, dc_MotorRotation.bRunNow);

   TIMER2_CTL_R = 1; // Enables timer

   return;
}