# CI/CD Pipeline

Este documento descreve o pipeline de CI/CD do projeto RPi Speeduino Dashboard.

## Visão Geral

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              GitHub Actions                                  │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌─────────────────────┐    ┌─────────────────────┐    ┌────────────────┐  │
│  │   PR / Branches     │    │    Merge to Main    │    │   Tag (v*.*)   │  │
│  │  (GitHub-hosted)    │    │  (Self-hosted Pi5)  │    │ (Self-hosted)  │  │
│  ├─────────────────────┤    ├─────────────────────┤    ├────────────────┤  │
│  │ • Lint & Format     │    │ • Full ARM64 Build  │    │ • Build        │  │
│  │ • Static Analysis   │    │ • Unit Tests        │    │ • Package .deb │  │
│  │ • Security Scan     │    │ • Integration Tests │    │ • Release      │  │
│  │ • Spell Check       │    │ • Artifacts         │    │ • Artifacts    │  │
│  └─────────────────────┘    └─────────────────────┘    └────────────────┘  │
│           │                          │                         │            │
│           ▼                          ▼                         ▼            │
│      ~2-3 min                    ~5-10 min                 ~10-15 min       │
│    (sem hardware)              (Raspberry Pi 5)          (Raspberry Pi 5)   │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Workflows

### 1. CI Lint (`ci-lint.yml`)

**Trigger:** Push em qualquer branch, PRs para main/develop

**Runner:** GitHub-hosted (ubuntu-latest)

**Jobs:**

| Job | Descrição | Ferramentas |
|-----|-----------|-------------|
| `format-check` | Verifica formatação C++ | clang-format |
| `static-analysis` | Análise estática | cppcheck, MISRA check |
| `security` | Detecta secrets | detect-secrets |
| `lint` | Linting geral | shellcheck, codespell |
| `python-check` | Verifica código Python | black, flake8 |

### 2. CI Build (`ci-build.yml`)

**Trigger:** Push para branch `main` apenas

**Runner:** Self-hosted (Raspberry Pi 5)

**Labels necessárias:** `self-hosted`, `linux`, `arm64`, `rpi5`

**Jobs:**

| Job | Descrição |
|-----|-----------|
| `build` | Compila com CMake/Ninja |
| `test-unit` | Executa GoogleTest |
| `test-integration` | Testes com vcan0 |
| `build-summary` | Resumo final |

### 3. Release (`release.yml`)

**Trigger:** Push de tags `v*.*.*`

**Runner:** Self-hosted (Raspberry Pi 5)

**Artifacts gerados:**

| Arquivo | Descrição |
|---------|-----------|
| `speeduino-dash-{version}-arm64.tar.gz` | Tarball com binários |
| `speeduino-dash_{version}_arm64.deb` | Pacote Debian |

## Configurar Self-Hosted Runner

### Pré-requisitos

- Raspberry Pi 5 com Raspberry Pi OS 64-bit
- Conexão com internet
- Acesso sudo

### Instalação

```bash
# 1. Obter token do runner em:
#    GitHub → Settings → Actions → Runners → New self-hosted runner

# 2. Executar script de setup
./scripts/setup-github-runner.sh \
    https://github.com/Guiimartinho/rpi-speeduino-dashboard \
    <RUNNER_TOKEN>
```

### Verificar Status

```bash
# Status do serviço
sudo /opt/github-runner/svc.sh status

# Logs
sudo journalctl -u actions.runner.*.service -f

# No GitHub
# → Repo Settings → Actions → Runners
```

### Gerenciar Runner

```bash
# Parar
sudo /opt/github-runner/svc.sh stop

# Iniciar
sudo /opt/github-runner/svc.sh start

# Remover
cd /opt/github-runner
sudo ./svc.sh uninstall
./config.sh remove --token <TOKEN>
```

## Branch Protection

Configurações recomendadas para branch `main`:

```
☑ Require pull request before merging
  ☑ Require approvals: 1
  ☑ Dismiss stale reviews when new commits are pushed
☑ Require status checks to pass before merging
  ☑ Require branches to be up to date
  Status checks:
    - lint-summary (ci-lint.yml)
☑ Do not allow bypassing the above settings
```

## Criar Release

```bash
# 1. Atualizar versão (se necessário)

# 2. Criar tag
git tag -a v1.0.0 -m "Release v1.0.0"

# 3. Push tag
git push origin v1.0.0

# 4. O workflow de release será executado automaticamente
```

### Versioning

Seguimos [Semantic Versioning](https://semver.org/):

- `v1.0.0` - Release estável
- `v1.0.0-beta.1` - Pre-release (beta)
- `v1.0.0-rc.1` - Release candidate

## Troubleshooting

### Runner offline

```bash
# Verificar serviço
sudo systemctl status actions.runner.*.service

# Reiniciar
sudo /opt/github-runner/svc.sh stop
sudo /opt/github-runner/svc.sh start

# Verificar logs
sudo journalctl -u actions.runner.*.service -n 100
```

### Build falha com dependências

```bash
# No runner, instalar dependências
sudo apt-get update
sudo apt-get install -y cmake ninja-build qt6-base-dev libzmq3-dev
```

### vcan não funciona

```bash
# Verificar módulo
lsmod | grep vcan

# Carregar módulo
sudo modprobe vcan

# Verificar interface
ip link show vcan0
```

### Permissões insuficientes

```bash
# Verificar sudoers
sudo cat /etc/sudoers.d/github-runner

# Recriar se necessário
sudo rm /etc/sudoers.d/github-runner
# Executar setup novamente
```

## Custos

| Componente | Custo |
|------------|-------|
| GitHub Actions (GitHub-hosted) | Grátis (2000 min/mês) |
| Self-hosted runner | Grátis (seu hardware) |
| GitHub Releases storage | Grátis |

## Monitoramento

### Métricas importantes

- Tempo médio de build
- Taxa de falha de builds
- Cobertura de testes
- Tempo de resposta do runner

### Notificações

Para adicionar notificações (Slack, Discord, email), edite o job `build-summary` em `ci-build.yml`:

```yaml
- name: Notify on failure
  if: failure()
  uses: 8398a7/action-slack@v3
  with:
    status: failure
    webhook_url: ${{ secrets.SLACK_WEBHOOK }}
```
