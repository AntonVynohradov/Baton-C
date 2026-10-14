/**
 * @file  Intercom.c
 * @brief Public API: thin wrapper around a single static library instance.
 */
#include "ic_internal.h"

static ic_t s_ic;

void Intercom_GetDefaultCfg(IntercomCfg *cfg)
{
    ic_default_cfg(cfg);
}

Intercom_Status Intercom_Init(const IntercomCfg *cfg)
{
    return ic_init(&s_ic, cfg);
}

void Intercom_Process(void)
{
    ic_process(&s_ic);
}

Intercom_Status Intercom_Send(uint8_t dstAddr, uint8_t msgId, const void *data,
                              uint8_t len, Intercom_Prio prio)
{
    return ic_send(&s_ic, dstAddr, msgId, data, len, prio);
}

Intercom_Status Intercom_Publish(uint8_t msgId, const void *data, uint8_t len)
{
    return ic_publish(&s_ic, msgId, data, len);
}

void Intercom_RxByteIsr(uint8_t byte)
{
    ic_rx_byte_isr(&s_ic, byte);
}

void Intercom_TxDoneIsr(void)
{
    ic_tx_done_isr(&s_ic);
}

void Intercom_GetStats(Intercom_Stats *stats)
{
    ic_get_stats(&s_ic, stats);
}
