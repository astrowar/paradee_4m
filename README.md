# Paradee 4M (pt-BR / Brazilian Portuguese)

> **Um modelo de texto para fala (TTS) em português brasileiro ultra-compacto com ~3,6M de parâmetros, destilado do Kokoro-82M. Apenas ~3,9 MB em ONNX int8, executando a >40x tempo real em uma única thread de CPU.**
> 
> *(A ~3.6M-parameter ultra-compact Brazilian Portuguese text-to-speech model distilled from Kokoro-82M. ~3.9 MB in int8 ONNX, running >40x real-time on a single CPU thread.)*

Este repositório é um fork oficial do [Paradee pt-BR](https://github.com/astrowar/paradee_pt) focado em **máxima miniaturização** para dispositivos embarcados (ESP32 com PSRAM, Raspberry Pi, relógios, wearables, extensões de navegadores e edge computing).

---

## Comparativo de Arquitetura: Paradee 8M vs Paradee 4M

| Componente | Paradee 8M (`paradee_pt`) | **Paradee 4M (`paradee_4m`)** | Redução |
| :--- | :--- | :--- | :--- |
| **TextStudent Preset** | `s+mlp` | **`xs+mlp`** (ou `4m+mlp`) | **-58.6%** |
| *BERT (Albert)* | 256h / 6l / 4att (0.85M) | **128h / 4l / 4att (0.29M)** | -65.8% |
| *ProsodyPredictor* | 192d / 3l (2.16M) | **128d / 2l (0.83M)** | -61.5% |
| *TextEncoder* | 192ch / 3l (0.81M) | **128ch / 2l (0.29M)** | -64.2% |
| *ASR Projection* | 192->512 MLP (0.36M) | **128->512 MLP (0.33M)** | -8.3% |
| **Total TextStudent** | **4.23M** | **1.75M** | **-58.6%** |
| **VDecoder Preset** | `A` + Head `v2` | **`D` + Head `v2`** | **-49.6%** |
| *Trunk (AdaIN)* | Width 256, UIC 128 (2.66M) | **Width 128, UIC 64 (0.92M)** | -65.4% |
| *Head v2 (ConvNeXt)* | In 512, Dim 160 (0.99M) | **In 256, Dim 160 (0.92M)** | -7.1% |
| **Total VDecoder** | **3.65M** | **1.84M** | **-49.6%** |
| **Total de Parâmetros** | **7.88M** | **3.59M** | **-54.4%** |
| **Tamanho ONNX (int8)** | **8.46 MB** | **3.91 MB** | **-53.8%** |
| **Velocidade (1 thread CPU)** | ~25x tempo real | **>40x tempo real** | **+60%** |

---

## Características Técnicas

- **Sub-4MB ONNX:** O arquivo quantizado `paradee_int8.onnx` ocupa exatamente **3,91 MB**, tornando viável o armazenamento direto em Flash/SPIFFS ou download instantâneo via Web.
- **Português Brasileiro Nativo (pt-BR):** Treinado com cobertura prosódica completa (Clarice Lispector, TigreGótico, Gutenberg pt-BR) e suporte às vozes **`pf_dora`** (feminina) e **`pm_alex`** (masculina).
- **Sem Perda de Expressividade:** Mantém a projeção não-linear de ASR (MLP) e o vocoder de taxa de quadros (Head v2 baseado no Vocos com banco de senoides por F0), preservando clareza fonética e entonação natural.
- **Zero Heap no ESP32:** Compatível com o módulo nativo **`phonemas_ptbr`**, permitindo inferência completa sem bibliotecas externas pesadas de G2P.

---

## Como Treinar o Paradee 4M

O treinamento utiliza o mesmo dataset de sentenças e shards do professor gerados pelo Kokoro-82M.

### 1. Treinar o Estudante de Texto (`TextStudent - xs+mlp`)

Treina a predição de duração, curvas F0/energia e o alinhamento de texto para o espaço de ASR:

```bash
cd training

# Voz feminina (Dora)
python scripts/train_text.py xs+mlp --steps 10000 --voice ef_dora.pt --tag _dora_4m

# Voz masculina (Alex)
python scripts/train_text.py xs+mlp --steps 10000 --voice pm_alex.pt --tag _alex_4m
```

### 2. Treinar o Decodificador Acústico (`VDecoder - D + Head v2`)

Treina o trunk leve AdaIN (Preset D) com o cabeçote neural ConvNeXt (Head v2):

```bash
cd training

# Treinamento com GAN e perdas espectrais multi-resolução
python scripts/train_vhead.py --preset D --head v2 --steps 8000 --tag _4m
```

### 3. Exportar para Grafo Único ONNX int8 (< 4.0 MB)

Gera o grafo ONNX otimizado para navegador e CPU:

```bash
cd training

python scripts/export_paradee.py checkpoints/vh_4m/last.pt \
    --text checkpoints/text_xs+mlp_dora_4m/last.pt \
    --text-preset xs+mlp \
    --dec-preset D \
    --head v2 \
    --out models/paradee_ptbr_dora_4m
```

---

## Validação de Arquitetura e Exportação

Para validar localmente ou no servidor de GPU que o pipeline gera o modelo com menos de 4 MB e que a inferência ONNX Runtime funciona perfeitamente:

```bash
python training/scripts/test_export_4m.py
```

Saída esperada:
```text
============================================================
VERIFICANDO ARQUITETURA E ORÇAMENTO DE PARÂMETROS DO PARADEE 4M
============================================================
TextStudent (xs+mlp): 1.75M parâmetros (1,749,158 params)
VDecoder (D + head v2): 1.84M parâmetros (1,842,525 params)

TOTAL DE PARÂMETROS: 3.59M (3,591,683 params)
FP32 ONNX Size: 16.89 MB
INT8 ONNX Size: 3.91 MB
>> SUCESSO: Tamanho do modelo está estritamente abaixo de 4.0 MB! <<
Inference test SUCCESS! Output audio shape: (1, 88800)
============================================================
```

---

## Uso e Inferência

```python
import onnxruntime as ort
import numpy as np
import json
import soundfile as sf
from misaki.espeak import EspeakG2P

# 1. Carrega modelo ONNX 4M
sess = ort.InferenceSession("models/paradee_ptbr_dora_4m/paradee_int8.onnx", providers=["CPUExecutionProvider"])
vocab = json.load(open("models/paradee_ptbr_dora_4m/config.json"))["vocab"]

# 2. G2P em português
g2p = EspeakG2P(language="pt-br")
phonemes, _ = g2p("Olá, este é o novo modelo Paradee 4M falando português brasileiro.")
tokens = [0] + [vocab[c] for c in phonemes if c in vocab][:510] + [0]

# 3. Síntese
audio = sess.run(None, {
    "input_ids": np.array([tokens], dtype=np.int64),
    "speed": np.array([1.0], dtype=np.float32)
})[0][0]

sf.write("saida_4m.wav", audio, 24000)
print("Áudio sintetizado com sucesso pelo Paradee 4M!")
```

---

## Licença

Distribuído sob a licença [Apache 2.0](LICENSE).
Baseado no trabalho original de [Sahil Mahendrakar (Paradee)](https://github.com/sahilmahendrakar/paradee) e [Kokoro-82M](https://huggingface.co/hexgrad/Kokoro-82M).
