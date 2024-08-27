#ifndef C_BINDINGS_LINK_GPIO_H
#define C_BINDINGS_LINK_GPIO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <tonc_core.h>

typedef void* C_LinkGPIOHandle;

typedef enum {
  C_LINK_GPIO_PIN_SI,
  C_LINK_GPIO_PIN_SO,
  C_LINK_GPIO_PIN_SD,
  C_LINK_GPIO_PIN_SC
} C_LinkGPIO_Pin;

typedef enum {
  C_LINK_GPIO_DIRECTION_INPUT,
  C_LINK_GPIO_DIRECTION_OUTPUT
} C_LinkGPIO_Direction;

C_LinkGPIOHandle C_LinkGPIO_create();
void C_LinkGPIO_destroy(C_LinkGPIOHandle handle);

void C_LinkGPIO_reset(C_LinkGPIOHandle handle);
void C_LinkGPIO_setMode(C_LinkGPIOHandle handle,
                        C_LinkGPIO_Pin pin,
                        C_LinkGPIO_Direction direction);
C_LinkGPIO_Direction C_LinkGPIO_getMode(C_LinkGPIOHandle handle,
                                        C_LinkGPIO_Pin pin);

bool C_LinkGPIO_readPin(C_LinkGPIOHandle handle, C_LinkGPIO_Pin pin);
void C_LinkGPIO_writePin(C_LinkGPIOHandle handle,
                         C_LinkGPIO_Pin pin,
                         bool isHigh);

void C_LinkGPIO_setSIInterrupts(C_LinkGPIOHandle handle, bool isEnabled);

extern C_LinkGPIOHandle cLinkGPIO;

#ifdef __cplusplus
}
#endif

#endif  // C_BINDINGS_LINK_GPIO_H
