# E2-1 — Repetibilidade da captura PWM

## Configuração

- Fonte: F28069M EPWM1A
- Saída: GPIO0
- Entrada: PicoHIL GPIO6
- Frequência nominal: 1 Hz
- Duty nominal: 50%
- Amostras válidas encontradas no arquivo: **141**

## Resultados

E2-1 — Repetibilidade da captura PWM
Amostras válidas: 141
Configuração nominal: 1 Hz, 50% duty

Frequência:
média = 1.008801 Hz
desvio-padrão = 0.000400 Hz
mínimo = 1.008000 Hz
máximo = 1.009000 Hz
pico-a-pico = 0.001000 Hz

Período:
média = 991.433099 ms
desvio-padrão = 0.145469 ms
mínimo = 991.139000 ms
máximo = 991.738000 ms
pico-a-pico = 0.599000 ms
CV = 0.01467 %

Duty:
média = 49.99950 %
desvio-padrão = 0.00218 %
mínimo = 49.99000 %
máximo = 50.00000 %
pico-a-pico = 0.01000 %


## Tabela para o paper

| Parâmetro | Valor nominal | Média medida | Desvio-padrão | Mínimo | Máximo | Pico-a-pico |
|---|---:|---:|---:|---:|---:|---:|
| Frequência (Hz) | 1,000000 | 1,008801 | 0,000400 | 1,008000 | 1,009000 | 0,001000 |
| Período (ms) | 1000,000 | 991,433 | 0,145 | 991,139 | 991,738 | 0,599 |
| Duty (%) | 50,000 | 49,9995 | 0,00218 | 49,990 | 50,000 | 0,010 |

## Observação

O arquivo bruto contém também mensagens de estado/plot e ocorrências de ausência de sinal. Para as estatísticas acima foram consideradas somente as linhas válidas no formato:

`E1_PWM,GPIO6,period_us=...,high_us=...,freq_hz=...,duty_pct=...`

O resultado caracteriza a **repetibilidade da captura realizada pelo PicoHIL**. A diferença entre a frequência nominal configurada no F28069M e a frequência medida pelo PicoHIL não deve ser interpretada como erro absoluto do PicoHIL sem uma referência externa independente.
