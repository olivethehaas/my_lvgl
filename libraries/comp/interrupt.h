#ifndef INTERRUPT_H
#define INTERRUPT_H

#define maxIRQs 15
#define GPIOCNT 30

typedef enum {
  LOW     = 0,
  HIGH    = 1,
  CHANGE  = 2,
  FALLING = 3,
  RISING  = 4,
} PinStatus;

typedef void (*voidFuncPtr)(void);
typedef void (*voidFuncPtrParam)(void*);


static uint32_t _irqStackTop[2] = { 0, 0 };
static uint32_t _irqStack[2][maxIRQs];

void attachInterrupt(uint pin, voidFuncPtr callback, PinStatus mode);
void attachInterruptParam(uint pin, voidFuncPtrParam callback, PinStatus mode, void *param);
void detachInterrupt(uint pin);

#endif



