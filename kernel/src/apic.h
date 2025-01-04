#pragma once
#include "interrupt.h"
extern IntController apic_controller;
// NOTE: Called by ACPI
void init_apic();
