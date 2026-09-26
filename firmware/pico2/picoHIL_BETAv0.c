/*
 * Projeto: picoHIL - Firmware de simulação de circuitos
 * Autor: Luiz Daniel S. Bezerra
 * Data: Dezembro/2025 - Versão beta inicial.
 *
 * Descrição:
 * Este firmware implementa rotinas de simulação de circuitos elétricos
 * (R, L, C, fontes e elementos controlados) para uso em sistemas embarcados,
 * como o Raspberry Pi Pico2.
 * 
 * Emula com excelente precisao Circuitos Simples;
 * No Raspberry Pi Pico 2 estão disponíveis as entradas ADC0[26], ADC1[27], ADC2[28]
 * que podem iteragir com o circuito sendo fontes de tensão ou corrente.
 * A saída do circuito é realizada através de 4 PWM+LPF (2.2k com 10nF), que podem
 * ler as tensões nos nós ou a corrente através dos elementos.
 * O código foi feito em C para máxima optmizacao dos recuros limitados e aproveitamento
 * da CPU do Pico2.
 * 
 * Existem exemplos de uso no arquivo circuit.c
 * 
 *
 * Licença:
 * Copyright (c) 2025 Luiz Daniel S. Bezerra
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * O SOFTWARE É FORNECIDO "NO ESTADO EM QUE SE ENCONTRA", SEM GARANTIA DE
 * QUALQUER TIPO, EXPRESSA OU IMPLÍCITA, INCLUINDO MAS NÃO SE LIMITANDO ÀS
 * GARANTIAS DE COMERCIALIZAÇÃO, ADEQUAÇÃO A UM PROPÓSITO ESPECÍFICO E
 * NÃO VIOLAÇÃO. EM NENHUM CASO OS AUTORES OU DETENTORES DE DIREITOS SERÃO
 * RESPONSÁVEIS POR QUALQUER RECLAMAÇÃO, DANOS OU OUTRAS RESPONSABILIDADES,
 * SEJA EM AÇÃO DE CONTRATO, ATO ILÍCITO OU OUTRA FORMA, DECORRENTE DE,
 * OU RELACIONADA AO SOFTWARE OU AO USO OU OUTRAS INTERAÇÕES COM O SOFTWARE.
 */

#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "hardware/interp.h"
#include "hardware/timer.h"
#include "hardware/clocks.h"
#include "hardware/uart.h"
#include "hardware/pwm.h"
#include "hardware/watchdog.h"
#include "mini_spiceHILv3.h"


extern void setup_rlc_circuit(ms_circuit_t *c, volatile float *adc_in);

extern void benchmark_matrices();
void setup_pwm(uint pin, uint chan, uint duty) {
    gpio_set_function(pin, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(pin);
    pwm_set_wrap(slice, PWM_WRAP);
    pwm_set_chan_level(slice, chan, duty);
    pwm_set_enabled(slice, true);
}

uint32_t millis() {
    return to_ms_since_boot(get_absolute_time());
}
uint64_t micros() {
    return to_us_since_boot(get_absolute_time());
}

// I2C defines
// This example will use I2C0 on GPIO8 (SDA) and GPIO9 (SCL) running at 400KHz.
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
#define I2C_PORT i2c0
#define I2C_SDA 8
#define I2C_SCL 9


int64_t alarm_callback(alarm_id_t id, void *user_data) {
    // Put your timeout handler code in here
    return 0;
}


// UART defines
// By default the stdout UART is `uart0`, so we will use the second one
#define UART_ID uart1
#define BAUD_RATE 115200

// Use pins 4 and 5 for UART1
// Pins can be changed, see the GPIO function select table in the datasheet for information on GPIO assignments
#define UART_TX_PIN 4
#define UART_RX_PIN 5

// ======================================================
// TESTE DE VALIDACAO VIA USB/SERIAL
// Envia Va, Vb e Vc para o PC em formato CSV.
// O valor 5 significa: 1 amostra a cada 5 passos de simulacao.
// Como dt = 150 us, a taxa nominal e ~666.7 amostras/s.
// ======================================================
#define PLOT_EVERY_STEPS 5

const uint LED_PIN = PICO_DEFAULT_LED_PIN;
const uint GPIO22_MONITOR_OUTPUT = 22;

// ======================================================
// TESTE E1 — PWM DIGITAL DO F28069M -> GPIO6
// PicoHIL U1-2 = GPIO6
//
// O GPIO6 fica reservado como entrada digital para receber
// o EPWM1A do F28069M. A medição é feita por interrupção
// nas bordas de subida e descida, sem alterar a lógica da
// simulação existente.
// ======================================================
const uint PWM_INPUT_PIN = 6;

// Variáveis atualizadas pela interrupção do GPIO6.
volatile uint64_t pwm_last_rise_us = 0;
volatile uint64_t pwm_period_us = 0;
volatile uint64_t pwm_high_us = 0;
volatile uint64_t pwm_last_edge_us = 0;
volatile uint32_t pwm_rise_count = 0;
volatile uint32_t pwm_fall_count = 0;
volatile bool pwm_have_rise = false;
volatile bool pwm_have_fall = false;

// Callback de captura do PWM recebido do F28069M.
void pwm_input_irq_callback(uint gpio, uint32_t events)
{
    if (gpio != PWM_INPUT_PIN) return;

    uint64_t now_us = micros();
    pwm_last_edge_us = now_us;

    if (events & GPIO_IRQ_EDGE_RISE) {
        pwm_rise_count++;

        if (pwm_have_rise) {
            pwm_period_us = now_us - pwm_last_rise_us;
        }

        pwm_last_rise_us = now_us;
        pwm_have_rise = true;
    }

    if (events & GPIO_IRQ_EDGE_FALL) {
        pwm_fall_count++;
        
        if (pwm_have_rise) {
            pwm_high_us = now_us - pwm_last_rise_us;
            pwm_have_fall = true;
        }
    }
}


// ======================================================
// MAIN
// ======================================================
extern void setup_circuit(ms_circuit_t *c, volatile float *adc_in);
extern void output_circuit(ms_circuit_t *c);
extern void update_sources(ms_circuit_t *c, volatile float *adc_in);
ms_circuit_t circuit;
volatile float adc0_val, adc1_val, adc2_val;

int main()
{
    stdio_init_all();
    gpio_init(LED_PIN);     gpio_init(GPIO22_MONITOR_OUTPUT);
    
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_set_dir(GPIO22_MONITOR_OUTPUT, GPIO_OUT);

    // ✅ Configura ADCs
    adc_init();
    adc_gpio_init(26); // ADC0
    adc_gpio_init(27); // ADC1
    adc_gpio_init(28); // ADC2

    // ✅ Configura PWMs
    setup_pwm(16, PWM_CHAN_A, 0); // PWM0A
    setup_pwm(17, PWM_CHAN_B, 0); // PWM0B
    setup_pwm(18, PWM_CHAN_A, 0); // PWM1A
    setup_pwm(19, PWM_CHAN_B, 0); // PWM1B

    // ======================================================
    // E1 — GPIO6 como entrada digital para PWM do F28069M
    // PicoHIL U1-2 = GPIO6
    // ======================================================
   
gpio_init(PWM_INPUT_PIN);
gpio_set_dir(PWM_INPUT_PIN, GPIO_IN);

/*
 * Mantém o GPIO6 em LOW quando não há sinal externo.
 */
gpio_pull_down(PWM_INPUT_PIN);

/*
 * Captura as duas bordas do PWM:
 *
 * subida  -> mede período
 * descida -> mede tempo em HIGH
 */
gpio_set_irq_enabled_with_callback(
    PWM_INPUT_PIN,
    GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
    true,
    &pwm_input_irq_callback
);

    // I2C Initialisation. Using it at 400Khz.
    i2c_init(I2C_PORT, 400*1000);
    
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    // For more examples of I2C use see https://github.com/raspberrypi/pico-examples/tree/master/i2c

    // Interpolator example code
    interp_config cfg = interp_default_config();
    // Now use the various interpolator library functions for your use case
    // e.g. interp_config_clamp(&cfg, true);
    //      interp_config_shift(&cfg, 2);
    // Then set the config 
    interp_set_config(interp0, 0, &cfg);
    // For examples of interpolator use see https://github.com/raspberrypi/pico-examples/tree/master/interp

    // Timer example code - This example fires off the callback after 2000ms
    add_alarm_in_ms(2000, alarm_callback, NULL, false);
    // For more examples of timer use see https://github.com/raspberrypi/pico-examples/tree/master/timer
    gpio_put(LED_PIN, !gpio_get(LED_PIN));
    sleep_ms(5000);

    printf("System Clock Frequency is %d Hz\n", clock_get_hz(clk_sys));
    printf("USB Clock Frequency is %d Hz\n", clock_get_hz(clk_usb));
    // For more examples of clocks use see https://github.com/raspberrypi/pico-examples/tree/master/clocks

    if (watchdog_caused_reboot()) {
        printf("Reboot causado pelo Watchdog!\n");
    }
    watchdog_enable(5000, true);        // 5000us

    // Set up our UART
    uart_init(UART_ID, BAUD_RATE);
    // Set the TX and RX pins by using the function select on the GPIO
    // Set datasheet for more information on function select
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
    
    // Use some the various UART functions to send out data
    // In a default system, printf will also output via the default UART
    
    // Send out a string, with CR/LF conversions
    uart_puts(UART_ID, "picoHIL-BETAv0 => Hardware Init DONE!\n");
    printf("picoHIL-BETAv0 => Hardware Init DONE!\n");
    // For more examples of UART use see https://github.com/raspberrypi/pico-examples/tree/master/uart

    // ✅ Benchmark inicial
    benchmark_matrices();

    // ✅ Configura circuito RLC
    setup_circuit(&circuit, &adc0_val);

    // Depois de montar o circuito exibe.
    ms_list_elements(&circuit);
    // 
    int status;
    uint32_t blink_update = millis();
    uint64_t last_step = micros();

    // Contador usado somente pelo teste de aquisicao no PC.
    uint32_t plot_step_counter = 0;
    uint32_t pwm_report_millis = millis();

    // Cabecalho identificado pelo script Python.
    // printf("PICOHIL_PLOT,t,Va,Vb,Vc\n");

    while (true) {

        
        watchdog_update();
        // Leitura ADC0 e normalizacao
        adc_select_input(0);
        adc0_val = (float)adc_read() / 4095.0f;
        // Leitura ADC1 e normalizacao
        adc_select_input(1);
        adc1_val = (float)adc_read() / 4095.0f;
        // Leitura ADC2 e normalizacao
        adc_select_input(2);
        adc2_val = (float)adc_read() / 4095.0f;

        // Controle de passo em tempo real
        uint64_t circuit_stepcost, now = micros();
        if (now - last_step >= (uint64_t)(circuit.dt * 1e6)) {
            last_step += (uint64_t)(circuit.dt * 1e6); // avanço fixo
            //last_step = now;

            // Passo de simulação
            gpio_put(GPIO22_MONITOR_OUTPUT, true);
            uint64_t step_start = micros();
            status = ms_circuit_step(&circuit);
            // Para o exemplo setup_three_phase_rl2()
            update_sources(&circuit, &adc0_val);
            output_circuit(&circuit);
            circuit_stepcost = micros() - step_start;
            gpio_put(GPIO22_MONITOR_OUTPUT, false);

            // --------------------------------------------------
            // Saida para o "osciloscopio virtual" no PC.
            // Le as tensoes dos nos A, B e C do circuito trifasico.
            // A transmissao fica FORA da medicao de circuit_stepcost.
            // --------------------------------------------------
            plot_step_counter++;
            if (plot_step_counter >= PLOT_EVERY_STEPS) {
                plot_step_counter = 0;
                
                //printf("PLOT,%.6f,%.6f,%.6f,%.6f\n",
                 //      circuit.t,
                 //      ms_get_node_voltage(&circuit, 1),
                 //      ms_get_node_voltage(&circuit, 2),
                //       ms_get_node_voltage(&circuit, 3));
            }
        }

        if (circuit.t >= 10.0f) circuit.t = 0.0f;       
        //
        // Realiza um reset => importante para garantir que
        // as funcoes trigonometricas se comportem de forma
        // estavel durante a simulacao de longo termo.
        // Testar valores de acordo com a demanda.
        // Ex. t > 120 produz problemas de calculo na senoide
        // com f = 60.0Hz.
        // 

        uint32_t now_millis = millis();

//gpio_set_irq_enabled_with_callback(
    //PWM_INPUT_PIN,
    //GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
    //true,
    //&pwm_input_irq_callback
//);

        if(now_millis - blink_update > 250)
        {   blink_update = now_millis;
            gpio_put(LED_PIN, !gpio_get(LED_PIN));
            
            if (status != 0) {
                printf("Falha na simulação (código %d): %s\n",
                status, ms_system_status_str(status));
            }

            // ======================================================
            // E1 — Relatório do PWM recebido no GPIO6.
            // Não participa do cálculo do tempo de simulação.
            // ======================================================
            if (now_millis - pwm_report_millis >= 1000) {
                pwm_report_millis = now_millis;

                uint64_t period_copy = pwm_period_us;
                uint64_t high_copy = pwm_high_us;
                uint64_t last_edge_copy = pwm_last_edge_us;
                bool have_rise_copy = pwm_have_rise;
                bool have_fall_copy = pwm_have_fall;

                // Considera "sem sinal" se nenhuma borda foi detectada
                // nos últimos 200 ms.
                bool signal_timeout =
                     (last_edge_copy == 0) ||
                     ((uint64_t)millis() * 1000ULL - last_edge_copy > 1500000ULL);

                if (have_rise_copy && have_fall_copy &&
                    period_copy > 0 && !signal_timeout) {
                    float pwm_freq_hz = 1000000.0f / (float)period_copy;
                    float pwm_duty_pct =
                        100.0f * (float)high_copy / (float)period_copy;

                    printf(
                        "E1_PWM,GPIO6,period_us=%llu,high_us=%llu,"
                        "freq_hz=%.3f,duty_pct=%.2f\n",
                        (unsigned long long)period_copy,
                        (unsigned long long)high_copy,
                        pwm_freq_hz,
                        pwm_duty_pct
                    );
                } else {
                    printf("E1_PWM,GPIO6,sem_sinal\n");
                }
            }


            printf("picoHIL[%08dms]>> t:%0.4f steplen: %ldus adc0-2: %0.4f %0.4f %0.4f\n", 
                millis(),
                circuit.t,
                circuit_stepcost,
                adc0_val, adc1_val, adc2_val);
            // Descomentar as linhas abaixo para observar os valores nos nós
            // ou elementos para validacao de simulacao.
            //printf("picoHIL[%08dms]>> %0.4f, %0.4f\n",
            //    millis(),
            //    ms_get_node_voltage(&circuit, 1),
            //    ms_get_resistor_current(&circuit, 2));
        }

    }
}
