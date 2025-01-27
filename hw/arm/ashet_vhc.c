#include "qemu/osdep.h"
#include "exec/address-spaces.h"
#include "hw/arm/armv7m.h"
#include "hw/arm/boot.h"
#include "hw/boards.h"
#include "hw/char/serial-mm.h"
#include "hw/qdev-clock.h"
#include "hw/misc/sifive_test.h"
#include "hw/rtc/goldfish_rtc.h"
#include "hw/qdev-properties.h"
#include "hw/ssi/pl022.h"
#include "qapi/error.h"
#include "qemu/error-report.h"
#include "sysemu/sysemu.h"
#include "cpu.h"

#define ROM_BASE 0x00000000UL

#define XIP_BASE 0x10000000UL
#define XIP_NOCACHE_NOALLOC_BASE 0x14000000UL
#define XIP_MAINTENANCE_BASE 0x18000000UL
#define XIP_NOCACHE_NOALLOC_NOTRANSLATE_BASE 0x1c000000UL

#define XIP_SLOT0_BASE XIP_BASE
#define XIP_SLOT0_SIZE (16UL << 20) // 16 MiB

#define XIP_SLOT1_BASE XIP_BASE + XIP_SLOT0_SIZE
#define XIP_SLOT1_SIZE (16UL << 20) // 16 MiB

#define SRAM_BASE 0x20000000UL
#define SRAM_STRIPED_BASE 0x20000000UL
#define SRAM0_BASE 0x20000000UL
#define SRAM4_BASE 0x20040000UL
#define SRAM_STRIPED_END 0x20080000UL
#define SRAM8_BASE 0x20080000UL
#define SRAM9_BASE 0x20081000UL
#define SRAM_END 0x20082000UL

#define SYSINFO_BASE 0x40000000UL
#define SYSCFG_BASE 0x40008000UL

#define CLOCKS_BASE 0x40010000UL
#define PSM_BASE 0x40018000UL
#define RESETS_BASE 0x40020000UL
#define IO_BANK0_BASE 0x40028000UL
#define IO_QSPI_BASE 0x40030000UL
#define PADS_BANK0_BASE 0x40038000UL
#define PADS_QSPI_BASE 0x40040000UL
#define XOSC_BASE 0x40048000UL
#define PLL_SYS_BASE 0x40050000UL
#define PLL_USB_BASE 0x40058000UL
#define ACCESSCTRL_BASE 0x40060000UL
#define BUSCTRL_BASE 0x40068000UL
#define UART0_BASE 0x40070000UL
#define UART1_BASE 0x40078000UL
#define SPI0_BASE 0x40080000UL
#define SPI1_BASE 0x40088000UL
#define I2C0_BASE 0x40090000UL
#define I2C1_BASE 0x40098000UL
#define ADC_BASE 0x400a0000UL
#define PWM_BASE 0x400a8000UL
#define TIMER0_BASE 0x400b0000UL
#define TIMER1_BASE 0x400b8000UL
#define HSTX_CTRL_BASE 0x400c0000UL
#define XIP_CTRL_BASE 0x400c8000UL
#define XIP_QMI_BASE 0x400d0000UL
#define WATCHDOG_BASE 0x400d8000UL
#define BOOTRAM_BASE 0x400e0000UL
#define ROSC_BASE 0x400e8000UL
#define TRNG_BASE 0x400f0000UL
#define SHA256_BASE 0x400f8000UL
#define POWMAN_BASE 0x40100000UL
#define TICKS_BASE 0x40108000UL
#define OTP_BASE 0x40120000UL
#define OTP_DATA_BASE 0x40130000UL
#define OTP_DATA_RAW_BASE 0x40134000UL
#define OTP_DATA_GUARDED_BASE 0x40138000UL
#define OTP_DATA_RAW_GUARDED_BASE 0x4013c000UL
#define CORESIGHT_PERIPH_BASE 0x40140000UL
#define CORESIGHT_ROMTABLE_BASE 0x40140000UL
#define CORESIGHT_AHB_AP_CORE0_BASE 0x40142000UL
#define CORESIGHT_AHB_AP_CORE1_BASE 0x40144000UL
#define CORESIGHT_TIMESTAMP_GEN_BASE 0x40146000UL
#define CORESIGHT_ATB_FUNNEL_BASE 0x40147000UL
#define CORESIGHT_TPIU_BASE 0x40148000UL
#define CORESIGHT_CTI_BASE 0x40149000UL
#define CORESIGHT_APB_AP_RISCV_BASE 0x4014a000UL
#define GLITCH_DETECTOR_BASE 0x40158000UL
#define TBMAN_BASE 0x40160000UL

#define DMA_BASE 0x50000000UL
#define USBCTRL_BASE 0x50100000UL
#define USBCTRL_DPRAM_BASE 0x50100000UL
#define USBCTRL_REGS_BASE 0x50110000UL
#define PIO0_BASE 0x50200000UL
#define PIO1_BASE 0x50300000UL
#define PIO2_BASE 0x50400000UL
#define XIP_AUX_BASE 0x50500000UL
#define HSTX_FIFO_BASE 0x50600000UL
#define CORESIGHT_TRACE_BASE 0x50700000UL

#define VIRTIO_BASE 0x60000000UL
#define VIRTIO_SIZE 0x200UL
#define VIRTIO_COUNT 8U

#define QEMU_BASE 0x70000000UL
#define QEMU_GOLDFISH_RTC QEMU_BASE + 0x100000UL
#define QEMU_SIFIVE_TEST QEMU_BASE + 0x200000UL


#define SIO_BASE 0xd0000000UL
#define SIO_NONSEC_BASE 0xd0020000UL

#define SYSCLK_FRQ (25 * 1000 * 1000) // 25 MHz
#define REFCLK_FRQ (1 * 1000 * 1000)  //  1 MHz

typedef struct AshetVhcMachineState {
  ARMv7MState armv7m;
  Clock *sysclk;
  Clock *refclk;

  MemoryRegion flash_alias;
  MemoryRegion xip_flash_rom;
  MemoryRegion builtin_sram;

} AshetVhcMachineState;

#define UARTn_REGSIZE 2 // (1<<2) = 4 byte per register

static void ashet_vhc_init(MachineState *machine) {
  // MachineClass *machine_class = MACHINE_GET_CLASS(machine);

  static const char *const legal_cpus[] = {
      ARM_CPU_TYPE_NAME("cortex-m0"),
      ARM_CPU_TYPE_NAME("cortex-m3"),
      ARM_CPU_TYPE_NAME("cortex-m4"),
      ARM_CPU_TYPE_NAME("cortex-m33"),
      NULL,
  };

  int cpu_type_index = -1;
  for (size_t i = 0; legal_cpus[i]; i++) {
    if (strcmp(machine->cpu_type, legal_cpus[i]) == 0) {
      cpu_type_index = i;
      break;
    }
  }
  if (cpu_type_index < 0) {
    error_report("Illegal CPU type. Use cortex-m{0,3,4,33}");
    exit(1);
  }

  AshetVhcMachineState *ashetvhc = g_new0(AshetVhcMachineState, 1);

  /* This clock doesn't need migration because it is fixed-frequency */
  ashetvhc->sysclk = clock_new(OBJECT(machine), "SYSCLK");
  clock_set_hz(ashetvhc->sysclk, SYSCLK_FRQ);

  ashetvhc->refclk = clock_new(OBJECT(machine), "REFCLK");
  clock_set_hz(ashetvhc->refclk, REFCLK_FRQ);

  if (!memory_region_init_rom(&ashetvhc->xip_flash_rom, NULL, "RP2350.xip0.flash", XIP_SLOT0_SIZE, &error_fatal)) {
    return;
  }

  if (!memory_region_init_ram(&ashetvhc->builtin_sram, NULL, "RP2350.sram", SRAM_END - SRAM_BASE, &error_fatal)) {
    return;
  }

  memory_region_init_alias(&ashetvhc->flash_alias, OBJECT(machine), "RP2350.xip0.alias", &ashetvhc->xip_flash_rom, ROM_BASE, XIP_SLOT0_SIZE);

  // Initialize RAM:
  MemoryRegion *system_memory = get_system_memory();

  // Create memory map:
  memory_region_add_subregion(system_memory, XIP_SLOT0_BASE, &ashetvhc->xip_flash_rom);
  memory_region_add_subregion(system_memory, XIP_SLOT1_BASE, machine->ram);
  memory_region_add_subregion(system_memory, SRAM_BASE, &ashetvhc->builtin_sram);
  memory_region_add_subregion(system_memory, ROM_BASE, &ashetvhc->flash_alias);

  // Create RP2350 devices:
  serial_mm_init(system_memory, UART0_BASE, UARTn_REGSIZE, 0, 399193, serial_hd(0), DEVICE_LITTLE_ENDIAN);
  serial_mm_init(system_memory, UART1_BASE, UARTn_REGSIZE, 0, 399193, serial_hd(1), DEVICE_LITTLE_ENDIAN);

  sysbus_create_simple(TYPE_PL022, SPI0_BASE, NULL);
  sysbus_create_simple(TYPE_PL022, SPI1_BASE, NULL);

  // Create virtual devices:
  sysbus_create_simple(TYPE_GOLDFISH_RTC, QEMU_GOLDFISH_RTC, NULL);
  sysbus_create_simple(TYPE_SIFIVE_TEST, QEMU_SIFIVE_TEST, NULL);

  // Create virtio slots:
  for(size_t i = 0; i < VIRTIO_COUNT; i++)
  {
    hwaddr const base = VIRTIO_BASE + VIRTIO_SIZE * i;
    sysbus_create_simple("virtio-mmio", base, NULL);
  }


  object_initialize_child(OBJECT(machine), "armv7m", &ashetvhc->armv7m, TYPE_ARMV7M);
  DeviceState *armv7m = DEVICE(&ashetvhc->armv7m);

  qdev_connect_clock_in(armv7m, "cpuclk", ashetvhc->sysclk);
  qdev_connect_clock_in(armv7m, "refclk", ashetvhc->refclk);
  qdev_prop_set_string(armv7m, "cpu-type", machine->cpu_type);

  qdev_prop_set_bit(armv7m, "enable-bitband", false);

  object_property_set_link(OBJECT(&ashetvhc->armv7m), "memory", OBJECT(system_memory), &error_fatal);
  sysbus_realize(SYS_BUS_DEVICE(&ashetvhc->armv7m), &error_fatal);

  // Load the kernel into the flash:
  armv7m_load_kernel(ashetvhc->armv7m.cpu,
                     machine->kernel_filename,
                     XIP_SLOT0_BASE,
                     XIP_SLOT0_SIZE);
}

static void ashet_vhc_machine_init(MachineClass *mc) {
  mc->desc = "Ashet Virtual Home Computer";
  mc->init = ashet_vhc_init;
  mc->default_cpu_type = ARM_CPU_TYPE_NAME("cortex-m33");
  mc->default_ram_size = XIP_SLOT1_SIZE;
  mc->default_ram_id = "RP2350.xip1.psram";
}

DEFINE_MACHINE("ashet-vhc", ashet_vhc_machine_init)
