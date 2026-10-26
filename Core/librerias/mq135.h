#ifndef MQ135_H
#define MQ135_H
 
#ifdef __cplusplus
extern "C" {
#endif
 
#include <stdint.h>
#include <stddef.h>
#include <math.h>
 
/* ---------------- Parámetros eléctricos (AJUSTAR A TU HARDWARE) ---------------- */
#define MQ135_ADC_VREF      3.3f     // Referencia del ADC (VDDA del STM32)
#define MQ135_VC_SENSOR     4.76f     // Alimentación del módulo MQ-135 (calefactor + RL)
#define MQ135_DIV_FACTOR    1.5f     // (R1+R2)/R2 si hay divisor en serie entre AO y el pin ADC.
                                     // Usa 1.0 si AO va directo al pin. VERIFICAR comparando
                                     // V_AO (Teleplot) con el multímetro en AO.
#define MQ135_ADC_MAX_VAL   4095.0f  // ADC de 12 bits (0 - 4095)
 
/* ---------------- Parámetros del sensor ---------------- */
#define MQ135_RL            28.84f   // kOhm medidos entre AO y GND (RL del módulo || resistencia del divisor)
#define MQ135_DEFAULT_R0    1712.22f   // R0 por defecto (kOhm). Reemplazar por el R0 calibrado.
 
/* ---------------- Curva de CO2: ppm = a * (Rs/R0)^b ---------------- */
#define MQ135_CO2_A         110.47f
#define MQ135_CO2_B        -2.862f
#define MQ135_ATMO_CO2      420.0f   // ppm de referencia en aire limpio para calibrar
 
typedef struct {
    float    r0;       // Resistencia de referencia (kOhm)
    float    rl;       // Resistencia de carga (kOhm)
    uint32_t adc_raw;  // Valor bruto del ADC
    float    voltage;  // Voltaje en AO (V), compensado por el divisor
    float    rs;       // Resistencia actual del sensor (kOhm)
    float    ppm;      // CO2 estimado (ppm)
} MQ135_t;
 
void  MQ135_Init(MQ135_t *sensor, float r0_calibrada);
float MQ135_CalculateRs(const MQ135_t *sensor, uint32_t adc_val);
void  MQ135_Calibrate(MQ135_t *sensor, uint32_t adc_prom);
float MQ135_ReadPPM(MQ135_t *sensor, uint32_t adc_val);
 
#ifdef __cplusplus
}
#endif
 
#endif /* MQ135_H */
 