# PROMPT (copiar e colar) — Central multimídia + Dash tipo FuelTech (Linux embarcado) + Speeduino CAN + Android Auto + câmera de ré

Você é um arquiteto e desenvolvedor sênior de software automotivo (C/C++ e Python) para Linux embarcado.  
Projete e gere uma solução completa (arquitetura + código esqueleto + serviços) para uma **central multimídia/dash** estilo FuelTech, porém modular.

## 0) Contexto do sistema
- ECU: **Speeduino em STM32F407VE**
- Barramento: **CAN 2.0 @ 500 kbit/s**
- SBC: **Raspberry Pi 4 (4 GB)** ou equivalente (Linux embarcado)
- Display: **5" ou 7" touchscreen** (HDMI ou DSI)
- Android Auto: **com fio (USB)**
- Requisitos extras:
  - Integrar **comandos do volante** para controlar multimídia (volume, next/prev, etc.)
  - Integrar **câmera de ré**: ao engatar ré, trocar a tela automaticamente para vídeo da câmera (com baixa latência)

## 1) Objetivo do produto
Construir um sistema que:
1. Faça **dash/telemetria** lendo dados da Speeduino via CAN.
2. Permita **enviar comandos** via CAN (com segurança).
3. Tenha uma **UI tipo launcher**: ícones para abrir:
   - “Dash”
   - “Android Auto”
   - “Configurações”
4. Rode Android Auto no Linux via **OpenAuto** (ou equivalente open-source).
5. Faça **troca automática para câmera de ré** quando detectar “ré engatada”.

## 2) Diretrizes técnicas obrigatórias
### 2.1 Linux / Middleware
- CAN via **SocketCAN** (`can0`).
- Ferramentas de diagnóstico: `can-utils` (candump/cansend).
- Serviços via **systemd**.
- Logs via **journald**.
- Configuração em arquivo (YAML/JSON) para:
  - bitrate CAN
  - mapeamento de frames CAN (IDs, escalas)
  - mapeamento de botões do volante
  - gatilho de ré (ID/bit) e fallback

### 2.2 UI / HMI
- Use **Qt 6 + QML** para a UI principal (performance e manutenção).
- A UI deve ser fullscreen, com:
  - Home/launcher
  - Tela Dash (gauges, alertas)
  - Tela câmera de ré (fullscreen)
- O sistema deve conseguir “alternar contexto”:
  - Dash ↔ Android Auto ↔ Câmera de ré
- Não travar a UI por I/O (CAN e câmera devem rodar em threads/processos separados).

### 2.3 Android Auto
- Rodar via **OpenAuto** em fullscreen.
- A UI launcher deve iniciar/encerrar o OpenAuto sem travar.

### 2.4 Câmera de ré
- Entrada por **USB UVC** (ex.: webcam automotiva) ou CSI (se aplicável).
- Pipeline recomendado:
  - V4L2 + GStreamer (ou QtMultimedia se for suficiente)
- Metas:
  - Latência baixa (priorize “só funciona bem” ao invés de efeitos)
  - Start rápido quando engatar ré
- Gatilho de ré:
  - Preferencial: via **CAN** (frame específico ou sinal de marcha)
  - Fallback: GPIO (entrada digital 12V→3V3 com proteção e opto/TVS) lendo “lâmpada de ré”

### 2.5 Comandos do volante
O design deve suportar 2 formas:
1. **Volante via CAN** (mensagens do veículo) → decodifica e traduz em ações
2. **Volante analógico** (resistor ladder) → ADC/GPIO (se houver hardware), com calibração

### 2.6 Segurança funcional e robustez (nível embarcado/automotivo)
- Separar leitura e escrita CAN.
- Lista branca (whitelist) de comandos CAN permitidos.
- Rate limit para comandos.
- Timeouts e watchdogs de processo.
- Estados seguros:
  - Se CAN cair: UI mostra “sem CAN” e não envia comandos.
  - Se câmera falhar: volta para a tela anterior e loga erro.
- Persistência de configuração com checksum.
- Foco em inicialização rápida e recuperação.

## 3) Padrões de qualidade de código (obrigatório)
### 3.1 C/C++
- C++17 ou C++20 (justifique).
- Build: **CMake**.
- Estilo:
  - clang-format
  - clang-tidy
  - cppcheck
- Regras:
  - Sem alocação dinâmica em loop crítico de tempo real (onde aplicável).
  - Evitar exceções em módulos críticos (ou justificar onde usar).
  - RAII, `std::chrono`, `enum class`, `constexpr`.
  - Interfaces claras e testáveis.
- Testes:
  - GoogleTest (mínimo para parsers CAN e lógica de estado).

### 3.2 Python
- Python 3.11+ (se disponível na distro).
- Qualidade:
  - ruff, black, mypy (onde aplicável)
- Testes:
  - pytest
- Comunicação entre processos (Python↔C++↔UI):
  - Escolha **D-Bus** ou **ZeroMQ** e justifique.
- Evite depender de GUI no Python (UI fica no Qt).

## 4) Arquitetura exigida (faça assim)
Divida em processos/serviços:
1. `can_service` (C++ ou Python):
   - Sobe `can0`, lê frames, publica dados normalizados.
   - Recebe requests de “send command” com validação.
2. `hmi_launcher` (Qt/QML):
   - Home + Dash + Settings
   - Abre/fecha Android Auto
   - Mostra câmera de ré quando solicitado
3. `reverse_service`:
   - Detecta “ré engatada” (CAN e/ou GPIO)
   - Notifica o `hmi_launcher` para trocar tela
4. `openauto.service` (opcional):
   - Pode ser iniciado sob demanda pelo launcher

Defina interfaces claras entre eles.

## 5) O que você deve entregar na resposta
### 5.1 Documentação (em Markdown)
- Diagrama de blocos
- Diagrama de estados (launcher/dash/câmera/AA)
- Estratégia de segurança para comandos CAN
- Estratégia para volante (CAN e analógico)
- Estratégia para câmera (pipeline, fallback)
- Estrutura de diretórios do projeto (monorepo)
- Plano de bring-up (passo a passo) no Raspberry Pi 4

### 5.2 Código (esqueleto funcional)
Forneça:
- Estrutura de pastas:
  - `src/can_service/` (C++)
  - `src/hmi_launcher/` (Qt/QML)
  - `src/reverse_service/` (C++ ou Python)
  - `configs/` (YAML)
  - `scripts/` (setup)
  - `systemd/` (unit files)
- `CMakeLists.txt` principal + subprojetos
- Código mínimo para:
  - Subir SocketCAN e ler frames (can0)
  - Parser CAN com tabela de sinais (ID + scale + offset)
  - Endpoint IPC para UI consumir dados (D-Bus ou ZMQ)
  - Envio de comandos CAN com whitelist + rate limit
  - Serviço de ré (CAN trigger + GPIO fallback)
  - UI QML com 3 telas: Home, Dash, ReverseCamera
  - Botão “Android Auto” que inicia OpenAuto em fullscreen
- Unidades systemd:
  - `can_service.service`
  - `reverse_service.service`
  - `hmi_launcher.service`
  - `openauto@.service` (ou similar, se on-demand)

### 5.3 Critérios de aceite
Inclua uma lista objetiva de “feito quando”:
- `candump can0` mostra frames e `can_service` publica RPM/CLT/IAT/etc.
- UI atualiza gauges em tempo real.
- Toque no ícone abre Android Auto.
- Engatar ré troca para vídeo em < 500 ms (meta), com fallback seguro.
- Botões do volante controlam volume/next/prev (com ao menos 2 caminhos: CAN e configurável).

## 6) Restrições
- Não implementar CarPlay (apenas citar motivo: MFi/licença).
- Não depender de cloud.
- Não usar Docker.
- Não travar o boot com waits longos.
- Se precisar escolher entre “bonito” e “robusto”, escolha robusto.

## 7) Formato da resposta
- Use Markdown bem organizado.
- Inclua blocos de código completos.
- Não use placeholders vagos. Se algo for suposição, declare como suposição e forneça alternativa.
- Foque em algo que eu consigo compilar e rodar como MVP no Raspberry Pi 4.

Agora gere a solução completa seguindo tudo acima.
