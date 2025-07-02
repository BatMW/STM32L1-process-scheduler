#include "main.h"
#include "process.h"

int main(void)
{
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
