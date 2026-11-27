#include "oled.h"

#include "main.h"

#include <stddef.h>

#define OLED_I2C_ADDRESS 0x78U
#define OLED_WIDTH       128U

typedef struct
{
  char character;
  uint8_t columns[5];
} oled_glyph_t;

static const oled_glyph_t OLED_FONT[] = {
  {' ', {0x00, 0x00, 0x00, 0x00, 0x00}},
  {'0', {0x3E, 0x51, 0x49, 0x45, 0x3E}},
  {'1', {0x00, 0x42, 0x7F, 0x40, 0x00}},
  {'2', {0x42, 0x61, 0x51, 0x49, 0x46}},
  {'3', {0x21, 0x41, 0x45, 0x4B, 0x31}},
  {'4', {0x18, 0x14, 0x12, 0x7F, 0x10}},
  {'5', {0x27, 0x45, 0x45, 0x45, 0x39}},
  {'6', {0x3C, 0x4A, 0x49, 0x49, 0x30}},
  {'7', {0x01, 0x71, 0x09, 0x05, 0x03}},
  {'8', {0x36, 0x49, 0x49, 0x49, 0x36}},
  {'9', {0x06, 0x49, 0x49, 0x29, 0x1E}},
  {'A', {0x7E, 0x11, 0x11, 0x11, 0x7E}},
  {'B', {0x7F, 0x49, 0x49, 0x49, 0x36}},
  {'C', {0x3E, 0x41, 0x41, 0x41, 0x22}},
  {'D', {0x7F, 0x41, 0x41, 0x22, 0x1C}},
  {'E', {0x7F, 0x49, 0x49, 0x49, 0x41}},
  {'F', {0x7F, 0x09, 0x09, 0x09, 0x01}},
  {'G', {0x3E, 0x41, 0x49, 0x49, 0x7A}},
  {'H', {0x7F, 0x08, 0x08, 0x08, 0x7F}},
  {'I', {0x00, 0x41, 0x7F, 0x41, 0x00}},
  {'J', {0x20, 0x40, 0x41, 0x3F, 0x01}},
  {'K', {0x7F, 0x08, 0x14, 0x22, 0x41}},
  {'L', {0x7F, 0x40, 0x40, 0x40, 0x40}},
  {'M', {0x7F, 0x02, 0x0C, 0x02, 0x7F}},
  {'N', {0x7F, 0x04, 0x08, 0x10, 0x7F}},
  {'O', {0x3E, 0x41, 0x41, 0x41, 0x3E}},
  {'P', {0x7F, 0x09, 0x09, 0x09, 0x06}},
  {'Q', {0x3E, 0x41, 0x51, 0x21, 0x5E}},
  {'R', {0x7F, 0x09, 0x19, 0x29, 0x46}},
  {'S', {0x46, 0x49, 0x49, 0x49, 0x31}},
  {'T', {0x01, 0x01, 0x7F, 0x01, 0x01}},
  {'U', {0x3F, 0x40, 0x40, 0x40, 0x3F}},
  {'V', {0x1F, 0x20, 0x40, 0x20, 0x1F}},
  {'W', {0x7F, 0x20, 0x18, 0x20, 0x7F}},
  {'X', {0x63, 0x14, 0x08, 0x14, 0x63}},
  {'Y', {0x07, 0x08, 0x70, 0x08, 0x07}},
  {'Z', {0x61, 0x51, 0x49, 0x45, 0x43}},
  {':', {0x00, 0x36, 0x36, 0x00, 0x00}},
  {'=', {0x14, 0x14, 0x14, 0x14, 0x14}},
  {',', {0x00, 0x50, 0x30, 0x00, 0x00}},
  {'-', {0x08, 0x08, 0x08, 0x08, 0x08}},
  {'m', {0x7C, 0x04, 0x18, 0x04, 0x78}},
  {'s', {0x48, 0x54, 0x54, 0x54, 0x24}},
};

static bool s_present;

static void i2c_delay(void)
{
  for (volatile uint32_t i = 0U; i < 80U; ++i)
  {
    __NOP();
  }
}

static void scl_write(GPIO_PinState state)
{
  HAL_GPIO_WritePin(OLED_SCL_GPIO_Port, OLED_SCL_Pin, state);
}

static void sda_write(GPIO_PinState state)
{
  HAL_GPIO_WritePin(OLED_SDA_GPIO_Port, OLED_SDA_Pin, state);
}

static void i2c_start(void)
{
  sda_write(GPIO_PIN_SET);
  i2c_delay();
  scl_write(GPIO_PIN_SET);
  i2c_delay();
  sda_write(GPIO_PIN_RESET);
  i2c_delay();
  scl_write(GPIO_PIN_RESET);
  i2c_delay();
}

static void i2c_stop(void)
{
  sda_write(GPIO_PIN_RESET);
  i2c_delay();
  scl_write(GPIO_PIN_SET);
  i2c_delay();
  sda_write(GPIO_PIN_SET);
  i2c_delay();
}

static bool i2c_write_byte(uint8_t byte)
{
  bool acknowledged;

  for (uint8_t bit = 0U; bit < 8U; ++bit)
  {
    sda_write((byte & 0x80U) != 0U ? GPIO_PIN_SET : GPIO_PIN_RESET);
    byte <<= 1U;
    i2c_delay();
    scl_write(GPIO_PIN_SET);
    i2c_delay();
    scl_write(GPIO_PIN_RESET);
    i2c_delay();
  }

  sda_write(GPIO_PIN_SET);
  i2c_delay();
  scl_write(GPIO_PIN_SET);
  i2c_delay();
  acknowledged = HAL_GPIO_ReadPin(OLED_SDA_GPIO_Port, OLED_SDA_Pin) == GPIO_PIN_RESET;
  scl_write(GPIO_PIN_RESET);
  i2c_delay();

  return acknowledged;
}

static bool i2c_write_block(uint8_t control, const uint8_t *data, size_t length)
{
  if (!s_present)
  {
    return false;
  }

  i2c_start();
  if (!i2c_write_byte(OLED_I2C_ADDRESS) ||
      !i2c_write_byte(control))
  {
    i2c_stop();
    s_present = false;
    return false;
  }

  for (size_t i = 0U; i < length; ++i)
  {
    if (!i2c_write_byte(data[i]))
    {
      i2c_stop();
      s_present = false;
      return false;
    }
  }

  i2c_stop();
  return true;
}

static const uint8_t *glyph_for(char character)
{
  for (size_t i = 0U; i < sizeof(OLED_FONT) / sizeof(OLED_FONT[0]); ++i)
  {
    if (OLED_FONT[i].character == character)
    {
      return OLED_FONT[i].columns;
    }
  }

  return OLED_FONT[0].columns;
}

bool OLED_Init(void)
{
  static const uint8_t init_commands[] = {
    0xAE, 0x20, 0x00, 0xB0, 0xC8, 0x00, 0x10, 0x40,
    0x81, 0xCF, 0xA1, 0xA6, 0xA8, 0x3F, 0xA4, 0xD3,
    0x00, 0xD5, 0x80, 0xD9, 0xF1, 0xDA, 0x12, 0xDB,
    0x40, 0x8D, 0x14, 0xAF
  };

  s_present = true;
  scl_write(GPIO_PIN_SET);
  sda_write(GPIO_PIN_SET);
  HAL_Delay(100U);

  if (!i2c_write_block(0x00U, init_commands, sizeof(init_commands)))
  {
    return false;
  }

  OLED_Clear();
  return s_present;
}

void OLED_Clear(void)
{
  static const uint8_t zeros[32] = {0};

  for (uint8_t page = 0U; page < 8U; ++page)
  {
    uint8_t cursor[3] = {(uint8_t)(0xB0U | page), 0x00U, 0x10U};
    if (!i2c_write_block(0x00U, cursor, sizeof(cursor)))
    {
      return;
    }

    for (uint8_t chunk = 0U; chunk < 4U; ++chunk)
    {
      if (!i2c_write_block(0x40U, zeros, sizeof(zeros)))
      {
        return;
      }
    }
  }
}

void OLED_PrintLine(uint8_t page, const char *text)
{
  uint8_t pixels[OLED_WIDTH];

  if (!s_present || page >= 8U || text == NULL)
  {
    return;
  }

  for (size_t i = 0U; i < sizeof(pixels); ++i)
  {
    pixels[i] = 0U;
  }

  size_t column = 0U;
  while (*text != '\0' && column + 6U <= sizeof(pixels))
  {
    const uint8_t *glyph = glyph_for(*text++);
    for (uint8_t i = 0U; i < 5U; ++i)
    {
      pixels[column++] = glyph[i];
    }
    pixels[column++] = 0x00U;
  }

  uint8_t cursor[3] = {(uint8_t)(0xB0U | page), 0x00U, 0x10U};
  if (i2c_write_block(0x00U, cursor, sizeof(cursor)))
  {
    (void)i2c_write_block(0x40U, pixels, sizeof(pixels));
  }
}
