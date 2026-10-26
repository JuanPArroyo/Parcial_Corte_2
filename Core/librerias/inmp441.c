/**
 * inmp441.c - Micrófono I2S INMP441: captura por DMA + RMS y frecuencia dominante.
 */
#include "inmp441.h"
#include <math.h>

#define HALF_HW   (INMP441_DMA_HALFWORDS / 2U)    /* 256 -> 64 tramas = 4 ms */

/* La ventana debe completarse exactamente al final de una mitad del DMA. */
_Static_assert((INMP441_WIN_SAMPLES % (HALF_HW / 4U)) == 0U,
               "INMP441_WIN_SAMPLES debe ser multiplo de las tramas por mitad de DMA");

static I2S_HandleTypeDef *s_hi2s = NULL;
static uint16_t           s_dma_buf[INMP441_DMA_HALFWORDS];
static float              s_win[INMP441_WIN_SAMPLES];
static uint16_t           s_win_idx = 0;
static volatile int32_t   s_pending  = -1;   /* offset de la mitad lista (0 o HALF_HW) */
static volatile uint32_t  s_overruns = 0;

/* ---- Callbacks del HAL (sobrescriben los __weak) ------------------------- */
void HAL_I2S_RxHalfCpltCallback(I2S_HandleTypeDef *hi2s)
{
    if (hi2s != s_hi2s) return;
    if (s_pending != -1) s_overruns++;
    s_pending = 0;
}

void HAL_I2S_RxCpltCallback(I2S_HandleTypeDef *hi2s)
{
    if (hi2s != s_hi2s) return;
    if (s_pending != -1) s_overruns++;
    s_pending = (int32_t)HALF_HW;
}

/* ---- Internas ------------------------------------------------------------ */
static void analizar_ventana(inmp441_result_t *out)
{
    /* 1) quitar offset DC y medir amplitud */
    float mean = 0.0f;
    for (uint32_t i = 0; i < INMP441_WIN_SAMPLES; i++) mean += s_win[i];
    mean /= INMP441_WIN_SAMPLES;

    float sumsq = 0.0f, peak = 0.0f;
    for (uint32_t i = 0; i < INMP441_WIN_SAMPLES; i++) {
        float v = s_win[i] - mean;
        s_win[i] = v;
        sumsq += v * v;
        if (fabsf(v) > peak) peak = fabsf(v);
    }
    float rms = sqrtf(sumsq / INMP441_WIN_SAMPLES);

    /* 2) frecuencia por cruces ascendentes con histéresis */
    float freq = 0.0f;
    if (rms > INMP441_RMS_MIN) {
        float th = 0.2f * peak;
        int armed = 0, n = 0, first = -1, last = -1;
        for (int i = 0; i < (int)INMP441_WIN_SAMPLES; i++) {
            if (s_win[i] < -th) armed = 1;
            else if (armed && s_win[i] > th) {
                armed = 0;
                if (first < 0) first = i;
                last = i;
                n++;
            }
        }
        if (n >= 3) freq = (float)(n - 1) * INMP441_FS_HZ / (float)(last - first);
    }

    out->freq_hz  = freq;
    out->rms      = rms;
    out->overruns = s_overruns;
}

static void procesar_mitad(const uint16_t *p)
{
    for (uint32_t k = 0; k < HALF_HW / 4U; k++) {                 /* 64 tramas */
        uint32_t w   = ((uint32_t)p[4 * k] << 16) | p[4 * k + 1]; /* canal izquierdo */
        int32_t  s24 = ((int32_t)w) >> 8;                         /* 24 bits con signo */
        s_win[s_win_idx++] = (float)s24;
    }
}

/* ---- API pública --------------------------------------------------------- */
HAL_StatusTypeDef inmp441_init(I2S_HandleTypeDef *hi2s)
{
    s_hi2s     = hi2s;
    s_win_idx  = 0;
    s_pending  = -1;
    s_overruns = 0;

    /* ojo: en formato de 24 bits el HAL cuenta tramas de 32 bits, por eso /2.
     * Después de esta llamada, hi2s->RxXferSize debe valer INMP441_DMA_HALFWORDS. */
    return HAL_I2S_Receive_DMA(hi2s, s_dma_buf, INMP441_DMA_HALFWORDS / 2U);
}

int inmp441_process(inmp441_result_t *out)
{
    int32_t off;

    __disable_irq();
    off = s_pending;
    s_pending = -1;
    __enable_irq();

    if (off < 0) return 0;

    procesar_mitad(&s_dma_buf[off]);

    if (s_win_idx >= INMP441_WIN_SAMPLES) {
        s_win_idx = 0;
        analizar_ventana(out);
        return 1;
    }
    return 0;
}

void inmp441_discard(void)
{
    s_win_idx = 0;
    s_pending = -1;
}
