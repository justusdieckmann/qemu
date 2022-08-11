/*
 * TI ARM interrupt controller device emulation.
 *
 * Copyright (c) 2022 Justus Dieckmann
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "migration/vmstate.h"
#include "hw/intc/ev3_aintc.h"
#include "hw/irq.h"
#include "qemu/log.h"
#include "qemu/module.h"

static void ev3_aintc_update(EV3AINTCState *s)
{
    /*uint8_t i;
    int irq = 0, fiq = 0, zeroes;

    s->vector = 0;

    for (i = 0; i < EV3_AINTC_REG_NUM; i++) {
        irq |= s->irq_pending[i] & ~s->mask[i];
        fiq |= s->select[i] & s->irq_pending[i] & ~s->mask[i];

        if (!s->vector) {
            zeroes = ctz32(s->irq_pending[i] & ~s->mask[i]);
            if (zeroes != 32) {
                s->vector = (i * 32 + zeroes) * 4;
            }
        }
    }

    qemu_set_irq(s->parent_irq, !!irq);
    qemu_set_irq(s->parent_fiq, !!fiq);*/
}

static bool ev3_aintc_set_irq_status_raw(EV3AINTCState *s, uint8_t irq, bool level) {
    if ((irq & 0x7f) >= EV3_AINTC_SINT_NUM) {
        return false;
    }

    if (level) {
        set_bit(irq % 32, (void *)&s->sint_status[irq / 32]);
    } else {
        clear_bit(irq % 32, (void *)&s->sint_status[irq / 32]);
    }

    return true;
}

static void ev3_aintc_set_irq_status(void *opaque, int irq, int level)
{
    EV3AINTCState *s = opaque;

    if (ev3_aintc_set_irq_status_raw(s, irq, level)) {
        ev3_aintc_update(s);
    }
}

static bool ev3_aintc_set_irq_enabled_raw(EV3AINTCState *s, uint8_t irq, bool enabled) {
    if ((irq & 0x7f) >= EV3_AINTC_SINT_NUM) {
        return false;
    }

    if (enabled) {
        set_bit(irq % 32, (void *)&s->sint_enabled[irq / 32]);
    } else {
        clear_bit(irq % 32, (void *)&s->sint_enabled[irq / 32]);
    }

    return true;
}

static void ev3_aintc_set_irq_enabled(EV3AINTCState *s, uint8_t irq, bool enabled) {
    if (ev3_aintc_set_irq_enabled_raw(s, irq, enabled)) {
        ev3_aintc_update(s);
    }
}

static uint64_t ev3_aintc_read(void *opaque, hwaddr offset, unsigned size)
{
    EV3AINTCState *s = opaque;
    uint8_t index = ((offset) / 4) * 4;

    switch (offset) {
    case EV3_AINTC_REV:
        return 0x4E82A900;
    case EV3_AINTC_CR:
        return 0; // TODO implement;
    case EV3_AINTC_GER:
        return s->global_enable;
    case EV3_AINTC_GNLR:
        return 0; // TODO implement;
    case EV3_AINTC_VBR:
        return s->vector_base;
    case EV3_AINTC_VSR:
        return s->vector_size;
    case EV3_AINTC_VNR:
        return s->vector_null;
    case EV3_AINTC_GPIR:
        qemu_log_mask(LOG_UNIMP, "Unimplemented read access to EV3_AINTC_GPIR\n");
        break;
    case EV3_AINTC_GPVR:
        qemu_log_mask(LOG_UNIMP, "Unimplemented read access to EV3_AINTC_GPVR\n");
        break;
    case EV3_AINTC_SECR1 ... EV3_AINTC_SECR4:
        // TODO I dont really understand...
        qemu_log_mask(LOG_UNIMP, "Unimplemented read access to EV3_AINTC_SECR%d\n", index);
        break;
    case EV3_AINTC_CMR0 ... EV3_AINTC_CMR25:
        index = (offset & 0x7F) * 4;
        uint32_t result = 0;
        for (uint i = 0; i < 4; i++) {
            result |= s->channel_for_sint[index | i] << i * 8;
        }
        return result;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: Bad offset 0x%x\n",  __func__, (int)offset);
        break;
    }

    return 0;
}

static void ev3_aintc_write(void *opaque, hwaddr offset, uint64_t value,
                             unsigned size)
{
    EV3AINTCState *s = opaque;

    if ((offset & 3) != 0 && (offset < EV3_AINTC_CMR0 || offset > EV3_AINTC_CMR25)) {
        qemu_log_mask(LOG_UNIMP, "AINTC: Offset not 4 byte aligned, write 0x%lx to 0x%lx\n", value, offset);
    }

    uint index = (offset & 0xc) / 4;

    switch (offset) {
    case EV3_AINTC_GER:
        s->global_enable = 1;
        break;
    case EV3_AINTC_SISR:
        ev3_aintc_set_irq_status(s, value, true);
        break;
    case EV3_AINTC_SICR:
        ev3_aintc_set_irq_status(s, value, false);
        break;
    case EV3_AINTC_EISR:
        ev3_aintc_set_irq_enabled(s, value, true);
        break;
    case EV3_AINTC_EICR:
        ev3_aintc_set_irq_enabled(s, value, false);
        break;
    case EV3_AINTC_HIEISR:
        if (value & 1) {
            s->irq_enabled = true;
        } else {
            s->fiq_enabled = true;
        }
        ev3_aintc_update(s);
        break;
    case EV3_AINTC_HIEICR:
        if (value & 1) {
            s->irq_enabled = false;
        } else {
            s->fiq_enabled = false;
        }
        ev3_aintc_update(s);
        break;
    case EV3_AINTC_VBR:
        s->vector_base = value;
        break;
    case EV3_AINTC_VSR:
        s->vector_size = value & 0xFF;
        break;
    case EV3_AINTC_VNR:
        s->vector_null = value;
        break;
    case EV3_AINTC_SRSR1 ... EV3_AINTC_SRSR4:
        for (uint i = index * 32; value; i++) {
            if (value & 1) {
                ev3_aintc_set_irq_status_raw(s, i, true);
            }
            value >>= 1;
        }
        ev3_aintc_update(s);
        break;
    case EV3_AINTC_SECR1 ... EV3_AINTC_SECR4:
        for (uint i = index * 32; value; i++) {
            if (value & 1) {
                ev3_aintc_set_irq_status_raw(s, i, false);
            }
            value >>= 1;
        }
        ev3_aintc_update(s);
        break;
    case EV3_AINTC_ESR1 ... EV3_AINTC_ESR4:
        for (uint i = index * 32; value; i++) {
            if (value & 1) {
                ev3_aintc_set_irq_enabled_raw(s, i, true);
            }
            value >>= 1;
        }
        ev3_aintc_update(s);
        break;
    case EV3_AINTC_ECR1 ... EV3_AINTC_ECR4:
        for (uint i = index * 32; value; i++) {
            if (value & 1) {
                ev3_aintc_set_irq_enabled_raw(s, i, false);
            }
            value >>= 1;
        }
        ev3_aintc_update(s);
        break;
    case EV3_AINTC_CMR0 ... EV3_AINTC_CMR25:
        index = (offset & 0x7F);
        for (uint i = 0; i < size; i++) {
            s->channel_for_sint[index | i] = value & 0xFF;
            value >>= 8;
        }
        break;
    case EV3_AINTC_HIER:
        s->irq_enabled = (value & 2) != 0;
        s->fiq_enabled = (value & 1) != 0;
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: Bad offset 0x%x\n",  __func__, (int)offset);
        break;
    }

    ev3_aintc_update(s);
}

static const MemoryRegionOps ev3_aintc_ops = {
    .read = ev3_aintc_read,
    .write = ev3_aintc_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static const VMStateDescription vmstate_ev3_aintc = {
    .name = "a10.pic",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
            /*VMSTATE_UINT32(vector, EV3AINTCState),
            VMSTATE_UINT32(base_addr, EV3AINTCState),
            VMSTATE_UINT32(protect, EV3AINTCState),
            VMSTATE_UINT32(nmi, EV3AINTCState),
            VMSTATE_BOOL_ARRAY(status, EV3AINTCState, EV3_AINTC_SINT_NUM),
            VMSTATE_BOOL_ARRAY(enabled, EV3AINTCState, EV3_AINTC_SINT_NUM),
            VMSTATE_UINT32_ARRAY(irq_pending, EV3AINTCState, EV3_AINTC_SINT_NUM),
            VMSTATE_UINT32_ARRAY(fiq_pending, EV3AINTCState, EV3_AINTC_SINT_NUM),
            VMSTATE_UINT32_ARRAY(enable, EV3AINTCState, EV3_AINTC_SINT_NUM),
            VMSTATE_UINT32_ARRAY(select, EV3AINTCState, EV3_AINTC_SINT_NUM),
            VMSTATE_UINT32_ARRAY(mask, EV3AINTCState, EV3_AINTC_SINT_NUM),*/
        VMSTATE_END_OF_LIST()
    }
};

static void ev3_aintc_init(Object *obj)
{
    EV3AINTCState *s = EV3_AINTC(obj);
    SysBusDevice *dev = SYS_BUS_DEVICE(obj);

    qdev_init_gpio_in(DEVICE(dev), ev3_aintc_set_irq_status, EV3_AINTC_SINT_NUM);
    sysbus_init_irq(dev, &s->parent_irq);
    sysbus_init_irq(dev, &s->parent_fiq);
    memory_region_init_io(&s->iomem, OBJECT(s), &ev3_aintc_ops, s,
                          TYPE_EV3_AINTC, 0x2000);
    sysbus_init_mmio(dev, &s->iomem);
}

static void ev3_aintc_reset(DeviceState *d)
{
    EV3AINTCState *s = EV3_AINTC(d);
    uint8_t i;

    s->protect = 0;
    s->nmi = 0;
    for (i = 0; i < EV3_AINTC_SINT_NUM; i++) {
        /*s->irq_pending[i] = 0;
        s->fiq_pending[i] = 0;
        s->select[i] = 0;
        s->enable[i] = 0;
        s->mask[i] = 0;*/
    }
}

static void ev3_aintc_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->reset = ev3_aintc_reset;
    dc->desc = "ev3 interrupt controller";
    dc->vmsd = &vmstate_ev3_aintc;
 }

static const TypeInfo ev3_aintc_info = {
    .name = TYPE_EV3_AINTC,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(EV3AINTCState),
    .instance_init = ev3_aintc_init,
    .class_init = ev3_aintc_class_init,
};

static void ev3_aintc_register_types(void)
{
    type_register_static(&ev3_aintc_info);
}

type_init(ev3_aintc_register_types);
