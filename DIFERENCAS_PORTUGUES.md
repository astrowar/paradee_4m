# Paradee: Diferenças entre a Versão Original (Git) e a Versão Adaptada para Português (pt-BR)

Este documento detalha todas as diferenças arquiteturais, de dados, de código e de infraestrutura entre o repositório original do **Paradee** (voltado para inglês) e a versão adaptada e treinada para **Português Brasileiro (pt-BR)** com a voz **Dora (`pf_dora`)**.

---

## 1. Visão Geral e Contexto

### O Paradee Original
- **Objetivo:** Destilar o modelo [Kokoro-82M](https://huggingface.co/hexgrad/Kokoro-82M) (82 milhões de parâmetros, ~325 MB) em um modelo ultra-compacto de **7.88M parâmetros** (~8.5 MB em int8 ONNX).
- **Idioma original:** Inglês americano (`af_heart`, `am_puck`) e inglês britânico (`bf_isabella`, `bm_fable`).
- **Hardware de referência original:** MacBook Pro com chip Apple Silicon (M4 Pro), utilizando o backend `mps` (Metal Performance Shaders) do PyTorch.
- **Corpus de treinamento:** 12.000 sentenças em inglês extraídas do WikiText-103 (`data/wikitext_sents.json`).

### A Versão Adaptada (pt-BR)
- **Objetivo:** Criar a primeira versão do Paradee nativa em **Português Brasileiro**, preservando a pegada ultra-leve de **8.46 MB** e taxa de execução de **~25x tempo real em 1 thread de CPU**, utilizando o voicepack da **Dora (`pf_dora.pt`)**.
- **Infraestrutura utilizada:** Servidor remoto Linux Ubuntu com **2x GPUs NVIDIA GeForce RTX 3090** (24 GB VRAM cada) utilizando PyTorch com aceleração **CUDA**.
- **Corpus de treinamento:** 12.000 sentenças em português brasileiro curadas foneticamente e literariamente (`data/portugues_sents.json`).

---

## 2. Matriz Comparativa Resumida

| Característica | Versão Original no Git (Inglês) | Versão Adaptada (Português pt-BR) |
| :--- | :--- | :--- |
| **Idioma Principal** | Inglês (`en-US` / `en-GB`) | Português Brasileiro (`pt-BR`) |
| **Código Kokoro (`lang_code`)** | `"a"` (American) ou `"b"` (British) | `"p"` (Brazilian Portuguese) |
| **G2P (Grapheme-to-Phoneme)** | `misaki.en.G2P` + `EspeakFallback` | `misaki.espeak.EspeakG2P(language="pt-br")` |
| **Voz Padrão** | `af_heart` (feminina, US) | `pf_dora` (feminina, BR) |
| **Dataset de Treino** | `wikitext_sents.json` (WikiText-103) | `portugues_sents.json` (TigreGótico + Clarice + Gutenberg) |
| **Dispositivo Padrão** | Hardcoded para Apple `mps` | Dinâmico: `cuda` (multi-GPU) / `mps` / `cpu` |
| **Geração de Shards** | Sequencial monotarefa no `mps` | Paralela particionada em 2 GPUs RTX 3090 |
| **Tamanho do ONNX int8** | 8.5 MB | **8.46 MB** |
| **Velocidade (1 thread CPU)** | ~60x RT (M4 Pro) / ~25x RT (x86_64) | **~25x tempo real** em CPU x86_64 |
| **Sentenças de Avaliação** | Frases em inglês do *Chickadee* | Frases foneticamente ricas em português (`SENTS_PT`) |

---

## 3. Detalhamento das Alterações por Componente

### 3.1. Pipeline de Texto e G2P (Grapheme-to-Phoneme)

Na versão original, a conversão de texto para fonemas dependia exclusivamente do pacote `misaki.en`, que carrega léxicos em inglês com regras fonológicas norte-americanas e britânicas.

**Na versão pt-BR:**
1. A conversão de texto utiliza `misaki.espeak.EspeakG2P(language="pt-br")` integrado ao motor do `espeak-ng`.
2. O vocabulário de fonemas gerado pelo Kokoro para o português contém **114 tokens** ativos (como vogais nasais `ɐ̃`, `oŋ`, `Aŋ`, consoantes palatais `ʧ`, `ʤ`, vibrantes múltiplas `x`, etc.).
3. O modelo `TextStudent` (baseado em ALBERT compacto) manteve sua matriz de embedding (`n_token=178`), permitindo absorver os símbolos fonéticos em português sem alteração estrutural no grafo ONNX.
4. Foi gerado o arquivo `config.json` contendo o mapa de `vocab` correspondente para permitir decodificação direta via ONNX Runtime sem necessidade de carregar o checkpoint do PyTorch.

---

### 3.2. Curadoria e Construção do Corpus (`build_dataset.py`)

No repositório original havia apenas o arquivo `wikitext_sents.json` estático. Para o português, foi desenvolvido um script dedicado de curadoria fonética e sintática (`build_dataset.py`):

1. **Fontes de dados combinadas:**
   - **TigreGótico (`TigreGotico/portuguese-sentences-synthetic-g2p`):** Sentenças balanceadas para ampla cobertura de difones e fonemas raros da língua portuguesa.
   - **Literatura Brasileira (Clarice Lispector):** Extração de sentenças literárias com prosódia fluida, diálogos e estruturas oracionais naturais.
   - **Project Gutenberg (pt):** Textos clássicos em domínio público em língua portuguesa.
2. **Critérios de filtragem aplicados:**
   - Comprimento controlado entre 20 e 160 caracteres (ideal para os blocos de duração do Paradee).
   - Remoção de caracteres especiais não-fonetizáveis (`[]{}<>|_#@/\\*~`^=+$%&`).
   - Eliminação de URLs, metadados e duplicações (case-insensitive).
   - O resultado final totalizou **12.000 sentenças** salvas em `data/portugues_sents.json`.

---

### 3.3. Adaptação dos Scripts de Treinamento (`training/scripts/`)

#### A. `common.py`
- **Original:** Função `load_teacher(device="cpu", voice="af_heart", fuse=True)` fixa com `lang_code="a"` e frases de avaliação em inglês (`SENTS`).
- **Modificação pt-BR:**
  - Adicionado suporte a `lang_code` (padrão `"p"` para português).
  - Adicionado `SENTS_PT` com frases de aferição fonética em português (ex.: *"O modelo Paradee é um sintetizador de voz leve e rápido"*).
  - Suporte ao carregamento direto de arquivos de voz `.pt` arbitrários (como `pf_dora.pt` ou caminhos absolutos), além das vozes do hub do Kokoro.

#### B. `gen_teacher.py`
- **Original:** Executava o professor apenas em `DEV = "mps"`, fixo em 12.000 sentenças em inglês com `af_heart`.
- **Modificação pt-BR:**
  - Adicionada interface de linha de comando (`argparse`) com suporte a `--device`, `--lang`, `--voice`, `--sents`, `--shard`, `--start-shard` e `--end-shard`.
  - **Paralelismo multi-GPU:** Permitiu particionar a geração em fatias independentes:
    - GPU 0 processou shards `0000` a `0011` (6.000 sentenças).
    - GPU 1 processou shards `0012` a `0023` (6.000 sentenças).
  - Adicionada proteção de índice para tensores de estilo:
    ```python
    idx_ref = min(max(0, T - 3), ref.shape[0] - 1)
    ```
    evitando que sentenças com contagem de fonemas atípica causassem erro de `IndexError` no tensor de estilo do Kokoro.
  - Gravação atômica dos shards via arquivo temporário para evitar corrupção em caso de interrupção.

#### C. `train_text.py` (Lado do Texto - 4.23M parâmetros)
- **Original:** Argumento `--device default="mps"`, avaliação fixa com `af_heart` e frases em inglês.
- **Modificação pt-BR:**
  - Detecção automática de aceleração: `cuda` se disponível, senão `mps`, senão `cpu`.
  - Suporte aos parâmetros `--lang p` e `--voice pf_dora.pt`.
  - Treinamento executado por **8.000 passos** com batch size 32 na GPU 0 (RTX 3090), convergindo com perda final de **0.2835** (duração: 0.040, f0: 0.044, n: 0.139, asr: 0.059).

#### D. `train_vhead.py` & `vhead.py` (Decodificador Acústico VDecoder v2 - 3.85M parâmetros)
- **Original:** Treinamento padrão no M4 Pro com 8 shards e device `mps`.
- **Modificação pt-BR:**
  - Suporte completo a `cuda:1` (GPU 1 em paralelo com o treino de texto na GPU 0).
  - Inicialização a partir dos pesos pré-treinados do tronco (`pretrained_decoder.pt`) e ajuste fino da cabeça `SourceFilterHead2` nos dados da voz Dora.
  - Treinamento executado por **5.000 passos** com perdas espectrais e estatísticas harmônicas (`--harm-weight 2 --mod-weight 5`), atingindo DTW L1 de **0.5754**.

#### E. `regen_audio.py`
- Adicionado suporte a `multiprocessing.Pool` com `--workers` e parâmetros de voz e idioma para regerar amostras no formato numérico canônico de CPU quando necessário.

---

### 3.4. Exportação e Quantização ONNX (`export_paradee_v1_1.py`)

A exportação unifica os dois módulos treinados em um **único grafo ONNX de ponta a ponta**:
- **Grafo Unificado:** `input_ids [1, T]` (int64) + `speed [1]` (float32) $\rightarrow$ `waveform [1, S]` (float32 a 24 kHz).
- **Substituição de Operações Complexas:** A FFT/iSTFT e os bancos harmônicos foram substituídos por convoluções 1D pré-computadas (`ConvSTFT`), eliminando incompatibilidades de operadores complexos em browsers e runtimes leves.
- **Quantização:**
  - Matrizes lineares e convoluções quantizadas para **int8** simétrico por canal.
  - Vetores de escala e estilo em **fp16**.
  - Bases de Fourier geradas dinamicamente na inicialização do grafo.
- **Resultado:** Redução de **34.8 MB (fp32)** para **8.46 MB (int8)** sem perda perceptível de qualidade sonora.

---

## 4. Estrutura de Arquivos Resultantes

```
dev/paradee/
├── models/
│   └── paradee_ptbr_dora/
│       ├── config.json               # Vocabulário Kokoro pt-BR (114 fonemas)
│       ├── paradee.onnx               # Grafo ONNX unificado fp32 (34.8 MB)
│       ├── paradee_int8.onnx          # Grafo ONNX unificado int8 (8.46 MB)
│       └── held_*_paradee_int8.wav   # Áudios de avaliação held-out
├── paradee_pt_frase.wav              # Áudio de referência inicial
├── teste_pf_dora_step3500.wav        # Teste intermediário (passo 3.500)
├── teste_pf_dora_step8000.wav        # Teste final PyTorch (passos 8.000 + 5.000)
└── teste_onnx_int8_ptbr.wav          # Teste final executado direto via ONNX Runtime int8
```

---

## 5. Como Executar a Inferência com a Versão pt-BR

Abaixo o exemplo mínimo para utilizar o modelo exportado em qualquer ambiente Python sem depender de PyTorch ou GPU:

```python
import json
import numpy as np
import onnxruntime as ort
import soundfile as sf
from misaki.espeak import EspeakG2P

# 1. Carregar fonetizador e vocabulário
g2p = EspeakG2P(language="pt-br")
with open("models/paradee_ptbr_dora/config.json", encoding="utf-8") as f:
    vocab = json.load(f)["vocab"]

# 2. Carregar a sessão ONNX (apenas 8.5 MB na memória)
so = ort.SessionOptions()
so.intra_op_num_threads = 1
session = ort.InferenceSession(
    "models/paradee_ptbr_dora/paradee_int8.onnx",
    sess_options=so,
    providers=["CPUExecutionProvider"]
)

# 3. Processar texto
texto = "O modelo Paradee em português brasileiro é leve, rápido e eficiente."
phonemes, _ = g2p(texto)

# 4. Converter fonemas para IDs com tokens [0] nas pontas
ids = [0] + [vocab[c] for c in phonemes if c in vocab][:510] + [0]

# 5. Gerar áudio
audio = session.run(None, {
    "input_ids": np.array([ids], dtype=np.int64),
    "speed": np.array([1.0], dtype=np.float32)
})[0][0]

# 6. Salvar áudio final (24 kHz)
sf.write("saida_paradee_ptbr.wav", audio, 24000)
```
