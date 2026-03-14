# Bring-Up Guide - Speeduino UI

Este documento descreve o processo passo a passo para configurar e executar o sistema de central multimídia/dash no Raspberry Pi 4.

## 1. Requisitos de Hardware

### 1.1 Componentes Principais
- Raspberry Pi 4 (4GB ou 16GB)
- Display touchscreen 5" ou 7" (HDMI ou DSI)
- Módulo CAN MCP2515 ou MCP2518FD (SPI)
- Cabo USB para Android Auto
- Câmera USB UVC para ré (opcional)

### 1.2 Conexões MCP2515 → Raspberry Pi 4

| MCP2515 | RPi4 Pin | GPIO |
|---------|----------|------|
| VCC     | Pin 1    | 3.3V |
| GND     | Pin 6    | GND  |
| CS      | Pin 24   | GPIO8 (SPI0 CE0) |
| SO      | Pin 21   | GPIO9 (SPI0 MISO) |
| SI      | Pin 19   | GPIO10 (SPI0 MOSI) |
| SCK     | Pin 23   | GPIO11 (SPI0 SCLK) |
| INT     | Pin 22   | GPIO25 |

## 2. Instalação do Sistema Operacional

### 2.1 Preparar SD Card
```bash
# Baixar Raspberry Pi OS 64-bit (Bookworm)
# Usar Raspberry Pi Imager para gravar

# Configurar no imager:
# - Hostname: speeduino-dash
# - Usuário: pi / sua_senha
# - WiFi (opcional)
# - SSH habilitado
```

### 2.2 Primeiro Boot
```bash
# Conectar via SSH ou terminal local
ssh pi@speeduino-dash.local

# Atualizar sistema
sudo apt update && sudo apt upgrade -y
```

## 3. Configurar CAN Interface

### 3.1 Configurar Overlay MCP2515
```bash
# Executar script de configuração
cd /path/to/speeduino-ui-openauto
sudo ./scripts/flash_overlay.sh

# Reiniciar
sudo reboot
```

### 3.2 Verificar Interface CAN
```bash
# Após reboot
ip link show can0

# Configurar manualmente (para teste)
sudo ./scripts/setup_can.sh can0 500000

# Testar com loopback (sem Speeduino)
sudo ip link set can0 down
sudo ip link set can0 type can loopback on
sudo ip link set can0 up

cansend can0 360#0BB81F401E200000
candump can0
```

## 4. Instalar Dependências

```bash
cd /path/to/speeduino-ui-openauto
sudo ./scripts/install_deps.sh
```

## 5. Compilar o Projeto

```bash
mkdir build && cd build
cmake -GNinja ..
ninja

# Verificar
ls -la src/can_service/can_service
ls -la src/reverse_service/reverse_service
ls -la src/hmi_launcher/hmi_launcher
```

## 6. Instalar

```bash
sudo ninja install

# Verificar binários
which can_service
which reverse_service
which hmi_launcher

# Instalar configurações
sudo cp -r configs/ /etc/speeduino-ui/

# Instalar systemd units
sudo cp systemd/*.service /etc/systemd/system/
sudo systemctl daemon-reload
```

## 7. Configurar Speeduino

### 7.1 Habilitar Broadcast CAN na Speeduino

No TunerStudio/SpeedyLoader:
1. Ir em `Settings` → `CAN/Second Serial`
2. Habilitar `CAN Broadcasting`
3. Selecionar protocolo: `Haltech IC-7` (recomendado)
4. Bitrate: `500kbps`
5. Gravar configuração

### 7.2 Verificar Comunicação
```bash
# Iniciar candump
candump can0

# Ligar motor (ou dar partida)
# Deve aparecer frames 0x360, 0x361, 0x362, etc.
```

## 8. Testar Serviços Individualmente

### 8.1 can_service
```bash
# Terminal 1: Iniciar serviço
./build/src/can_service/can_service -i can0 -c ./configs -v

# Terminal 2: Verificar ZMQ
python3 -c "
import zmq
ctx = zmq.Context()
sub = ctx.socket(zmq.SUB)
sub.connect('ipc:///tmp/speeduino_data.ipc')
sub.setsockopt_string(zmq.SUBSCRIBE, 'ENGINE')
while True:
    topic, data = sub.recv_multipart()
    print(f'{topic}: {len(data)} bytes')
"
```

### 8.2 reverse_service
```bash
./build/src/reverse_service/reverse_service -i can0 -c ./configs -v
```

### 8.3 hmi_launcher
```bash
# Em ambiente gráfico (Wayland/X11)
./build/src/hmi_launcher/hmi_launcher -c ./configs

# Fullscreen
./build/src/hmi_launcher/hmi_launcher -f -c ./configs
```

## 9. Habilitar Serviços no Boot

```bash
# Habilitar interface CAN
sudo systemctl enable speeduino-can.service

# Habilitar serviços
sudo systemctl enable can_service.service
sudo systemctl enable reverse_service.service
sudo systemctl enable hmi_launcher.service

# Iniciar agora
sudo systemctl start speeduino-can.service
sudo systemctl start can_service.service
sudo systemctl start reverse_service.service
sudo systemctl start hmi_launcher.service

# Verificar status
sudo systemctl status can_service.service
sudo systemctl status reverse_service.service
sudo systemctl status hmi_launcher.service
```

## 10. Instalar OpenAuto

### 10.1 Compilar aasdk
```bash
cd ~
git clone https://github.com/openDsh/aasdk.git
cd aasdk
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j4
sudo make install
```

### 10.2 Compilar OpenAuto
```bash
cd ~
git clone https://github.com/openDsh/openauto.git
cd openauto
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j4
sudo make install
```

### 10.3 Testar OpenAuto
```bash
# Conectar smartphone Android via USB
openauto --fullscreen
```

## 11. Configurar Auto-Login e Boot Gráfico

```bash
# Configurar auto-login para ambiente gráfico
sudo raspi-config
# → System Options → Boot / Auto Login → Desktop Autologin

# Ou via linha de comando
sudo systemctl set-default graphical.target
```

## 12. Otimizações de Boot

```bash
# Desabilitar serviços desnecessários
sudo systemctl disable bluetooth
sudo systemctl disable avahi-daemon
sudo systemctl disable triggerhappy

# Configurar GPU memory
echo "gpu_mem=128" | sudo tee -a /boot/firmware/config.txt

# Desabilitar splash screen para boot mais rápido
sudo sed -i 's/quiet splash/quiet/' /boot/firmware/cmdline.txt
```

## 13. Troubleshooting

### CAN não funciona
```bash
# Verificar dmesg
dmesg | grep -i can
dmesg | grep -i mcp

# Verificar overlay
dtoverlay -l | grep mcp

# Verificar SPI
ls /dev/spidev*
```

### UI não inicia
```bash
# Verificar logs
journalctl -u hmi_launcher -f

# Testar Qt manualmente
QT_QPA_PLATFORM=eglfs /usr/local/bin/hmi_launcher
```

### OpenAuto não conecta
```bash
# Verificar USB
lsusb

# Verificar logs
journalctl -u openauto -f

# Verificar Android Debug Bridge
# No smartphone: Habilitar USB Debugging + Android Auto Developer
```

## 14. Verificação Final (Checklist)

- [ ] `ip link show can0` mostra interface UP
- [ ] `candump can0` recebe frames da Speeduino
- [ ] `systemctl status can_service` mostra active (running)
- [ ] `systemctl status hmi_launcher` mostra active (running)
- [ ] UI exibe RPM, CLT, TPS em tempo real
- [ ] Toque em "Android Auto" abre OpenAuto
- [ ] Engatar ré troca para câmera (se configurado)
- [ ] Sistema boota automaticamente após power cycle
