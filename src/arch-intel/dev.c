/*
 * \brief   DEV and PCI code.
 * \date    2006-10-25
 * \author  Bernhard Kauer <kauer@tudos.org>
 */
/*
 * Copyright (C) 2006,2007,2010  Bernhard Kauer <kauer@tudos.org>
 * Technische Universitaet Dresden, Operating Systems Research Group
 *
 * This file is part of the OSLO package, which is distributed under
 * the  terms  of the  GNU General Public Licence 2.  Please see the
 * COPYING file for details.
 */

#ifndef ISABELLE
#include "asm.h"
#include "alloc.h"
#include "tcg.h"
#include "util.h"
#include "dev.h"
#include "mp.h"
#include "amd.h"

// Generate RESULT types
RESULT_GEN(BYTE);

#define CPU_NAME "AMD CPU booted by SABLE"
const char *const cpu_name = CPU_NAME;

/**
 * Read a byte from the pci config space.
 */
unsigned char pci_read_byte(unsigned addr) {
  outl(PCI_ADDR_PORT, addr);
  return inb(PCI_DATA_PORT + (addr & 3));
}

/**
 * Read a word from the pci config space.
 */
unsigned short pci_read_word(unsigned addr) {
  outl(PCI_ADDR_PORT, addr);
  return inw(PCI_DATA_PORT + (addr & 2));
}

/**
 * Read a long from the pci config space.
 */
unsigned pci_read_long(unsigned addr) {
  outl(PCI_ADDR_PORT, addr);
  return inl(PCI_DATA_PORT);
}

/**
 * Write a word to the pci config space.
 */
void pci_write_word(unsigned addr, unsigned short value) {
  outl(PCI_ADDR_PORT, addr);
  outw(PCI_DATA_PORT + (addr & 2), value);
}

/**
 * Write a long to the pci config space.
 */
void pci_write_long(unsigned addr, unsigned value) {
  outl(PCI_ADDR_PORT, addr);
  outl(PCI_DATA_PORT, value);
}

/**
 * Read a word from the pci config space.
 */
static inline unsigned short pci_read_word_aligned(unsigned addr) {
  outl(PCI_ADDR_PORT, addr);
  return inw(PCI_DATA_PORT);
}

static inline void pci_write_word_aligned(unsigned addr, unsigned short value) {
  outl(PCI_ADDR_PORT, addr);
  outw(PCI_DATA_PORT, value);
}

/**
 * Return an pci config space address of a device with the given
 * class/subclass id or 0 on error.
 *
 * Note: this returns the last device found!
 */
unsigned pci_find_device_per_class(unsigned short class) {
  unsigned res = 0;
  for (unsigned i = 0; i < 1 << 13; i++) {
    unsigned char maxfunc = 0;
    for (unsigned func = 0; func <= maxfunc; func++) {
      unsigned addr = 0x80000000 | i << 11 | func << 8;
      if (!maxfunc && pci_read_byte(addr + 14) & 0x80)
        maxfunc = 7;
      if (class == (pci_read_long(addr + 0x8) >> 16))
        res = addr;
    }
  }
  return res;
}


/**
 * Iterate over all devices in the pci config space.
 */
int pci_iterate_devices(void) {
  for (unsigned bus = 0; bus < 255; bus++)
    for (unsigned dev = 0; dev < 32; dev++) {
      unsigned char maxfunc = 0;
      for (unsigned func = 0; func <= maxfunc; func++) {
        unsigned addr = 0x80000000 | bus << 16 | dev << 11 | func << 8;
        unsigned value = pci_read_long(addr);
#ifndef NDEBUG
        unsigned class = pci_read_long(addr + 0x8) >> 16;
#endif

        unsigned char header_type = pci_read_byte(addr + 14);
        if (!maxfunc && header_type & 0x80)
          maxfunc = 7;
        if (!value || value == 0xffffffff)
          continue;
#ifndef NDEBUG
        out_hex(bus, 7);
        out_char(':');
        out_hex(dev, 4);
        out_char('.');
        out_hex(func, 3);
        out_char(' ');
        out_hex(class, 15);
        out_char(':');
        out_char(' ');
        out_hex(value & 0xffff, 15);
        out_char(':');
        out_hex(value >> 16, 15);
        out_char(' ');
        out_hex(header_type, 7);
        out_char('\n');
#endif
      }
    }
  return 0;
}


#define REALMODE_CODE 0x20000
extern char smp_init_start;
extern char smp_init_end;

#endif
