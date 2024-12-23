#pragma once
#include <stdint.h>
#include <pci.h>
#include <minos/status.h>
#include <log.h>
intptr_t init_xhci(PciDevice* dev);
