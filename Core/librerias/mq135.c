#include "mq135.h"
 
void MQ135_Init(MQ135_t *sensor, float r0_calibrada) {
    if (sensor == NULL) return;
    sensor->rl      = MQ135_RL;
    sensor->r0      = (r0_calibrada > 0.0f) ? r0_calibrada : MQ135_DEFAULT_R0;
    sensor->adc_raw = 0;
    sensor->voltage = 0.0f;
    sensor->rs      = 0.0f;
    sensor->ppm     = 0.0f;
}
 
/* Rs = RL * (Vc - Vout) / Vout, con Vout = voltaje real en AO */
float MQ135_CalculateRs(const MQ135_t *sensor, uint32_t adc_val) {
    if (sensor == NULL) return 0.0f;
 
    float v_pin = ((float)adc_val / MQ135_ADC_MAX_VAL) * MQ135_ADC_VREF;
    float v_out = v_pin * MQ135_DIV_FACTOR;
 
    if (v_out <= 0.001f) return 999999.0f;
    if (v_out >= MQ135_VC_SENSOR) return 0.1f;
 
    return sensor->rl * (MQ135_VC_SENSOR - v_out) / v_out;
}
 
/* Calibra R0 en aire limpio: R0 = Rs / (ppm/a)^(1/b) */
void MQ135_Calibrate(MQ135_t *sensor, uint32_t adc_prom) {
    if (sensor == NULL) return;
 
    float rs     = MQ135_CalculateRs(sensor, adc_prom);
    float factor = powf(MQ135_ATMO_CO2 / MQ135_CO2_A, 1.0f / MQ135_CO2_B);
 
    if (factor > 0.0f && rs > 0.0f) {
        sensor->r0 = rs / factor;
    }
}
 
float MQ135_ReadPPM(MQ135_t *sensor, uint32_t adc_val) {
    if (sensor == NULL || sensor->r0 <= 0.0f) return 0.0f;
 
    sensor->adc_raw = adc_val;
    sensor->voltage = ((float)adc_val / MQ135_ADC_MAX_VAL) * MQ135_ADC_VREF * MQ135_DIV_FACTOR;
    sensor->rs      = MQ135_CalculateRs(sensor, adc_val);
 
    float ratio = sensor->rs / sensor->r0;
    sensor->ppm = MQ135_CO2_A * powf(ratio, MQ135_CO2_B);
    return sensor->ppm;
}
 