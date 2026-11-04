# Paradee (pt-BR / Brazilian Portuguese)

> **Um modelo de texto para fala (TTS) em português brasileiro com 8M de parâmetros, destilado do Kokoro-82M. Apenas 8,5 MB, roda 25x mais rápido que o tempo real em uma única thread de CPU.**
> 
> *(An 8M-parameter Brazilian Portuguese text-to-speech model distilled from Kokoro-82M. 8.5 MB, runs 25x real time on one CPU thread.)*

Este repositório é um fork do [Paradee](https://github.com/sahilmahendrakar/paradee) adaptado e treinado para **Português Brasileiro (pt-BR)** com suporte às vozes **Dora (`pf_dora` - feminina)** e **Alex (`pm_alex` - masculina)**.

- **Extremamente Pequeno:** O modelo int8 quantizado é um único arquivo ONNX de **8,46 MB**, comparado aos ~325 MB do Kokoro original.
- **Ultra-Rápido:** Executa a **~25x tempo real** em uma única thread de CPU (x86_64), sem necessidade de GPU.
- **Fidelidade ao Professor:** Destilado diretamente do `Kokoro-82M` (código de idioma `"p"` com vetores de estilo `pf_dora` e `pm_alex`).
- **Nativo para Navegador e Edge:** Totalmente compatível com ONNX Runtime Web, mobile e CPU padrão.

---

## Principais Mudanças em Relação à Versão Original em Inglês

Para adaptar o Paradee (originalmente concebido para inglês) para o português brasileiro, foram implementadas as seguintes alterações técnicas:

1. **G2P e Fonetização:**
   - *Original:* Usava `misaki.en` com dicionários e regras para inglês americano e britânico.
   - *Versão pt-BR:* Utiliza `misaki.espeak.EspeakG2P(language="pt-br")` integrado ao `espeak-ng`. O vocabulário fonético ativo mapeia todos os 114 símbolos fonéticos utilizados pelo Kokoro no pt-BR (incluindo vogais nasais `ɐ̃`, `oŋ`, `Aŋ` e consoantes africadas `ʧ`, `ʤ`).

2. **Corpus de Treinamento em Português (`build_dataset.py`):**
   - *Original:* 12.000 frases em inglês do WikiText-103 (`wikitext_sents.json`).
   - *Versão pt-BR:* Novo dataset de 12.000 sentenças em português brasileiro (`data/portugues_sents.json`), construído a partir de:
     - **TigreGótico:** Cobertura fonética balanceada com difones raros do pt-BR.
     - **Clarice Lispector:** Prosódia literária, diálogos e cadência natural da língua falada.
     - **Project Gutenberg (pt):** Obras clássicas em domínio público.

3. **Vozes e Estilo:**
   - *Original:* Vozes em inglês (`af_heart`, `am_puck`, `bf_isabella`, `bm_fable`).
   - *Versão pt-BR:* Modelos treinados com as vozes brasileiras **`pf_dora.pt`** (feminina) e **`pm_alex.pt`** (masculina) do Kokoro-82M.

4. **Infraestrutura e Aceleração de Hardware:**
   - *Original:* Hardcoded para Apple Silicon utilizando o backend `mps` do PyTorch.
   - *Versão pt-BR:* Suporte dinâmico a `cuda`, `mps` e `cpu`. Os scripts foram paralelizados para rodar em servidores com múltiplas GPUs NVIDIA (treinado com 2x RTX 3090).

5. **Documentação de Diferenças Técnicas:**
   - Consulte [DIFERENCAS_PORTUGUES.md](DIFERENCAS_PORTUGUES.md) para a análise linha a linha das mudanças nos scripts de treinamento.

6. **Módulo Embarcado C/C++ Autocontido para ESP32 (`phonemas_ptbr/`):**
   - Para projetos em microcontroladores (ESP32, Arduino, FreeRTOS) sem interpretador Python nem arquivos `.dat`, o repositório inclui o módulo nativo **[`phonemas_ptbr/`](phonemas_ptbr/)**.
   - **Zero Heap / Zero Filesystem:** Opera 100% em ponteiros `char*` com buffers na stack e tabelas pré-compiladas em Flash (`PROGMEM`), convertendo texto diretamente nos IDs inteiros dos tokens do Paradee.

---

## Início Rápido (Quick Start)

### Instalação

```bash
pip install git+https://github.com/astrowar/paradee_pt
pip install misaki[espeak] onnxruntime soundfile
```

### Linha de Comando (CLI)

```bash
# Sintetizar áudio em português brasileiro
python -m paradee "Olá! O Paradee agora fala português brasileiro com apenas oito megabytes." --lang pt -o ola.wav
```

### Via Python

```python
from paradee import Paradee, SAMPLE_RATE
import soundfile as sf

# Carrega o modelo int8 de 8.5 MB
tts = Paradee(
    model_path="models/paradee_ptbr_dora/paradee_int8.onnx",
    config_path="models/paradee_ptbr_dora/config.json",
    lang="pt"
)

audio = tts("O modelo Paradee em português brasileiro é leve, rápido e eficiente.")
sf.write("saida.wav", audio, SAMPLE_RATE)
```

Com marcação temporal das palavras (`with_timestamps`):

```python
audio, words = tts.with_timestamps("O modelo Paradee fala português brasileiro.")
# words: [("O", 0.05, 0.12), ("modelo", 0.12, 0.54), ("Paradee", 0.54, 1.02), ...]: (texto, inicio, fim) em segundos
```

---

## Executando Diretamente via ONNX Runtime

O modelo exportado é um único grafo ONNX independente que não requer PyTorch nem GPU:

```python
import json
import numpy as np
import onnxruntime as ort
import soundfile as sf
from misaki.espeak import EspeakG2P

# 1. Carregar G2P e vocabulário de fonemas pt-BR
g2p = EspeakG2P(language="pt-br")
with open("models/paradee_ptbr_dora/config.json", encoding="utf-8") as f:
    vocab = json.load(f)["vocab"]

# 2. Inicializar sessão ONNX (8.5 MB)
so = ort.SessionOptions()
so.intra_op_num_threads = 1  # 1 única thread de CPU
session = ort.InferenceSession("models/paradee_ptbr_dora/paradee_int8.onnx", sess_options=so, providers=["CPUExecutionProvider"])

# 3. Converter texto para IDs de tokens
texto = "O modelo Paradee roda vinte e cinco vezes mais rápido que o tempo real."
phonemes, _ = g2p(texto)
ids = [0] + [vocab[c] for c in phonemes if c in vocab][:510] + [0]

# 4. Inferência
audio = session.run(None, {
    "input_ids": np.array([ids], dtype=np.int64),
    "speed": np.array([1.0], dtype=np.float32)
})[0][0]

sf.write("teste_ptbr.wav", audio, 24000)
```

---

## Como Funciona a Arquitetura

O Paradee destila a arquitetura do Kokoro mantendo a mesma separação em duas metades compactas:

```
Texto em pt-BR
      │
      ▼
[ EspeakG2P (pt-br) ] ────────► Fonemas (114 símbolos)
      │
      ▼
[ TextStudent (4.23M) ] ──────► Duração, Pitch (F0), Loudness (N) e ASR Features
      │
      ▼
[ VDecoder Head (3.65M) ] ────► Síntese harmônica (80 fps) + Ruído de pulso
      │
      ▼
Áudio Waveform (24 kHz)
```

1. **Lado do Texto (`TextStudent` - 4.23M parâmetros):**
   Um modelo ALBERT compacto com camadas convolucionais que prevê durações de fonemas, curvas de frequência fundamental ($F_0$) e representações intermediárias alinhadas ao professor Kokoro.
2. **Decodificador Acústico (`VDecoder` - 3.65M parâmetros):**
   Opera a 80 quadros por segundo sem camadas pesadas de upsampling. Para componentes vozeadas, constrói ondas senoidais limpas a partir da curva de pitch e harmônicos associados; para componentes não-vozeadas (fricativas, explosões), molda ruído calibrado em tempo e frequência.
3. **Exportação Sem Operadores Complexos:**
   Toda a geração STFT/iSTFT foi reescrita utilizando convoluções 1D pré-computadas (`ConvSTFT`), garantindo execução sem falhas em qualquer engine ONNX Runtime.

---

## Treinando Novas Vozes ou Modelos

O código de treinamento completo encontra-se no diretório [`training/`](training/):

1. **Construção do dataset de sentenças:**
   ```bash
   python scripts/build_dataset.py
   ```
2. **Geração de dados do professor Kokoro:**
   ```bash
   python scripts/gen_teacher.py 12000 --lang p --voice pf_dora.pt --device cuda
   ```
3. **Treino do modelo de texto:**
   ```bash
   python scripts/train_text.py s+mlp --steps 8000 --lang p --voice pf_dora.pt --device cuda:0
   ```
4. **Treino do decodificador acústico:**
   ```bash
   python scripts/train_vhead.py --head v2 --fine-stft --harm-weight 2 --mod-weight 5 --shards 24 --device cuda:1 --steps 5000
   ```
5. **Exportação ONNX (fp32 e int8):**
   ```bash
   python scripts/export_paradee_v1_1.py checkpoints/vh/last.pt --text checkpoints/text_s+mlp/last.pt --out models/paradee_ptbr_dora
   ```

---

## Licença

Apache 2.0 (a mesma licença do Kokoro e do Paradee original).

## Agradecimentos

- [Paradee](https://github.com/sahilmahendrakar/paradee) por Sahil Mahendrakar.
- [Kokoro-82M](https://huggingface.co/hexgrad/Kokoro-82M) por hexgrad.
- [misaki](https://github.com/hexgrad/misaki) e [espeak-ng](https://github.com/espeak-ng/espeak-ng) pela infraestrutura de conversão G2P.
