/**
 * dht22.c - Sensor DHT22 / AM2302 (1-wire) con GPIO open-drain y contador DWT.
 *
 * Protocolo: el micro baja la linea >= 1 ms y la suelta; el sensor responde con
 * ~80 us en bajo y ~80 us en alto, y luego manda 40 bits. Cada bit son ~50 us en
 * bajo seguidos de un pulso en alto: ~27 us = 0, ~70 us = 1.
 * Trama: humedad (16 bits, x0.1 %), temperatura (16 bits, x0.1 C, bit 15 = signo),
 * checksum (suma de los 4 bytes anteriores).
 */
#include "dht22.h"

/* 1 = desactiva las interrupciones mientras se leen los 40 bits (~4 ms); 0 = no.
 * Con 0 el audio por DMA no pierde mitades; como se mide el ANCHO de cada pulso,
 * el metodo tolera interrupciones cortas (< ~20 us). */
#ifndef DHT22_BLOQUEAR_IRQ
#define DHT22_BLOQUEAR_IRQ     0
#endif

/* El sensor necesita >= 2 s entre lecturas (y ~2 s despues del arranque). */
#ifndef DHT22_MIN_INTERVAL_MS
#define DHT22_MIN_INTERVAL_MS  2000U
#endif

static uint32_t s_cyc_per_us = 100U;
static uint32_t s_last_ms    = 0U;

/* Espera a que la linea llegue a 'level'. Devuelve los us esperados, o -1 si
 * se agota el tiempo. Para medir un pulso en alto: esperar SET, y luego llamar
 * a esta funcion con RESET; el valor devuelto es el ancho del pulso. */
static int32_t wait_level(GPIO_PinState level, uint32_t timeout_us)
{
    uint32_t t0    = DWT->CYCCNT;
    uint32_t limit = timeout_us * s_cyc_per_us;

    while (HAL_GPIO_ReadPin(DHT22_GPIO_Port, DHT22_Pin) != level) {
        if ((DWT->CYCCNT - t0) > limit) return -1;
    }
    return (int32_t)((DWT->CYCCNT - t0) / s_cyc_per_us);
}

/* Parte critica en tiempo: se llama con las interrupciones desactivadas. */
static HAL_StatusTypeDef read_raw(uint8_t d[5])
{
    /* suelta la linea y espera a que el pull-up la suba. Sin esta espera el pin
     * todavia lee bajo (es el nivel que puse yo) y se confunde con la respuesta
     * del sensor: todos los bits quedan corridos una posicion y el checksum falla. */
    HAL_GPIO_WritePin(DHT22_GPIO_Port, DHT22_Pin, GPIO_PIN_SET);
    if (wait_level(GPIO_PIN_SET, 100) < 0) return HAL_ERROR;   /* atascada en bajo: falta el pull-up o hay un corto */

    /* respuesta: ~80 us en bajo y ~80 us en alto */
    if (wait_level(GPIO_PIN_RESET, 250) < 0) return HAL_TIMEOUT;
    if (wait_level(GPIO_PIN_SET,   120) < 0) return HAL_TIMEOUT;
    if (wait_level(GPIO_PIN_RESET, 120) < 0) return HAL_TIMEOUT;

    /* 40 bits */
    for (uint8_t i = 0; i < 40U; i++) {
        if (wait_level(GPIO_PIN_SET, 100) < 0) return HAL_TIMEOUT;   /* fin de los 50 us en bajo */
        int32_t w = wait_level(GPIO_PIN_RESET, 120);                  /* ancho del pulso en alto */
        if (w < 0) return HAL_TIMEOUT;

        d[i / 8U] <<= 1;
        if (w > 48) d[i / 8U] |= 1U;                                  /* umbral entre 27 us y 70 us */
    }
    return HAL_OK;
}

void dht22_init(void)
{
    /* contador de ciclos del nucleo (DWT) para medir microsegundos */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
    s_cyc_per_us = SystemCoreClock / 1000000U;

    /* open-drain: escribir 1 suelta la linea y el IDR lee el nivel real */
    GPIO_InitTypeDef g = {0};
    g.Pin   = DHT22_Pin;
    g.Mode  = GPIO_MODE_OUTPUT_OD;
    g.Pull  = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(DHT22_GPIO_Port, &g);
    HAL_GPIO_WritePin(DHT22_GPIO_Port, DHT22_Pin, GPIO_PIN_SET);
}

HAL_StatusTypeDef dht22_read(dht22_data_t *out)
{
    uint8_t d[5] = {0, 0, 0, 0, 0};

    if (out == NULL) return HAL_ERROR;

    /* el sensor necesita >= 2 s entre lecturas; si es muy pronto no toca la linea */
    if ((HAL_GetTick() - s_last_ms) < DHT22_MIN_INTERVAL_MS) return HAL_BUSY;
    s_last_ms = HAL_GetTick();

    /* senal de inicio: linea en bajo 2-3 ms (el minimo es 1 ms) */
    HAL_GPIO_WritePin(DHT22_GPIO_Port, DHT22_Pin, GPIO_PIN_RESET);
    HAL_Delay(2);

#if DHT22_BLOQUEAR_IRQ
    __disable_irq();
#endif
    HAL_StatusTypeDef st = read_raw(d);
#if DHT22_BLOQUEAR_IRQ
    __enable_irq();
#endif

    HAL_GPIO_WritePin(DHT22_GPIO_Port, DHT22_Pin, GPIO_PIN_SET);
    if (st != HAL_OK) return st;

    if ((uint8_t)(d[0] + d[1] + d[2] + d[3]) != d[4]) return HAL_ERROR;

    uint16_t rh = ((uint16_t)d[0] << 8) | d[1];
    uint16_t t  = ((uint16_t)d[2] << 8) | d[3];

    out->hum_pct = (float)rh / 10.0f;
    out->temp_c  = (t & 0x8000U) ? -((float)(t & 0x7FFFU) / 10.0f) : ((float)t / 10.0f);
    return HAL_OK;
}
