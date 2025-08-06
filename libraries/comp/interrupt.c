#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "interrupt.h"
// Support nested IRQ disable/re-enable


void interrupts()
{
    uint core = get_core_num();
    if (!_irqStackTop[core])
    {
        // ERROR
        return;
    }
    restore_interrupts(_irqStack[core][--_irqStackTop[core]]);
}

void noInterrupts()
{
    uint core = get_core_num();
    if (_irqStackTop[core] == maxIRQs)
    {
        // ERROR
        panic("IRQ stack overflow");
    }
    _irqStack[core][_irqStackTop[core]++] = save_and_disable_interrupts();
}
static uint64_t _gpioIrqEnabled = 0; // Sized to work with RP2350B, 48 GPIOs
static uint64_t _gpioIrqUseParam;
void *_gpioIrqCB[GPIOCNT];
void *_gpioIrqCBParam[GPIOCNT];

// Only 1 GPIO IRQ callback for all pins, so we need to look at the pin it's for and
// dispatch to the real callback manually
void _gpioInterruptDispatcher(uint gpio, uint32_t events) {
    (void) events;
    uint64_t mask = 1LL << gpio;
    if (_gpioIrqEnabled & mask) {
        if (_gpioIrqUseParam & mask) {
            voidFuncPtr cb = (voidFuncPtr)_gpioIrqCB[gpio];
            cb();
        } else {
            voidFuncPtrParam cb = (voidFuncPtrParam)_gpioIrqCB[gpio];
            cb(_gpioIrqCBParam[gpio]);
        }
    }
}

// To be called when appropriately protected w/IRQ and mutex protects
static void _detachInterruptInternal(uint pin) {
    uint64_t mask = 1LL << pin;
    if (_gpioIrqEnabled & mask) {
        gpio_set_irq_enabled(pin, 0x0f /* all */, false);
        _gpioIrqEnabled &= ~mask;
    }
}
void attachInterrupt(uint pin, voidFuncPtr callback, PinStatus mode) 
{
    uint64_t mask = 1LL << pin;
    uint32_t events;
    switch (mode) {
    case LOW:     events = 1; break;
    case HIGH:    events = 2; break;
    case FALLING: events = 4; break;
    case RISING:  events = 8; break;
    case CHANGE:  events = 4 | 8; break;
    default:      return;  // ERROR
    }
    noInterrupts();
    _detachInterruptInternal(pin);
    _gpioIrqEnabled |= mask;
    _gpioIrqUseParam &= ~mask; // No parameter
    _gpioIrqCB[pin] = (void *)callback;
    gpio_set_irq_enabled_with_callback(pin, events, true, _gpioInterruptDispatcher);
    interrupts();
}
void attachInterruptParam(uint pin, voidFuncPtrParam callback, PinStatus mode, void *param) 
{
    
    uint64_t mask = 1LL << pin;
    uint32_t events;
    switch (mode) {
    case LOW:     events = 1; break;
    case HIGH:    events = 2; break;
    case FALLING: events = 4; break;
    case RISING:  events = 8; break;
    case CHANGE:  events = 4 | 8; break;
    default:      return;  // ERROR
    }
    noInterrupts();
    _detachInterruptInternal(pin);
    _gpioIrqEnabled |= mask;
    _gpioIrqUseParam &= ~mask; // No parameter
    _gpioIrqCB[pin] = (void *)callback;
    _gpioIrqCBParam[pin] = param;
    gpio_set_irq_enabled_with_callback(pin, events, true, _gpioInterruptDispatcher);
    interrupts();
}
void detachInterrupt(uint pin) {
    noInterrupts();
    _detachInterruptInternal(pin);
    interrupts();
}
