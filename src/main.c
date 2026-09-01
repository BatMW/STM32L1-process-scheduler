#include "main.h"
#include "process.h"
#include "core_cm3.h"
#include "stm32l1xx_hal_gpio.h"



static void MX_GPIO_Init(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // LED pins PB6 and PB7
    GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    // User button on PA0
    GPIO_InitStruct.Pin = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

struct Blink_Led_Args{
    uint32_t ticks_sleep;
    uint16_t pin;
};

void* blink_led(void* args){
    struct Blink_Led_Args* a = (struct Blink_Led_Args*)args;
    while(1){
        HAL_GPIO_TogglePin(GPIOB,a->pin);
        sleep_ticks(a->ticks_sleep);
    }
    return NULL;
}


void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
    RCC_OscInitStruct.MSIState = RCC_MSI_ON;
    RCC_OscInitStruct.MSICalibrationValue = 0;
    RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6; // ~4 MHz
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_MSI;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
    {
        Error_Handler();
    }
}

#define SLEEP_TICK 1000

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    process_scheduler_init();

    struct Blink_Led_Args args = {.ticks_sleep = SLEEP_TICK, .pin = GPIO_PIN_6};
    Process_form form = {
    .func = blink_led,
    .priority = 3,
    .process_memory_allocator = ALLOC_M,
    .args = &args,
    .ret = NULL
    };
    int32_t status = exec(&form);

    if(status == -1){
        while(1){}
    }

    struct Blink_Led_Args args2 = args;
    Process_form form2 = form;
    args2.pin = GPIO_PIN_7;
    args2.ticks_sleep = SLEEP_TICK/2; // Blink twice as fast
    form2.args = (void*)&args2;
    status = exec(&form2);

    if(status == -1){
        while(1){}
    }
    process_scheduler_start();
    while (1)
    {
        // MCU sleeps until interrupt
        __WFI(); // Wait for interrupt (low power mode)
    }
    Error_Handler();
}


void Error_Handler(void)
{
    __disable_irq();
    while (1) {}
}
