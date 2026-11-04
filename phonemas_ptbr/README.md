# phonemas_ptbr

Motor G2P (Texto $\rightarrow$ Fonemas) em **C/C++ Puro, 100% Autônomo e Ultra-Enxuto** para Português Brasileiro (pt-BR), projetado especificamente para **ESP32, microcontroladores e sistemas embarcados**.

---

## Zero Dependências Externas (Pronto para ESP32)

- **Zero arquivos `.dat`, `.json` ou de disco:** Não necessita de SPIFFS, LittleFS, SD Card ou qualquer sistema de arquivos.
- **Zero bibliotecas compartilhadas (`.so` / `.dll`):** Não utiliza `dlopen`, `libespeak-ng`, `fstream` ou interpretadores.
- **Zero alocações na Heap (na API C para microcontroladores):** Suporta gravação direta de tokens numéricos em buffers estáticos/locais da stack.
- **Tabela de Exceções embutida em Flash (`PROGMEM` / `.rodata`):** Contém 357 palavras irregulares com busca binária em $\mathcal{O}(\log N)$.
- **Silabificação e Acentuação Métrica:** Mapeia núcleos vocálicos (ditongos e monotongos) posicionando acento primário (`ˈ`) no núcleo tônico e secundário (`ˌ`) na sílaba inicial em polissílabos e derivados em `-mente`.
- **Vocabulário de Tokens Paradee Embutido:** Mapeamento dos 114 tokens do Kokoro/Paradee pré-compilado em C (`include/embedded_vocab.h`).
- **Consumo de Memória:**
  - **Flash (ROM):** ~18 KB no total.
  - **RAM (SRAM):** Menos de 2 KB durante a execução.

---

## Estrutura do Módulo

```
phonemas_ptbr/
├── include/
│   ├── phonemas_ptbr.h          # API C e C++ minimalista (sem caminhos de arquivos)
│   ├── embedded_vocab.h         # 114 tokens do Paradee embutidos em Flash
│   └── embedded_exceptions.h    # 357 exceções pt-BR com busca binária em Flash
├── src/
│   ├── phonemas_ptbr.cpp        # Motor de regras fonológicas pt-BR (C/C++ puro)
│   └── main.cpp                 # Utilitário CLI autônomo
├── tests/
│   └── test_main.cpp            # Testes unitários automatizados
├── tools/
│   ├── generate_embedded_data.py # Script de geração das tabelas embutidas
│   ├── exceptions_ptbr.json      # Base de exceções em JSON
│   └── discrepancias_excecoes.json # Relatório de auditoria de discrepâncias
├── CMakeLists.txt               # Build para desktop/Linux/Mac
└── README.md                    # Documentação
```

---

## Como Usar no ESP32 (ESP-IDF ou Arduino)

Basta copiar a pasta `include/` e o arquivo `src/phonemas_ptbr.cpp` para dentro do seu projeto ESP32 (ex: na pasta `components/phonemas_ptbr` no ESP-IDF ou na pasta do sketch no Arduino IDE).

### Exemplo em C Puro (Zero Alocações na Heap - Ideal para ESP32):

```c
#include "phonemas_ptbr.h"
#include <stdio.h>

void app_main(void) {
    // 1. Converter texto diretamente para tokens em buffer local (zero heap, zero init)
    int64_t tokens[128];
    int total = phonemas_ptbr_text_to_tokens("Olá mundo! Paradee fala português.", tokens, 128);
    
    printf("Gerados %d tokens diretamente em Flash/Stack!\n", total);
    // [0, 57, 54, 43, 16, 55, 63, ... 0]
    // Pronto para alimentar o tensor do modelo ONNX/TFLite!

    // 2. Se desejar a string de fonemas IPA:
    char* fonemas = phonemas_ptbr_translate("Olá mundo!");
    if (fonemas) {
        printf("Fonemas: %s\n", fonemas);
        phonemas_ptbr_free(fonemas);
    }
}
```

### Exemplo em C++:

```cpp
#include "phonemas_ptbr.h"
#include <iostream>

void loop() {
    // 1. Texto para Fonemas IPA
    std::string texto = "Ele explicou exatamente como o brasileiro aprende tecnologia.";
    std::string fonemas = phonemas_ptbr::text_to_phonemes(texto);
    std::cout << "Fonemas: " << fonemas << std::endl;

    // 2. Texto diretamente para Tokens Paradee (sem inicialização e sem arquivos .json!)
    std::vector<int64_t> tokens = phonemas_ptbr::text_to_tokens(texto);
    std::cout << "Total de tokens: " << tokens.size() << std::endl;
}
```

---

## Compilação e Teste no Desktop

```bash
cd phonemas_ptbr
mkdir -p build && cd build
cmake ..
make
./test_phonemas_ptbr
```

### Bateria de Sanitização e Estresse (ASan / UBSan / LSan):

Para compilar e rodar os testes de estresse com detecção de vazamentos e integridade de memória:

```bash
cmake -DENABLE_SANITIZERS=ON ..
make
./stress_test_phonemas_ptbr
```

**Resultado dos Testes:**
- **Ponteiros nulos e limites de borda:** Validados sem falhas.
- **Buffers de tamanho 1 a 64 bytes:** Truncamento seguro sem buffer overflow.
- **Ausência de vazamento de memória:** 10.000 iterações `malloc`/`free` verificadas via LeakSanitizer (0 leaks).
- **Throughput:** 50.000 frases processadas em 1,2 segundos (~24 microsegundos por frase).

### Testando com a CLI Autônoma (Sem argumentos extras):

```bash
./phonemas_ptbr "Olá mundo! O modelo Paradee fala português no ESP32."
```

**Saída:**
```text
========================================
Texto de Entrada : Olá mundo! O modelo Paradee fala português no ESP32.
Fonemas pt-BR    : olˈa mˈũŋdʊ! ʊ mˌodˈelʊ pˌaɾadˈey fˈalæ pˌoɾtuɡˈes nʊ ˈesp32.
========================================
Tokens Paradee (62 tokens embutidos em Flash):
[0, 57, 54, 156, 43, 16, 55, 156, 63, 17, 112, 46, 135, 5, 16, 135, 16, 55, 157, 57, 46, 156, 47, 54, 135, 16, 58, 157, 43, 125, 43, 46, 156, 47, 67, 16, 48, 156, 43, 54, 72, 16, 58, 157, 57, 125, 62, 63, 92, 156, 47, 61, 16, 56, 135, 16, 156, 47, 61, 58, 4, 0]
========================================
```
