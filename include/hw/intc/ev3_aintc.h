/*
 * TI ARM interrupt controller device emulation header.
 *
 * @see https://www.ti.com/lit/ug/spruh82c/spruh82c.pdf Chapter 11
 *
 * Copyright (c) 2022 Justus Dieckmann
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#ifndef EV3_AINTC_H
#define EV3_AINTC_H

#include "hw/sysbus.h"
#include "qom/object.h"

#define TYPE_EV3_AINTC  "ev3-aintc"
OBJECT_DECLARE_SIMPLE_TYPE(EV3AINTCState, EV3_AINTC)

#define EV3_AINTC_REV           0
#define EV3_AINTC_CR            4
#define EV3_AINTC_GER           0x10
#define EV3_AINTC_GNLR          0x1C
#define EV3_AINTC_SISR          0x20
#define EV3_AINTC_SICR          0x24
#define EV3_AINTC_EISR          0x28
#define EV3_AINTC_EICR          0x2C
#define EV3_AINTC_HIEISR        0x34
#define EV3_AINTC_HIEICR        0x38
#define EV3_AINTC_VBR           0x50
#define EV3_AINTC_VSR           0x54
#define EV3_AINTC_VNR           0x58
#define EV3_AINTC_GPIR          0x80
#define EV3_AINTC_GPVR          0x84
#define EV3_AINTC_SRSR1         0x200
// ...
#define EV3_AINTC_SRSR4         0x20C

#define EV3_AINTC_SECR1         0x280
// ...
#define EV3_AINTC_SECR4         0x28C

#define EV3_AINTC_ESR1          0x300
// ...
#define EV3_AINTC_ESR4          0x30C

#define EV3_AINTC_ECR1          0x380
// ...
#define EV3_AINTC_ECR4          0x38C

#define EV3_AINTC_CMR0          0x400
// ...
#define EV3_AINTC_CMR25         0x464

#define EV3_AINTC_HIPIR1        0x900
#define EV3_AINTC_HIPIR2        0x904

#define EV3_AINTC_HINLR1        0x1100
#define EV3_AINTC_HINLR2        0x1104

#define EV3_AINTC_HIER          0x1500

#define EV3_AINTC_HIPVR1        0x1600

#define EV3_AINTC_HIPVR2        0x1604

#define EV3_AINTC_SINT_NUM      100

#define EV3_AINTC_SINT_REG_NUM  DIV_ROUND_UP(EV3_AINTC_SINT_NUM, 32)

#define EV3_AINTC_CHANNEL_NUM   32

struct EV3AINTCState {
    /*< private >*/
    SysBusDevice parent_obj;
    /*< public >*/
    MemoryRegion iomem;
    qemu_irq parent_fiq;
    qemu_irq parent_irq;

    bool global_enable;
    uint32_t vector_base;
    uint8_t vector_size;
    uint32_t vector_null;
    uint32_t protect;
    uint32_t nmi;
    uint32_t sint_status[EV3_AINTC_SINT_REG_NUM];
    uint32_t sint_enabled[EV3_AINTC_SINT_REG_NUM];
    bool fiq_enabled;
    bool irq_enabled;
    uint8_t channel_for_sint[EV3_AINTC_SINT_NUM];
    /*uint32_t irq_pending[AW_A10_PIC_REG_NUM];
    uint32_t fiq_pending[AW_A10_PIC_REG_NUM];
    uint32_t select[AW_A10_PIC_REG_NUM];
    uint32_t enable[AW_A10_PIC_REG_NUM];
    uint32_t mask[AW_A10_PIC_REG_NUM];*/
    /*priority setting here*/
};

#endif
