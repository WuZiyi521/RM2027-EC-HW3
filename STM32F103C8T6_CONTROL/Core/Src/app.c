#include "app.h"

#include "main.h"
#include "oled.h"
#include "usart.h"

#include <stdbool.h>
#include <stdio.h>

#define LED_COUNT          3U
#define DEFAULT_STEP_MS    500U
#define MIN_STEP_MS        100U
#define MAX_STEP_MS        2000U
#define KEY_DEBOUNCE_MS    20U
#define RX_RING_SIZE       64U
#define RX_RING_MASK       (RX_RING_SIZE - 1U)

typedef struct
{
  GPIO_TypeDef *port;
  uint16_t pin;
} led_t;

typedef struct
{
  GPIO_TypeDef *port;
  uint16_t pin;
  bool raw_pressed;
  bool stable_pressed;
  uint32_t raw_changed_tick;
} button_t;

typedef struct
{
  bool running;
  bool reverse;
  uint16_t step_ms;
  uint8_t led_index;
  uint32_t switch_tick;
} flow_state_t;

typedef struct __attribute__((packed))
{
  uint8_t header;
  uint8_t run;
  uint8_t direction;
  uint8_t step_low;
  uint8_t step_high;
  uint8_t check;
} serial_packet_fields_t;

typedef union
{
  serial_packet_fields_t fields;
  uint8_t bytes[6];
} serial_packet_t;

_Static_assert(sizeof(serial_packet_fields_t) == 6U, "Serial packet must contain six bytes");

static const led_t LEDS[LED_COUNT] = {
  {LED0_GPIO_Port, LED0_Pin},
  {LED1_GPIO_Port, LED1_Pin},
  {LED2_GPIO_Port, LED2_Pin},
};

static flow_state_t s_state = {
  true,
  false,
  DEFAULT_STEP_MS,
  0U,
  0U,
};

static button_t s_direction_button = {
  KEY_DIR_GPIO_Port,
  KEY_DIR_Pin,
  false,
  false,
  0U,
};

static button_t s_run_button = {
  KEY_RUN_GPIO_Port,
  KEY_RUN_Pin,
  false,
  false,
  0U,
};

static uint8_t s_uart_rx_byte;
static volatile uint8_t s_rx_ring[RX_RING_SIZE];
static volatile uint8_t s_rx_head;
static volatile uint8_t s_rx_tail;

static serial_packet_t s_rx_packet;
static uint8_t s_rx_position;

static bool s_display_dirty;
static volatile bool s_tx_busy;
static volatile bool s_status_dirty;
static uint8_t s_tx_buffer[80];

static void leds_all_off(void)
{
  for (uint8_t i = 0U; i < LED_COUNT; ++i)
  {
    HAL_GPIO_WritePin(LEDS[i].port, LEDS[i].pin, GPIO_PIN_RESET);
  }
}

static void led_apply_current(void)
{
  leds_all_off();
  HAL_GPIO_WritePin(LEDS[s_state.led_index].port, LEDS[s_state.led_index].pin, GPIO_PIN_SET);
}

static bool button_update(button_t *button, uint32_t now)
{
  const bool pressed = HAL_GPIO_ReadPin(button->port, button->pin) == GPIO_PIN_RESET;

  if (pressed != button->raw_pressed)
  {
    button->raw_pressed = pressed;
    button->raw_changed_tick = now;
  }

  if ((uint32_t)(now - button->raw_changed_tick) >= KEY_DEBOUNCE_MS &&
      button->stable_pressed != button->raw_pressed)
  {
    button->stable_pressed = button->raw_pressed;
    return button->stable_pressed;
  }

  return false;
}

static void control_changed(uint32_t now)
{
  s_state.switch_tick = now;
  s_display_dirty = true;
  s_status_dirty = true;
}

static void scan_buttons(uint32_t now)
{
  if (button_update(&s_direction_button, now))
  {
    s_state.reverse = !s_state.reverse;
    control_changed(now);
  }

  if (button_update(&s_run_button, now))
  {
    s_state.running = !s_state.running;
    control_changed(now);
  }
}

static void update_animation(uint32_t now)
{
  if (!s_state.running)
  {
    return;
  }

  if ((uint32_t)(now - s_state.switch_tick) >= s_state.step_ms)
  {
    s_state.switch_tick += s_state.step_ms;
    if (s_state.reverse)
    {
      s_state.led_index = (uint8_t)((s_state.led_index + LED_COUNT - 1U) % LED_COUNT);
    }
    else
    {
      s_state.led_index = (uint8_t)((s_state.led_index + 1U) % LED_COUNT);
    }

    led_apply_current();
    s_display_dirty = true;
  }
}

static void render_display(void)
{
  char line[24];

  if (!s_display_dirty)
  {
    return;
  }

  OLED_Clear();
  snprintf(line, sizeof(line), "STATE: %s", s_state.running ? "RUN" : "PAUSE");
  OLED_PrintLine(0U, line);
  snprintf(line, sizeof(line), "DIR: %s", s_state.reverse ? "REV" : "FWD");
  OLED_PrintLine(2U, line);
  snprintf(line, sizeof(line), "STEP: %ums", (unsigned int)s_state.step_ms);
  OLED_PrintLine(4U, line);
  snprintf(line, sizeof(line), "LED: %u", (unsigned int)(s_state.led_index + 1U));
  OLED_PrintLine(6U, line);
  s_display_dirty = false;
}

static void transmit_status_if_needed(void)
{
  if (s_tx_busy || !s_status_dirty)
  {
    return;
  }

  const int length = snprintf(
      (char *)s_tx_buffer,
      sizeof(s_tx_buffer),
      "STATE=%s,DIR=%s,STEP=%u,LED=%u\r\n",
      s_state.running ? "RUN" : "PAUSE",
      s_state.reverse ? "REV" : "FWD",
      (unsigned int)s_state.step_ms,
      (unsigned int)(s_state.led_index + 1U));

  if (length > 0 && (size_t)length < sizeof(s_tx_buffer))
  {
    if (HAL_UART_Transmit_IT(&huart1, s_tx_buffer, (uint16_t)length) == HAL_OK)
    {
      s_tx_busy = true;
      s_status_dirty = false;
    }
  }
}

static bool packet_is_valid(const serial_packet_fields_t *packet)
{
  const uint16_t step = (uint16_t)packet->step_low | ((uint16_t)packet->step_high << 8U);
  const uint8_t check = (uint8_t)(
      packet->header + packet->run + packet->direction + packet->step_low + packet->step_high);

  return packet->header == 0xAAU &&
         packet->run <= 1U &&
         packet->direction <= 1U &&
         step >= MIN_STEP_MS &&
         step <= MAX_STEP_MS &&
         packet->check == check;
}

static void apply_packet(uint32_t now)
{
  const serial_packet_fields_t *packet = &s_rx_packet.fields;

  s_state.running = packet->run == 1U;
  s_state.reverse = packet->direction == 1U;
  s_state.step_ms = (uint16_t)packet->step_low | ((uint16_t)packet->step_high << 8U);
  control_changed(now);
}

static void feed_packet_byte(uint8_t byte, uint32_t now)
{
  if (s_rx_position == 0U)
  {
    if (byte == 0xAAU)
    {
      s_rx_packet.bytes[0] = byte;
      s_rx_position = 1U;
    }
    return;
  }

  s_rx_packet.bytes[s_rx_position++] = byte;
  if (s_rx_position == sizeof(s_rx_packet.bytes))
  {
    if (packet_is_valid(&s_rx_packet.fields))
    {
      apply_packet(now);
    }
    s_rx_position = 0U;
  }
}

static void process_serial_bytes(void)
{
  while (s_rx_tail != s_rx_head)
  {
    const uint8_t byte = s_rx_ring[s_rx_tail];
    s_rx_tail = (uint8_t)((s_rx_tail + 1U) & RX_RING_MASK);
    feed_packet_byte(byte, HAL_GetTick());
  }
}

void App_Init(void)
{
  const uint32_t now = HAL_GetTick();

  s_state.running = true;
  s_state.reverse = false;
  s_state.step_ms = DEFAULT_STEP_MS;
  s_state.led_index = 0U;
  s_state.switch_tick = now;

  s_rx_head = 0U;
  s_rx_tail = 0U;
  s_rx_position = 0U;
  s_tx_busy = false;
  s_status_dirty = true;
  s_display_dirty = true;

  s_direction_button.raw_pressed = false;
  s_direction_button.stable_pressed = false;
  s_direction_button.raw_changed_tick = now;
  s_run_button.raw_pressed = false;
  s_run_button.stable_pressed = false;
  s_run_button.raw_changed_tick = now;

  led_apply_current();
  (void)OLED_Init();
  render_display();
  (void)HAL_UART_Receive_IT(&huart1, &s_uart_rx_byte, 1U);
}

void App_Task(void)
{
  const uint32_t now = HAL_GetTick();

  process_serial_bytes();
  scan_buttons(now);
  update_animation(now);
  render_display();
  transmit_status_if_needed();
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    const uint8_t next_head = (uint8_t)((s_rx_head + 1U) & RX_RING_MASK);
    if (next_head != s_rx_tail)
    {
      s_rx_ring[s_rx_head] = s_uart_rx_byte;
      s_rx_head = next_head;
    }
    (void)HAL_UART_Receive_IT(&huart1, &s_uart_rx_byte, 1U);
  }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    s_tx_busy = false;
  }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    __HAL_UART_CLEAR_OREFLAG(huart);
    (void)HAL_UART_Receive_IT(&huart1, &s_uart_rx_byte, 1U);
  }
}
