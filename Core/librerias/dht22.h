/**
 * dht22.h - Sensor DHT22 / AM2302 (1-wire) con GPIO open-drain y contador DWT.
 *
 * Uso:
 *   dht22_init();                         // una vez, despues de SystemClock_Config()
 *   ...
 *   dht22_data_t d;
 *   if (dht22_read(&d) == HAL_OK) { usar d.temp_c, d.hum_pct }   // maximo cada 2 s
 *
 * Requisitos:
 *   - Pin con la etiqueta DHT22 en CubeMX (DHT22_Pin / DHT22_GPIO_Port).
 *   - Pull-up de 4.7k a 10k hacia 3.3 V (o modulo de 3 pines que ya lo trae).
 */
#ifndef DHT22_H
#define DHT22_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

typedef struct {
    float temp_c;     /* temperatura en grados C */
    float hum_pct;    /* humedad relativa en % */
} dht22_data_t;

/* Activa el contador de ciclos (DWT) y deja el pin en open-drain, linea suelta. */
void dht22_init(void);

/* Lectura bloqueante (~7 ms). Devuelve:
 *   HAL_OK       lectura valida
 *   HAL_BUSY     se llamo antes de 2 s desde la lectura anterior (o del arranque)
 *   HAL_TIMEOUT  el sensor no responde
 *   HAL_ERROR    checksum incorrecto, o la linea esta atascada en bajo (falta pull-up) */
HAL_StatusTypeDef dht22_read(dht22_data_t *out);

#ifdef __cplusplus
}
#endif

#endif /* DHT22_H */
