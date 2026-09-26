/*
 * HIL_F28069M_PWM_TEST.c
 *
 * Teste E1 - F28069M -> PicoHIL
 *
 * Gera PWM em:
 *
 *   GPIO0 / EPWM1A
 *   LAUNCHXL-F28069M J4-40
 *
 * Parametros:
 *   Frequencia = 1 kHz
 *   Duty       = 50 %
 *
 * Conexao:
 *
 *   F28069M J4-40 (GPIO0/EPWM1A)
 *          |
 *          +-----------------> PicoHIL U1-2 (GPIO6)
 *
 *   F28069M J3-22 (GND)
 *          |
 *          +-----------------> PicoHIL H4-2 (GND)
 */

#include "DSP28x_Project.h"


/* ------------------------------------------------------------------------- */
/* Configuracao do ePWM1                                                     */
/* ------------------------------------------------------------------------- */

static void epwm1_init(void)
{
    EALLOW;

    /*
     * GPIO0 = EPWM1A
     *
     * GPIO0 fica no primeiro bit do GPAMUX1.
     */
    GpioCtrlRegs.GPAMUX1.bit.GPIO0 = 1;

    /*
     * GPIO0 sera controlado pelo periferico ePWM.
     */
    GpioCtrlRegs.GPADIR.bit.GPIO0 = 1;

    EDIS;


    /*
     * Desabilita o clock dos ePWM durante a configuracao.
     */
    EALLOW;
    SysCtrlRegs.PCLKCR0.bit.TBCLKSYNC = 0;
    EDIS;


    /*
     * ---------------------------------------------------------------------
     * Time-base
     * ---------------------------------------------------------------------
     *
     * SYSCLK = 90 MHz
     *
     * HSPCLKDIV = 1
     * CLKDIV    = 1
     *
     * TBCLK = 90 MHz
     *
     * Para 1 kHz:
     *
     *     TBPRD = 90 MHz / 1 kHz - 1
     *           = 89999
     */

    EPwm1Regs.TBCTL.bit.CTRMODE = TB_COUNT_UP;

    EPwm1Regs.TBCTL.bit.PHSEN = TB_DISABLE;

    EPwm1Regs.TBCTL.bit.PRDLD = TB_SHADOW;

    EPwm1Regs.TBCTL.bit.SYNCOSEL = TB_SYNC_DISABLE;

    EPwm1Regs.TBCTL.bit.HSPCLKDIV = 7;

    EPwm1Regs.TBCTL.bit.CLKDIV = 7;

    EPwm1Regs.TBPRD = 50192;

    /*
     * Contador inicia em zero.
     */
    EPwm1Regs.TBCTR = 0;


    /*
     * ---------------------------------------------------------------------
     * Compare
     * ---------------------------------------------------------------------
     *
     * 50% de duty:
     *
     * CMPA = aproximadamente metade de TBPRD.
     */

    EPwm1Regs.CMPCTL.bit.LOADAMODE = CC_CTR_ZERO;

    EPwm1Regs.CMPCTL.bit.SHDWAMODE = CC_SHADOW;

    EPwm1Regs.CMPA.half.CMPA = 25096;


    /*
     * ---------------------------------------------------------------------
     * Action Qualifier
     * ---------------------------------------------------------------------
     *
     * No inicio do periodo:
     *
     *     PWM = HIGH
     *
     * Quando CTR = CMPA:
     *
     *     PWM = LOW
     *
     * Resultado:
     *
     *     aproximadamente 50% duty cycle.
     */

    EPwm1Regs.AQCTLA.bit.ZRO = AQ_SET;

    EPwm1Regs.AQCTLA.bit.CAU = AQ_CLEAR;


    /*
     * Dead-band desabilitado.
     */
    EPwm1Regs.DBCTL.all = 0;


    /*
     * Habilita novamente o clock dos ePWM.
     */
    EALLOW;
    SysCtrlRegs.PCLKCR0.bit.TBCLKSYNC = 1;
    EDIS;
}


/* ------------------------------------------------------------------------- */
/* MAIN                                                                      */
/* ------------------------------------------------------------------------- */

void main(void)
{
    /*
     * Inicializacao do sistema.
     */
    InitSysCtrl();

    /*
     * Desabilita interrupcoes durante inicializacao.
     */
    DINT;

    InitPieCtrl();

    IER = 0x0000;
    IFR = 0x0000;

    InitPieVectTable();


    /*
     * Inicializa EPWM1A.
     */
    epwm1_init();


    /*
     * Loop infinito.
     *
     * O ePWM continua funcionando autonomamente.
     */
    for (;;)
    {
        /*
         * Nao ha necessidade de software aqui.
         *
         * O hardware ePWM gera continuamente:
         *
         * GPIO0 / EPWM1A
         * 1 kHz
         * 50%
         */
    }
}