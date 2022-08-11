/*
 * Lego EV3
 *
 * Copyright (c) 2022 Justus Dieckmann
 *
 * This code is licensed under the GNU GPL v2.
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "cpu.h"
#include "hw/sysbus.h"
#include "migration/vmstate.h"
#include "hw/arm/boot.h"
#include "net/net.h"
#include "sysemu/sysemu.h"
#include "hw/boards.h"
#include "hw/char/serial.h"
#include "qemu/timer.h"
#include "hw/ptimer.h"
#include "hw/qdev-properties.h"
#include "hw/block/flash.h"
#include "ui/console.h"
#include "hw/i2c/i2c.h"
#include "hw/irq.h"
#include "hw/or-irq.h"
#include "hw/audio/wm8750.h"
#include "sysemu/block-backend.h"
#include "sysemu/runstate.h"
#include "sysemu/dma.h"
#include "ui/pixel_ops.h"
#include "qemu/cutils.h"
#include "qom/object.h"
#include "hw/net/mv88w8618_eth.h"
#include "hw/loader.h"
#include "hw/misc/unimp.h"
#include "hw/intc/ev3_aintc.h"

#define EV3_ENTRY               0xC0008000

#define EV3_RAM_BASE            0xC0000000
#define EV3_RAM_SIZE            256*1024*1024

#define EV3_LOCAL_RAM_BASE      0xFFFF0000
#define EV3_LOCAL_RAM_SIZE      8*1024

#define EV3_MMCSD0_BASE         0x01C40000
#define EV3_MMCSD0_SIZE         4*1024

#define EV3_LCD_BASE            0x01E13000
#define EV3_LCD_SIZE            4*1024

#define EV3_UART0_BASE          0x01C42000
#define EV3_UART1_BASE          0x01D0C000
#define EV3_UART2_BASE          0x01D0D000

#define EV3_AINTC_BASE          0xFFFEE000
#define EV3_AINTC_SIZE          0x2000

// TODO ev3 Timer
// TODO ev3 SD-Card

static struct arm_boot_info ev3_binfo = {
        .entry = 0xC0008000,
        .loader_start = 0xC0008000,
        .board_id = 0xe73,
};

static void add_unimplemented_devices(void) {
    create_unimplemented_device("UART0", EV3_UART0_BASE, 0x1000);
    create_unimplemented_device("UART2", EV3_UART2_BASE, 0x1000);
    create_unimplemented_device("MMCSD0", EV3_MMCSD0_BASE, EV3_MMCSD0_SIZE);
    create_unimplemented_device("LCD Controller", EV3_LCD_BASE, EV3_LCD_SIZE);
    create_unimplemented_device("ARM Interrupt Controller", 0xFFFEE000, 0x2000);
    create_unimplemented_device("TIMER0", 0x01C20000, 0x1000);
    create_unimplemented_device("TIMER1", 0x01C21000, 0x1000);
    create_unimplemented_device("I2C0", 0x01C22000, 0x1000);
    create_unimplemented_device("RTC", 0x01C23000, 0x1000);
    create_unimplemented_device("SPI0", 0x01C41000, 0x1000);
    create_unimplemented_device("GPIO", 0x01E26000, 0x1000);
    create_unimplemented_device("EDMA", 0x01C00000, 0x8000);

    DeviceState *dev = qdev_new(TYPE_UNIMPLEMENTED_DEVICE);
    qdev_prop_set_string(dev, "name", "   ######AAAAlllles andere");
    qdev_prop_set_uint64(dev, "size", 0x100000000);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);
    sysbus_mmio_map_overlap(SYS_BUS_DEVICE(dev), 0, 0, -10000);
}

static void ev3_init(MachineState *machine)
{
    ARMCPU *cpu;
    MachineClass *mc = MACHINE_GET_CLASS(machine);
    MemoryRegion *address_space_mem = get_system_memory();
    MemoryRegion *ramlocal = g_new(MemoryRegion, 1);
    MemoryRegion *psc0rom = g_new(MemoryRegion, 1);
    MemoryRegion *psc1rom = g_new(MemoryRegion, 1);

    /* For now, we use a fixed - the original - RAM size */
    if (machine->ram_size != mc->default_ram_size) {
        char *sz = size_to_str(mc->default_ram_size);
        error_report("Invalid RAM size, should be %s", sz);
        g_free(sz);
        exit(EXIT_FAILURE);
    }

    cpu = ARM_CPU(cpu_create(machine->cpu_type));

    // Memory init
    memory_region_add_subregion(address_space_mem, EV3_RAM_BASE, machine->ram);

    memory_region_init_ram(ramlocal, NULL, "ev3.localram", EV3_LOCAL_RAM_SIZE, &error_fatal);
    memory_region_add_subregion(address_space_mem, EV3_LOCAL_RAM_BASE, ramlocal);

    // Interrupt Controller

    EV3AINTCState *aintc = EV3_AINTC(qdev_new(TYPE_EV3_AINTC));
    SysBusDevice *sysbusdev = SYS_BUS_DEVICE(aintc);
    sysbus_mmio_map(sysbusdev, 0, EV3_AINTC_BASE);
    sysbus_connect_irq(sysbusdev, 0, qdev_get_gpio_in(DEVICE(cpu), ARM_CPU_IRQ));
    sysbus_connect_irq(sysbusdev, 1, qdev_get_gpio_in(DEVICE(cpu), ARM_CPU_FIQ));
    // qdev_pass_gpios(DEVICE(aintc), machine, NULL);


    serial_mm_init(address_space_mem, EV3_UART1_BASE, 2,
                   NULL,
                   1825000, serial_hd(0), DEVICE_NATIVE_ENDIAN);
    // TODO ev3 serial has dynamic baudclock (

    /////////////////////// PLEASE DONT TELL MOM ///////////////////////

    memory_region_init_rom(psc0rom, NULL, "ev3.ps0rom", 0x1000, &error_fatal);
    memory_region_add_subregion(address_space_mem, 0x01C10000, psc0rom);
    int i = -1;

    address_space_write_rom(&address_space_memory, 0x01C10128, MEMTXATTRS_UNSPECIFIED, &i, 4);

    memory_region_init_rom(psc1rom, NULL, "ev3.ps1rom", 0x1000, &error_fatal);
    memory_region_add_subregion(address_space_mem, 0x01E27000, psc1rom);

    address_space_write_rom(&address_space_memory, 0x01E27128, MEMTXATTRS_UNSPECIFIED, &i, 4);

    ////////////////////// YOU CAN TELL HER AGAIN //////////////////////

    // For faster startup just set ptstat = 0x3 (0x01c10128 + 0x01e27128)

    add_unimplemented_devices();

    /* load the firmware image (typically kernel.img) */
    ssize_t r = load_image_targphys("image", EV3_ENTRY,
                            EV3_RAM_SIZE - (EV3_ENTRY - EV3_RAM_BASE));
    if (r < 0) {
        error_report("Failed to load firmware");
        exit(1);
    }

    ev3_binfo.firmware_loaded = true;

    cpu->env.boot_info = &ev3_binfo;

    arm_load_kernel(cpu, machine, &ev3_binfo);
}

static void ev3_machine_init(MachineClass *mc)
{
    mc->desc = "AM1808 / Lego Mindstorm EV3 (ARM926EJ-S)";
    mc->init = ev3_init;
    mc->ignore_memory_transaction_failures = true;
    mc->default_cpu_type = ARM_CPU_TYPE_NAME("arm926");
    mc->default_ram_size = EV3_RAM_SIZE;
    mc->default_ram_id = "ev3.ram";
}

DEFINE_MACHINE("ev3", ev3_machine_init)

static void ev3_register_types(void)
{
    /*type_register_static(&mv88w8618_pic_info);
    type_register_static(&mv88w8618_pit_info);
    type_register_static(&mv88w8618_flashcfg_info);
    type_register_static(&mv88w8618_wlan_info);
    type_register_static(&musicpal_lcd_info);
    type_register_static(&musicpal_gpio_info);
    type_register_static(&musicpal_key_info);
    type_register_static(&musicpal_misc_info);*/
}

type_init(ev3_register_types)
