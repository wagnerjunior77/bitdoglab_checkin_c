Este projeto desenvolvido na Bitdoglab, consiste em um sistema de registro de entrada manual para um prédio de 5 andares, utilizando a placa Raspberry Pi Pico W e a linguagem de programação C. O sistema inclui feedback visual através de uma matriz de LEDs e um display OLED, exibindo o status de ocupação de cada andar.

🛠️ *Tecnologias Utilizadas*

Placa de desenvolvimento: BitDogLab (RP2040 - Raspberry Pi Pico W)

Linguagem: C

Protocolos: I²C, GPIO, Wi-Fi (modo Access Point)

Interfaces: Display OLED (SSD1306), matriz de LEDs WS2812, LEDs RGB, botões físicos

Ambiente de desenvolvimento: VS Code, PlatformIO

🚀 *Funcionalidades*

- Interface Web Local: Permite visualização e atualização remota da ocupação dos andares por meio de um Access Point Wi-Fi local.

- Exibição OLED: Mostra o número de pessoas presentes no andar selecionado.

- Matriz de LEDs WS2812: Representa visualmente a ocupação, com cada LED indicando até 10 pessoas por andar.

- LEDs RGB: Indicam o status dos andares (vermelho: vazio, apagado: ocupado).

- Controle por Botões Físicos: Navegação entre andares para visualização rápida e controle manual.

🔧 Instalação e Configuração

*Pré-requisitos*

- Neste repositório é possível entender quase tudo sobre a BitDogLab: https://github.com/BitDogLab/BitDogLab

- Firmware da BitDogLab com várias bibliotecas: https://github.com/BitDogLab/BitDogLab/tree/main/Firmware

- Extensão Raspberry Pi Pico no VSCode para instalar o SDK do Raspberry Pi Pico W
![image](https://github.com/user-attachments/assets/3ebb23d4-ecf1-4b16-b4b1-6f5758f96bd8)

- Extensão PlatformIO no VSCode para instalar o SDK do Raspberry Pi Pico W (OPCIONAL)
![image](https://github.com/user-attachments/assets/1bf2a436-9b99-460b-b723-7b52076ed2aa)

- Tutorial no Youtube de como configurar a BitDogLab no VSCode: https://www.youtube.com/watch?v=cMtbuvkkF5c

- Clone o repositório: git clone https://github.com/wagnerjunior77/bitdoglab_checkin_c.git

- Depois importe o projeto com a SDK mais recente do Pi Pico

![image](https://github.com/user-attachments/assets/0dab3957-1c4f-49e8-92bd-fc7c961b93b5)

- Clique em Configure CMake e depois Compile Project

![asdasdasd](https://github.com/user-attachments/assets/82953dbf-2c8d-43fe-b6cd-bae5b5863097)

Após isso você terá o arquivo .uf2 só para jogar na BitDogLab após conectar ela via USB e rodar o sistema.

📝 *Utilização*

- Inicialização

- Ligue a BitDogLab.

- O sistema entrará automaticamente em modo Access Point (AP).

- Conecte-se à rede Wi-Fi gerada pelo dispositivo (SSID: BitDog, senha: 12345678) [ou altere no código para o nome e senha que você quiser].

- Interface Web

- Abra o navegador e acesse: http://192.168.4.1

- Utilize a interface para adicionar, remover ou definir o número de pessoas em cada andar.

- Interface Física

- Botões: Use os botões físicos para navegar entre os andares.

- OLED: Exibe a ocupação do andar selecionado.

- Matriz de LEDs: Visualização rápida da ocupação (cada LED representa 10 pessoas).

- LEDs RGB: Indicadores rápidos (vermelho = andar vazio).

🚧 Melhorias Futuras

Modularização maior do código.

Inclusão de sensores de presença para atualização automática.

Melhoria do alcance do Wi-Fi para ambientes maiores.

