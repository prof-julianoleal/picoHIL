# F28069M – PicoHIL Interface

Firmware de teste da LAUNCHXL-F28069M utilizado na validação da
interface externa entre o F28069M e o PicoHIL.

## E1 – PWM externo

- Microcontrolador: TMS320F28069M
- Placa: LAUNCHXL-F28069M
- Periférico: ePWM1A
- Saída: GPIO0
- Conector: J4-40
- Frequência atual: aproximadamente 1 Hz
- Duty cycle: aproximadamente 50%

## Conexão com PicoHIL

| F28069M | PicoHIL |
|---|---|
| J4-40 — GPIO0 / EPWM1A | U1-2 — GPIO6 |
| GND — J3-22 | GND — H4-2 |

## Objetivo do teste

Gerar um sinal PWM externo no F28069M e utilizar o GPIO6 do
PicoHIL para realizar a captura temporal do sinal.

O PicoHIL calcula:

- período;
- tempo em nível HIGH;
- frequência;
- duty cycle.

## Resultado experimental E1

Com o F28069M configurado para aproximadamente 1 Hz e 50% de duty,
o PicoHIL apresentou medições próximas de:

- período: 991,5 ms;
- tempo HIGH: 495,7 ms;
- frequência: 1,009 Hz;
- duty cycle: 50,00%.

O resultado demonstra a captura do PWM externo pelo GPIO6 do PicoHIL.

## Ambiente

Firmware desenvolvido no Code Composer Studio (CCS) utilizando
C2000Ware para o TMS320F28069M.

O arquivo foi mantido separadamente do C2000Ware original e versionado
neste repositório como parte do projeto experimental PicoHIL.
