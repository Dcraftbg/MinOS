#pragma once
#include "interrupt.h"
extern IntController apic_controller;
// NOTE: Called by ACPI
uint32_t get_lapic_id(void);
void enable_lapic_timer(void);
void lapic_timer_reload(void);
void init_apic();
