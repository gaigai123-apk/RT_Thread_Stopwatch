#include <rtdevice.h>
#include "board.h"
#include "OLED.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>

/*
HAL 适配说明：
 - 使用 PB8 作为 SCL，PB9 作为 SDA，开漏上拉（外部上拉电阻或内部上拉）。
 - 通过GPIO位操作实现软件I2C，与原始接口保持一致：OLED_W_SCL / OLED_W_SDA。
*/

static void OLED_W_SCL(uint8_t BitValue);
static void OLED_W_SDA(uint8_t BitValue);
static void OLED_GPIO_Init(void);
static void OLED_I2C_Start(void);
static void OLED_I2C_Stop(void);
static void OLED_I2C_SendByte(uint8_t Byte);
static void OLED_WriteCommand(uint8_t Command);
static void OLED_WriteData(uint8_t *Data, uint8_t Count);
static void OLED_SetCursor(uint8_t Page, uint8_t X);
void OLED_ShowImage(int16_t X, int16_t Y, uint8_t Width, uint8_t Height, const uint8_t *Image);

uint8_t OLED_DisplayBuf[8][128];

#ifndef OLED_SCL_PIN
#define OLED_SCL_PIN    GET_PIN(B, 8)
#endif
#ifndef OLED_SDA_PIN
#define OLED_SDA_PIN    GET_PIN(B, 9)
#endif

static void OLED_W_SCL(uint8_t BitValue)
{
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, BitValue ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void OLED_W_SDA(uint8_t BitValue)
{
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, BitValue ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void OLED_GPIO_Init(void)
{
	uint32_t i, j;
	for (i = 0; i < 1000; i ++)
	{
		for (j = 0; j < 1000; j ++);
	}

	__HAL_RCC_GPIOB_CLK_ENABLE();
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_9;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;  /* 开漏输出 */
	GPIO_InitStruct.Pull = GPIO_PULLUP;          /* 使能上拉 */
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	OLED_W_SCL(1);
	OLED_W_SDA(1);
}

static void i2c_delay(void)
{
    for (volatile int i = 0; i < 30; i++) __NOP();
}

static void OLED_I2C_Start(void)
{
    OLED_W_SDA(1); i2c_delay();
    OLED_W_SCL(1); i2c_delay();
    OLED_W_SDA(0); i2c_delay();
    OLED_W_SCL(0); i2c_delay();
}

static void OLED_I2C_Stop(void)
{
    OLED_W_SDA(0); i2c_delay();
    OLED_W_SCL(1); i2c_delay();
    OLED_W_SDA(1); i2c_delay();
}

static void OLED_I2C_SendByte(uint8_t Byte)
{
    uint8_t i;
    for (i = 0; i < 8; i++)
    {
        OLED_W_SDA(!!(Byte & (0x80 >> i))); i2c_delay();
        OLED_W_SCL(1); i2c_delay();
        OLED_W_SCL(0); i2c_delay();
    }
    /* ACK 时序简单延时跳过读取 */
    OLED_W_SDA(1); i2c_delay();
    OLED_W_SCL(1); i2c_delay();
    OLED_W_SCL(0); i2c_delay();
}

static uint8_t s_oled_addr = 0x78; /* 0x3C<<1 默认 */

void OLED_SetI2CAddress(uint8_t addr7bit_left_shifted)
{
    s_oled_addr = addr7bit_left_shifted;
}

static void OLED_WriteCommand(uint8_t Command)
{
	OLED_I2C_Start();
    OLED_I2C_SendByte(s_oled_addr);
	OLED_I2C_SendByte(0x00);
	OLED_I2C_SendByte(Command);
	OLED_I2C_Stop();
}

static void OLED_WriteData(uint8_t *Data, uint8_t Count)
{
	uint8_t i;
	OLED_I2C_Start();
    OLED_I2C_SendByte(s_oled_addr);
	OLED_I2C_SendByte(0x40);
	for (i = 0; i < Count; i ++)
	{
		OLED_I2C_SendByte(Data[i]);
	}
	OLED_I2C_Stop();
}

void OLED_Init(void)
{
    OLED_GPIO_Init();
    /* 上电稳定等待 */
    for (volatile int i = 0; i < 720000; i++) __NOP(); /* ~10ms@72MHz */
	OLED_WriteCommand(0xAE);
	OLED_WriteCommand(0xD5);
	OLED_WriteCommand(0x80);
	OLED_WriteCommand(0xA8);
	OLED_WriteCommand(0x3F);
	OLED_WriteCommand(0xD3);
	OLED_WriteCommand(0x00);
	OLED_WriteCommand(0x40);
	OLED_WriteCommand(0xA1);
	OLED_WriteCommand(0xC8);
	OLED_WriteCommand(0xDA);
	OLED_WriteCommand(0x12);
    OLED_WriteCommand(0x81);
    OLED_WriteCommand(0xFF); /* 对比度拉满，便于确认亮屏 */
	OLED_WriteCommand(0xD9);
	OLED_WriteCommand(0xF1);
	OLED_WriteCommand(0xDB);
	OLED_WriteCommand(0x30);
	OLED_WriteCommand(0xA4);
	OLED_WriteCommand(0xA6);
	OLED_WriteCommand(0x8D);
	OLED_WriteCommand(0x14);
    OLED_WriteCommand(0xAF);
	OLED_Clear();
	OLED_Update();
}

static void OLED_SetCursor(uint8_t Page, uint8_t X)
{
	OLED_WriteCommand(0xB0 | Page);
	OLED_WriteCommand(0x10 | ((X & 0xF0) >> 4));
	OLED_WriteCommand(0x00 | (X & 0x0F));
}

void OLED_Update(void)
{
	uint8_t j;
	for (j = 0; j < 8; j ++)
	{
		OLED_SetCursor(j, 0);
		OLED_WriteData(OLED_DisplayBuf[j], 128);
	}
}

void OLED_UpdateArea(int16_t X, int16_t Y, uint8_t Width, uint8_t Height)
{
	int16_t j;
	int16_t Page, Page1;
	Page = Y / 8;
	Page1 = (Y + Height - 1) / 8 + 1;
	if (Y < 0)
	{
		Page -= 1;
		Page1 -= 1;
	}
	for (j = Page; j < Page1; j ++)
	{
		if (X >= 0 && X <= 127 && j >= 0 && j <= 7)
		{
			OLED_SetCursor(j, (uint8_t)X);
			OLED_WriteData(&OLED_DisplayBuf[j][X], Width);
		}
	}
}

void OLED_Clear(void)
{
	uint8_t i, j;
	for (j = 0; j < 8; j ++)
	{
		for (i = 0; i < 128; i ++)
		{
			OLED_DisplayBuf[j][i] = 0x00;
		}
	}
}

void OLED_ClearArea(int16_t X, int16_t Y, uint8_t Width, uint8_t Height)
{
	int16_t i, j;
	for (j = Y; j < Y + Height; j ++)
	{
		for (i = X; i < X + Width; i ++)
		{
			if (i >= 0 && i <= 127 && j >=0 && j <= 63)
			{
				OLED_DisplayBuf[j / 8][i] &= ~(0x01 << (j % 8));
			}
		}
	}
}

/* Cut unused effects/number helpers to save ROM */
/* void OLED_Reverse(void) { } */
/* void OLED_ReverseArea(int16_t X, int16_t Y, uint8_t Width, uint8_t Height) { } */
/* static uint32_t OLED_Pow(uint32_t X, uint32_t Y) { return 1; } */

void OLED_ShowChar(int16_t X, int16_t Y, char Char, uint8_t FontSize)
{
    if (FontSize == OLED_8X16)
    {
#if 0
        OLED_ShowImage(X, Y, 8, 16, OLED_F8x16[(uint8_t)Char - ' ']);
#else
        /* 若未编译 8x16 字模，退化为 6x8 显示 */
        OLED_ShowImage(X, Y, 6, 8, OLED_F6x8[(uint8_t)Char - ' ']);
#endif
    }
    else if(FontSize == OLED_6X8)
    {
        OLED_ShowImage(X, Y, 6, 8, OLED_F6x8[(uint8_t)Char - ' ']);
    }
}

void OLED_ShowString(int16_t X, int16_t Y, char *String, uint8_t FontSize)
{
	uint16_t i = 0;
	char SingleChar[5];
	uint8_t CharLength = 0;
	uint16_t XOffset = 0;
	while (String[i] != '\0')
	{
#ifdef OLED_CHARSET_UTF8
		if ((String[i] & 0x80) == 0x00)
		{
			CharLength = 1;
			SingleChar[0] = String[i ++];
			SingleChar[1] = '\0';
		}
		else if ((String[i] & 0xE0) == 0xC0)
		{
			CharLength = 2;
			SingleChar[0] = String[i ++];
			if (String[i] == '\0') {break;}
			SingleChar[1] = String[i ++];
			SingleChar[2] = '\0';
		}
		else if ((String[i] & 0xF0) == 0xE0)
		{
			CharLength = 3;
			SingleChar[0] = String[i ++];
			if (String[i] == '\0') {break;}
			SingleChar[1] = String[i ++];
			if (String[i] == '\0') {break;}
			SingleChar[2] = String[i ++];
			SingleChar[3] = '\0';
		}
		else if ((String[i] & 0xF8) == 0xF0)
		{
			CharLength = 4;
			SingleChar[0] = String[i ++];
			if (String[i] == '\0') {break;}
			SingleChar[1] = String[i ++];
			if (String[i] == '\0') {break;}
			SingleChar[2] = String[i ++];
			if (String[i] == '\0') {break;}
			SingleChar[3] = String[i ++];
			SingleChar[4] = '\0';
		}
		else
		{
			i ++;
			continue;
		}
#else
        /* 非 UTF8 模式：逐字符 ASCII 渲染 */
        CharLength = 1;
        SingleChar[0] = String[i ++];
        SingleChar[1] = '\0';
#endif

		if (CharLength == 1)
		{
			OLED_ShowChar(X + XOffset, Y, SingleChar[0], FontSize);
			XOffset += FontSize;
		}
		else
		{
            /* 为减小固件体积，UTF8 汉字查表默认关闭，直接用 '?' 代替 */
            OLED_ShowChar(X + XOffset, Y, '?', FontSize == OLED_8X16 ? OLED_8X16 : OLED_6X8);
            XOffset += (FontSize == OLED_8X16 ? 16 : OLED_6X8);
		}
	}
}

/* void OLED_ShowNum(...) { } */
/* void OLED_ShowSignedNum(...) { } */
/* void OLED_ShowHexNum(...) { } */
/* void OLED_ShowBinNum(...) { } */

#if 0 /* Reduce firmware size: disable floating drawing */
void OLED_ShowFloatNum(int16_t X, int16_t Y, double Number, uint8_t IntLength, uint8_t FraLength, uint8_t FontSize)
{
}
#endif

void OLED_ShowImage(int16_t X, int16_t Y, uint8_t Width, uint8_t Height, const uint8_t *Image)
{
	uint8_t i = 0, j = 0;
	int16_t Page, Shift;
	OLED_ClearArea(X, Y, Width, Height);
	for (j = 0; j < (Height - 1) / 8 + 1; j ++)
	{
		for (i = 0; i < Width; i ++)
		{
			if (X + i >= 0 && X + i <= 127)
			{
				Page = Y / 8;
				Shift = Y % 8;
				if (Y < 0)
				{
					Page -= 1;
					Shift += 8;
				}
				if (Page + j >= 0 && Page + j <= 7)
				{
					OLED_DisplayBuf[Page + j][X + i] |= Image[j * Width + i] << (Shift);
				}
				if (Page + j + 1 >= 0 && Page + j + 1 <= 7)
				{
					OLED_DisplayBuf[Page + j + 1][X + i] |= Image[j * Width + i] >> (8 - Shift);
				}
			}
		}
	}
}

void OLED_DrawPoint(int16_t X, int16_t Y)
{
	if (X >= 0 && X <= 127 && Y >=0 && Y <= 63)
	{
		OLED_DisplayBuf[Y / 8][X] |= 0x01 << (Y % 8);
	}
}

uint8_t OLED_GetPoint(int16_t X, int16_t Y)
{
	if (X >= 0 && X <= 127 && Y >=0 && Y <= 63)
	{
		if (OLED_DisplayBuf[Y / 8][X] & 0x01 << (Y % 8))
		{
			return 1;
		}
	}
	return 0;
}

/* void OLED_DrawLine(...) { } */

/* void OLED_DrawRectangle(...) { } */

/* void OLED_DrawTriangle(...) { } */

/* void OLED_DrawCircle(...) { } */

#if 0 /* Reduce firmware size: disable ellipse drawing */
void OLED_DrawEllipse(int16_t X, int16_t Y, uint8_t A, uint8_t B, uint8_t IsFilled)
{
}
#endif

#if 0
static uint8_t OLED_IsInAngle_Helper(int16_t px, int16_t py, int16_t StartAngle, int16_t EndAngle)
{
    return 0;
}

void OLED_DrawArc(int16_t X, int16_t Y, uint8_t Radius, int16_t StartAngle, int16_t EndAngle, uint8_t IsFilled)
{
}
#endif



