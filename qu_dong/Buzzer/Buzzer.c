#if 0 /* Reference driver disabled from build for RT-Thread project */
#include "stm32f10x.h"                  // Device header

void Buzzer_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
	
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	
	GPIO_SetBits(GPIOB, GPIO_Pin_12);
}
#endif /* Reference driver disabled */
#include <stdint.h>

void Buzzer_Init(void)
{
}

void Buzzer_ON(void)
{
}

void Buzzer_OFF(void)
{
}

void Buzzer_Turn(void)
{
}
