# Motherboard Resource Checker

**Motherboard Resource Checker** é uma aplicação desktop nativa desenvolvida em C++ e Qt, projetada para mapear visualmente a placa-mãe de uma máquina, identificando slots (PCIe, M.2, RAM, SATA) e exibindo quais estão ocupados, juntamente com o modelo de CPU instalado.

A aplicação é altamente portátil e otimizada para rodar desde sistemas legados (como Windows XP e Windows 7) até os mais modernos (Windows 10/11).

---

## 🛠️ Recursos e Funcionalidades

- **Mapeamento Visual Dinâmico**: Renderiza um diagrama interativo da placa-mãe com slots destacados (RAM, PCIe, M.2, SATA).
- **Leitura de Baixo Nível**: Rastreia informações detalhadas de hardware usando APIs nativas do Windows, WMI e parsing de tabelas SMBIOS/DMI brutas.
- **Relação Física de Dispositivos**: Identifica quais dispositivos físicos (placas de vídeo, discos NVMe, etc.) estão acoplados a cada barramento PCIe.
- **Exibição do Processador (CPU)**: Detecta e exibe a descrição completa do processador instalado diretamente no socket visual.
- **100% Portátil**: Empacotado em um único executável de ~90MB contendo todas as dependências do Qt embutidas, extraídas de forma transparente e executadas em memória, sem necessidade de instalação.
- **Compatibilidade Retroativa Completa**: Projetado para rodar em Windows XP, 7, 8, 10 e 11.

---

## 🚀 Como Executar o Portable

Basta baixar o arquivo final:
- **`Motherboard Resource Checker.exe`**

Ao executá-lo:
1. O loader interno extrai silenciosamente as bibliotecas necessárias para uma pasta temporária em `%TEMP%`.
2. A aplicação principal é iniciada imediatamente.
3. Nas próximas execuções, a inicialização é instantânea, pois o loader detecta que os arquivos já foram descompactados.

---

## 💻 Estrutura do Projeto

A organização dos arquivos no repositório segue a estrutura abaixo:

- `/src`: Arquivos de código-fonte C++ (`.cpp` e `.h`).
  - `main.cpp`: Ponto de entrada do aplicativo GUI.
  - `motherboard_map.*`: Widget de mapa visual e gerenciamento de layout.
  - `slot_item.*`: Elementos gráficos individuais de cada slot da placa-mãe.
  - `hardware_scanner.*`: Motor de varredura do sistema (WMI, SetupAPI, SMBIOS).
  - `loader.cpp`: O loader portátil que descompacta a aplicação usando a biblioteca estática `miniz`.
  - `miniz.*`: Biblioteca leve de compactação/descompactação em C.
  - `types.h`: Modelos comuns de dados.
- `/resources`: Recursos visuais do aplicativo (imagens, ícones).
- `mb_resource_checker.pro`: Arquivo de configuração de projeto QMake.
- `build.bat`: Script de build automatizado do MSVC.
- `.gitignore`: Configuração de arquivos ignorados pelo Git.

---

## 🛠️ Como Compilar do Código Fonte

Para compilar o projeto manualmente no Windows, você precisará do **MSVC (Visual Studio)** e do **Qt 5.15.2** instalados.

1. Abra um prompt de comando do desenvolvedor do Visual Studio (Developer Command Prompt).
2. Navegue até a pasta do projeto.
3. Execute o script de build:
   ```cmd
   .\build.bat
   ```
O script se encarregará de:
- Inicializar o ambiente do compilador MSVC de 64 bits.
- Executar o `qmake` e o `nmake` para compilar o executável principal.
- Usar o `windeployqt` para coletar as dependências do Qt na pasta `release`.
- Compactar a pasta `release` em um arquivo `.zip`.
- Compilar o loader C++ (`loader.cpp` + `miniz.c`) incorporando o arquivo `.zip` como recurso e gerar o executável final `Motherboard Resource Checker.exe` na raiz do projeto.
