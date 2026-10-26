/**
 * inmp441.h - Micrófono I2S INMP441: captura por DMA + RMS y frecuencia dominante.
 *
 * Uso:
 *   inmp441_init(&hi2s1);                      // una vez, después de MX_I2S1_Init()
 *   ...
 *   inmp441_result_t r;
 *   if (inmp441_process(&r)) { usar r.freq_hz, r.rms, r.overruns }   // en el while(1)
 */
#ifndef INMP441_H
#define INMP441_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

/* ---- Configuración (ajusta aquí) ---------------------------------------- */
#define INMP441_FS_HZ          16053U    /* frecuencia real de muestreo del I2S1 */
#define INMP441_DMA_HALFWORDS  512U      /* uint16_t del buffer DMA */
#define INMP441_WIN_SAMPLES    1024U     /* ventana de análisis: 64 ms */
#define INMP441_RMS_MIN        2000.0f   /* umbral de silencio: ajústalo viendo el rms */

typedef struct {
    float    freq_hz;     /* frecuencia estimada (0 si hay silencio o no se detecta) */
    float    rms;         /* amplitud RMS sin componente DC (unidades de 24 bits) */
    uint32_t overruns;    /* veces que el main no alcanzó a procesar una mitad del DMA */
} inmp441_result_t;

/* Arranca la recepción I2S por DMA circular. */
HAL_StatusTypeDef inmp441_init(I2S_HandleTypeDef *hi2s);

/* Procesa lo que haya llegado del DMA. Devuelve 1 si se completó una ventana
 * y 'out' tiene un resultado nuevo; 0 en caso contrario. Llamar en el while(1). */
int inmp441_process(inmp441_result_t *out);

/* Descarta la ventana en curso y cualquier mitad pendiente
 * (útil después de una lectura que bloquea, como el DHT22). */
void inmp441_discard(void);

#ifdef __cplusplus
}
#endif

#endif /* INMP441_H */
