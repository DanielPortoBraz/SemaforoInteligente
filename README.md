# 🚦 Semáforo Inteligente

Projeto de um sistema embarcado que simula um semáforo inteligente com dois modos de operação: **normal** e **noturno**, utilizando o Raspberry Pi Pico e periféricos da placa BitDogLab.

---

## Descrição

O sistema alterna entre dois modos de funcionamento:

- **Modo Normal**: alternância entre verde, amarelo e vermelho com tempos fixos.
- **Modo Noturno**: cor amarela piscante, simulando menor fluxo de tráfego.

---

## Objetivos

- Implementar controle de tempo e modos em um semáforo.
- Utilizar periféricos de I/O programáveis (PIO) para controle de LEDs endereçáveis.
- Aplicar lógica embarcada com o Raspberry Pi Pico.
- Aplicar conceitos de programação orientada a RTOS.

---

## Tecnologias Utilizadas

- Linguagem: C (Pico SDK)
- Microcontrolador: Raspberry Pi Pico
- PIO (Programável Input/Output) para controle dos LEDs
- Build System: CMake

---

## Estrutura do Projeto

```
SemaforoInteligente/
├── .vscode/                 # Configurações para o VSCode
├── build/                   # Diretório de build (gerado após compilação)
├── lib/                     # Bibliotecas auxiliares
├── ws2818b.pio              # Código PIO para LEDs WS2812B
├── SemaforoInteligente.c    # Lógica principal do semáforo
├── CMakeLists.txt           # Configuração do CMake
└── pico_sdk_import.cmake    # Inclusão do SDK do Pico
```

---

## Como Executar

### 1. Pré-requisitos

- [Pico SDK](https://github.com/raspberrypi/pico-sdk)
- [CMake](https://cmake.org/)
- Compilador para ARM Cortex-M0+ (como `arm-none-eabi-gcc`)

### 2. Clonar o Repositório

```bash
git clone https://github.com/DanielPortoBraz/SemaforoInteligente.git
cd SemaforoInteligente
```

### 3. Compilar o Projeto

```bash
mkdir build
cd build
cmake ..
make
```

### 4. Gravar no Raspberry Pi Pico

- Conecte o Pico ao PC com o botão BOOTSEL pressionado.
- Copie o `.uf2` gerado (dentro da pasta `build/`) para o dispositivo montado.

---

## Vídeo de Demonstração 

(https://youtube.com/playlist?list=PLaN_cHSVjBi-gO9XcC-Mh2WirtwcFZoGp&si=UypwQz6d_vikDFb2)

---

## Licença

Este projeto está sob a licença [MIT](LICENSE).

---

## Autor

Desenvolvido por [DanielPortoBraz](https://github.com/DanielPortoBraz)

