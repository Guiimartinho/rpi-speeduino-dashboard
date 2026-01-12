# Instalacao Camera de Re - VW Gol Quadrado AP 1.8

Este guia cobre a instalacao do sistema de deteccao de marcha re para
VW Gol Quadrado (e outros VW classicos brasileiros) com motor AP 1.8,
tanto na versao aspirada quanto turbo com Speeduino.

## Visao Geral do Sistema

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        GOL QUADRADO - SISTEMA DE RE                     │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│   CAIXA DE CAMBIO                    RASPBERRY PI 4                     │
│   ┌─────────────┐                    ┌─────────────┐                    │
│   │ Interruptor │    CIRCUITO        │             │                    │
│   │   de Re     │───►INTERFACE ──────►│  GPIO 17   │                    │
│   │  (+12V)     │    (12V→3.3V)      │             │                    │
│   └─────────────┘                    └──────┬──────┘                    │
│         │                                   │                           │
│         │                                   ▼                           │
│         │                           ┌─────────────┐                     │
│         │                           │   Camera    │                     │
│         ▼                           │    de Re    │                     │
│   ┌─────────────┐                   └─────────────┘                     │
│   │  Luz de Re  │                                                       │
│   │ (Lanterna)  │                                                       │
│   └─────────────┘                                                       │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

## Localizacao do Sinal de Re

### Interruptor na Caixa de Cambio

```
    VISTA SUPERIOR DO MOTOR (Gol Quadrado)

              FRENTE DO CARRO
                   ↑
    ┌──────────────────────────────┐
    │                              │
    │     ┌────────────────┐       │
    │     │                │       │
    │     │   MOTOR AP     │       │
    │     │                │       │
    │     └───────┬────────┘       │
    │             │                │
    │      ┌──────┴──────┐         │
    │      │   CAIXA DE  │         │
    │      │   CAMBIO    │         │
    │      │             │         │
    │   ──►│ (X) ← INTERRUPTOR     │
    │      │      DE RE  │         │
    │      └─────────────┘         │
    │                              │
    └──────────────────────────────┘

    Localizacao: Lado ESQUERDO da caixa de cambio
                 (olhando de cima, lado do motorista)
```

### Cores dos Fios (Referencia)

| Funcao              | Cor Comum           | Alternativa      |
|---------------------|---------------------|------------------|
| Sinal Re (+12V)     | Preto/Verde         | Verde            |
| Negativo (GND)      | Marrom              | Preto            |

**IMPORTANTE:** Sempre confirme com multimetro antes de conectar!

## Teste com Multimetro

```
    PROCEDIMENTO DE TESTE

    1. Localize o conector do interruptor de re

    2. Com ignicao LIGADA (motor pode estar desligado):

       ┌─────────────────────────────────────────┐
       │  MULTIMETRO em modo DC Volts (20V)      │
       │                                         │
       │  Ponta Vermelha → Fio do interruptor    │
       │  Ponta Preta    → Chassi (GND)          │
       │                                         │
       │  NEUTRO:    ~0V                         │
       │  RE ENGATADA: ~12-14V ✓                 │
       └─────────────────────────────────────────┘

    3. Se ler ~12V com re engatada, encontrou o fio correto!
```

## Circuito de Interface (12V para 3.3V)

### OPCAO A: Optoacoplador PC817 (RECOMENDADO)

Isolamento galvanico total - mais seguro para o Raspberry Pi.

```
    ESQUEMA ELETRICO - OPTOACOPLADOR


        LADO CARRO (12V)              │            LADO RASPBERRY (3.3V)
                                      │
                                      │
    Sinal Re ────────┐                │
    (+12V)           │                │
                    ┌┴┐               │
                    │ │ R1            │
                    │ │ 1K            │
                    └┬┘               │
                     │                │                    3.3V (Pin 1)
                     │    ┌───────────┼───────────────────────┬────
                     │    │  PC817    │                       │
                     ▼    │ ┌─────┐   │                      ┌┴┐
                   ──┬──  │ │1   4│   │                      │ │ R2
                     │    │ │ ●───┼───┼──────────────────────┤ │ 10K
                  [LED]   │ │     │   │                      └┬┘
                     │    │ │2   3│   │                       │
                   ──┴──  │ │ ●   │   │                       ├───── GPIO17 (Pin 11)
                     │    │ └──┬──┘   │                       │
                     │    │    │      │                       │
    GND Carro ───────┴────┼────┘      │                       │
                          │           │                      GND (Pin 9)
                          │           │

    PINAGEM PC817:
    ┌────────┐
    │ 1    4 │   1 = Anodo LED (entrada +)
    │ ●    ● │   2 = Catodo LED (entrada -)
    │        │   3 = Emissor Fototransistor
    │ ●    ● │   4 = Coletor Fototransistor
    │ 2    3 │
    └────────┘
```

**Lista de Componentes:**
- 1x Optoacoplador PC817 ou 4N25
- 1x Resistor 1KΩ 1/4W
- 1x Resistor 10KΩ 1/4W
- Fios, conectores, tubo termoretrátil

### OPCAO B: Divisor de Tensao com Zener

Mais simples, mas sem isolamento.

```
    ESQUEMA ELETRICO - DIVISOR DE TENSAO


    Sinal Re (+12V)
         │
         │
        ┌┴┐
        │ │ R1 = 10K
        │ │
        └┬┘
         │
         ├─────────────────────────► GPIO17 (Pin 11)
         │
        ┌┴┐
        │ │ R2 = 3.3K
        │ │
        └┬┘
         │
        ─┴─  D1 = Zener 3.3V
        ───  (protecao extra)
         │
         │
        GND


    CALCULO:
    Vout = Vin × R2/(R1+R2)
    Vout = 12V × 3.3K/(10K+3.3K)
    Vout = 12V × 0.248
    Vout = 2.98V ✓ (seguro para GPIO)
```

**Lista de Componentes:**
- 1x Resistor 10KΩ 1/4W
- 1x Resistor 3.3KΩ 1/4W
- 1x Diodo Zener 3.3V 500mW
- Fios, conectores

## Pinagem Raspberry Pi 4

```
    HEADER GPIO - RASPBERRY PI 4

           3.3V  (1) (2)  5V
    GPIO2  SDA1  (3) (4)  5V
    GPIO3  SCL1  (5) (6)  GND
          GPIO4  (7) (8)  GPIO14 TXD
            GND  (9) (10) GPIO15 RXD
   ════► GPIO17 (11) (12) GPIO18        ◄════ USAR ESTE PINO!
         GPIO27 (13) (14) GND
         GPIO22 (15) (16) GPIO23
           3.3V (17) (18) GPIO24
  GPIO10  MOSI  (19) (20) GND
   GPIO9  MISO  (21) (22) GPIO25
  GPIO11  SCLK  (23) (24) GPIO8 CE0
            GND (25) (26) GPIO7 CE1
          GPIO0 (27) (28) GPIO1
          GPIO5 (29) (30) GND
          GPIO6 (31) (32) GPIO12
         GPIO13 (33) (34) GND
         GPIO19 (35) (36) GPIO16
         GPIO26 (37) (38) GPIO20
            GND (39) (40) GPIO21


    CONEXOES NECESSARIAS:

    ┌──────────────────────────────────────┐
    │  Funcao          │  Pino  │  GPIO    │
    ├──────────────────┼────────┼──────────┤
    │  3.3V (pull-up)  │   1    │   -      │
    │  GND             │   9    │   -      │
    │  Sinal Re        │  11    │  GPIO17  │
    └──────────────────────────────────────┘
```

## Diagrama de Instalacao Completo

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                                                                             │
│  ┌─────────────┐                                                            │
│  │   BATERIA   │                                                            │
│  │    12V      │                                                            │
│  └──────┬──────┘                                                            │
│         │                                                                   │
│         │ +12V                                                              │
│         │                                                                   │
│  ┌──────┴──────┐         ┌─────────────┐                                    │
│  │   FUSIVEL   │         │ INTERRUPTOR │                                    │
│  │    10A      │────────►│   DE RE     │                                    │
│  └─────────────┘         │(caixa cambio)│                                    │
│                          └──────┬──────┘                                    │
│                                 │                                           │
│                    ┌────────────┴────────────┐                              │
│                    │                         │                              │
│                    ▼                         ▼                              │
│            ┌─────────────┐          ┌─────────────────┐                     │
│            │  LUZ DE RE  │          │    CIRCUITO     │                     │
│            │  (original) │          │   INTERFACE     │                     │
│            └──────┬──────┘          │  (12V → 3.3V)   │                     │
│                   │                 └────────┬────────┘                     │
│                   │                          │                              │
│                   ▼                          ▼                              │
│                  GND                 ┌─────────────────┐                    │
│                                      │  RASPBERRY PI   │                    │
│                                      │                 │                    │
│                                      │   ┌─────────┐   │     ┌──────────┐   │
│                                      │   │ GPIO17  │◄──┼─────│ INTERFACE│   │
│                                      │   └────┬────┘   │     └──────────┘   │
│                                      │        │        │                    │
│                                      │        ▼        │                    │
│                                      │  ┌──────────┐   │     ┌──────────┐   │
│                                      │  │ REVERSE  │   │     │  CAMERA  │   │
│                                      │  │ DETECTOR │───┼────►│   DE RE  │   │
│                                      │  └──────────┘   │     └──────────┘   │
│                                      │        │        │                    │
│                                      │        ▼        │                    │
│                                      │  ┌──────────┐   │     ┌──────────┐   │
│                                      │  │   HMI    │───┼────►│   TELA   │   │
│                                      │  │ LAUNCHER │   │     │ TOUCHSCR │   │
│                                      │  └──────────┘   │     └──────────┘   │
│                                      │                 │                    │
│                                      └─────────────────┘                    │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Configuracao do Software

### Arquivo de Configuracao

Crie/edite o arquivo `/etc/speeduino-ui/reverse.yaml`:

```yaml
# Configuracao de Deteccao de Marcha Re
# VW Gol Quadrado AP 1.8 (Aspirado ou Turbo)

reverse:
  # Modo de deteccao: "gpio" para carros classicos sem CAN
  detection_mode: "gpio"

  # Preset para configuracao rapida (opcional)
  # Valores: "gol_quadrado", "classic_vw", "speeduino_can", "haltech"
  preset: "gol_quadrado"

  # ═══════════════════════════════════════════════════════════════
  # CAN - DESABILITADO para Gol Quadrado
  # ═══════════════════════════════════════════════════════════════
  can_enabled: false

  # ═══════════════════════════════════════════════════════════════
  # GPIO - HABILITADO
  # ═══════════════════════════════════════════════════════════════
  gpio_enabled: true
  gpio_chip: "gpiochip0"      # Raspberry Pi 4
  gpio_line: 17               # GPIO17 = Pino 11 do header

  # active_low depende do seu circuito:
  # - Optoacoplador PC817: true (LED aceso = transistor conduz = LOW)
  # - Divisor de tensao: false (12V = HIGH proporcional)
  gpio_active_low: true

  # ═══════════════════════════════════════════════════════════════
  # DEBOUNCE
  # ═══════════════════════════════════════════════════════════════
  # Carros antigos podem ter interruptores com mais bounce
  # Aumente se houver ativacoes falsas
  debounce_ms: 100
```

### Teste do GPIO

Antes de rodar o sistema completo, teste o GPIO:

```bash
# Instalar gpioget (se necessario)
sudo apt install gpiod

# Testar leitura do GPIO17
# Com re em NEUTRO:
gpioget gpiochip0 17
# Deve retornar: 1 (se active_low) ou 0 (se active_high)

# Com re ENGATADA:
gpioget gpiochip0 17
# Deve retornar: 0 (se active_low) ou 1 (se active_high)
```

## Consideracoes para Versao Turbo

Para Gol Quadrado com motor AP Turbo + Speeduino:

```
┌─────────────────────────────────────────────────────────────────┐
│                     GOL QUADRADO TURBO                          │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│   A SPEEDUINO GERENCIA:                                         │
│   ✓ Injecao eletronica                                         │
│   ✓ Ignicao (avanco)                                           │
│   ✓ Controle de boost (se configurado)                         │
│   ✓ Wideband / Lambda                                          │
│                                                                 │
│   O CIRCUITO ORIGINAL DO CARRO MANTEM:                         │
│   ✓ Luz de re ← USA ESTE SINAL (GPIO)                          │
│   ✓ Luz de freio                                                │
│   ✓ Setas / Pisca                                               │
│   ✓ Farois                                                      │
│                                                                 │
│   FUTURO (se Speeduino tiver CAN):                             │
│   - Pode adicionar deteccao por CAN tambem                     │
│   - Configura detection_mode: "both"                           │
│   - GPIO funciona como fallback                                │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

## Troubleshooting

### Camera nao ativa ao engatar re

1. **Verificar sinal 12V:**
   ```bash
   # Com multimetro no fio do interruptor
   # Re engatada deve mostrar ~12V
   ```

2. **Verificar GPIO:**
   ```bash
   gpioget gpiochip0 17
   # Deve mudar entre 0 e 1 ao engatar/desengatar re
   ```

3. **Verificar logs:**
   ```bash
   journalctl -u reverse_service -f
   # Deve mostrar "Reverse gear ENGAGED/DISENGAGED"
   ```

### Ativacoes falsas (liga/desliga rapidamente)

- Aumentar `debounce_ms` para 150 ou 200
- Verificar conexoes (mau contato causa ruido)
- Verificar aterramento do circuito

### GPIO sempre em 0 ou sempre em 1

- Verificar se o optoacoplador/circuito esta funcionando
- Testar com LED antes de conectar ao Pi
- Verificar `gpio_active_low` esta correto para seu circuito

## Seguranca

```
⚠️  ATENCAO - TRABALHO COM SISTEMA ELETRICO AUTOMOTIVO

1. SEMPRE desconecte o terminal negativo da bateria antes
   de fazer qualquer conexao eletrica

2. Use fusivel de protecao no circuito (5A ou 10A)

3. Use conectores automotivos apropriados (a prova d'agua
   se possivel)

4. Proteja a fiacao com tubo corrugado ou espiral

5. Fixe bem todos os componentes para evitar vibracao

6. O circuito de interface DEVE ser usado - NUNCA conecte
   12V diretamente ao GPIO do Raspberry Pi!
```

## Proximos Passos

1. [ ] Montar circuito de interface em protoboard para teste
2. [ ] Testar com multimetro antes de conectar ao Pi
3. [ ] Instalar e testar no veiculo com motor desligado
4. [ ] Testar com motor ligado (verificar ruido eletrico)
5. [ ] Fixar instalacao permanente
6. [ ] Ajustar debounce se necessario

---

**Versao:** 1.0
**Data:** 2024
**Compatibilidade:** VW Gol Quadrado, Gol G1, Saveiro, Parati, Voyage (com motor AP)
