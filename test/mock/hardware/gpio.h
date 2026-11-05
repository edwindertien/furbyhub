#pragma once
#include <stdint.h>
#define GPIO_IN  0
#define GPIO_OUT 1
void gpio_init(unsigned pin);
void gpio_put(unsigned pin, bool v);
void gpio_set_dir(unsigned pin, bool out);
void gpio_disable_pulls(unsigned pin);
