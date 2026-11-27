#ifndef OLED_H
#define OLED_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

bool OLED_Init(void);
void OLED_Clear(void);
void OLED_PrintLine(uint8_t page, const char *text);

#ifdef __cplusplus
}
#endif

#endif /* OLED_H */
