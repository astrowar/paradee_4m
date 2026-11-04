# Treinamento do Paradee (pt-BR / Português Brasileiro)

Este diretório contém o pipeline completo para destilar o modelo **Kokoro-82M** em português brasileiro para a arquitetura ultra-compacta do **Paradee (8M parâmetros, ~8,5 MB em int8 ONNX)**.

O pipeline suporta treinamento com aceleração **CUDA (NVIDIA)**, **MPS (Apple Silicon)** ou **CPU**, além de execução particionada em múltiplas GPUs.

---

## 0. Preparação do Ambiente

O gerenciamento de dependências é feito com o [uv](https://docs.astral.sh/uv/):

```bash
cd training
uv sync
```

O ambiente configurará automaticamente PyTorch, Torchaudio, Kokoro, Misaki (com espeak-ng) e ONNX Runtime.

---

## 1. Construção do Dataset de Sentenças (`build_dataset.py`)

O script `scripts/build_dataset.py` faz a curadoria de 12.000 frases em português brasileiro com ampla cobertura fonética e sintática a partir de três fontes (TigreGótico, Clarice Lispector e Project Gutenberg pt):

```bash
uv run python scripts/build_dataset.py
```

O arquivo resultante é salvo em `data/portugues_sents.json`.

---

## 2. Geração dos Dados do Professor (`gen_teacher.py`)

Gera os alinhamentos, durações, curvas de pitch ($F_0$), loudness ($N$) e representações intermediárias a partir do professor Kokoro-82M no idioma português (`--lang p`).

### Voz Feminina (`pf_dora`)
```bash
uv run python scripts/gen_teacher.py 12000 --lang p --voice pf_dora.pt --device cuda
```

### Voz Masculina (`pm_alex`)
```bash
uv run python scripts/gen_teacher.py 12000 --lang p --voice pm_alex.pt --device cuda
```

*Dica para servidores multi-GPU:* Você pode particionar os shards em paralelo entre as placas:
- GPU 0: `--start-shard 0 --end-shard 12 --device cuda:0`
- GPU 1: `--start-shard 12 --end-shard 24 --device cuda:1`

---

## 3. Treinamento do Lado do Texto (`train_text.py`)

Treina o `TextStudent` (4.23M parâmetros) para aprender a prever durações, curvas de frequência fundamental, loudness e embeddings de texto diretamente dos dados gerados:

```bash
# Treino padrão (8.000 passos na voz Dora)
uv run python scripts/train_text.py s+mlp --steps 8000 --lang p --voice pf_dora.pt --device cuda:0

# Treino para a voz Alex
uv run python scripts/train_text.py s+mlp --steps 8000 --lang p --voice pm_alex.pt --device cuda:0 --tag _alex
```

O checkpoint final é salvo em `checkpoints/text_s+mlp<tag>/last.pt`.

---

## 4. Treinamento do Decodificador Acústico (`train_vhead.py`)

Treina o `VDecoder` com a cabeça harmônica `SourceFilterHead2` operando a 80 frames por segundo com perdas espectrais e estatísticas harmônicas:

```bash
# Treino com a voz Dora
uv run python scripts/train_vhead.py --head v2 --fine-stft --harm-weight 2 --mod-weight 5 --shards 24 --device cuda:1 --steps 5000 --init-from pretrained_decoder.pt --tag _dora

# Treino com a voz Alex
uv run python scripts/train_vhead.py --head v2 --fine-stft --harm-weight 2 --mod-weight 5 --shards 24 --device cuda:1 --steps 5000 --init-from pretrained_decoder.pt --tag _alex
```

*Nota:* Se o arquivo `pretrained_decoder.pt` não estiver presente localmente, o script fará o download automático do checkpoint base no Hugging Face Hub.

---

## 5. Exportação e Quantização para ONNX (`export_paradee.py`)

Unifica o modelo de texto e o decodificador acústico em um único grafo ONNX de ponta a ponta e gera as versões **fp32** (34.8 MB) e **int8 quantizada** (8.46 MB), além do `config.json` com os fonemas correspondentes:

```bash
# Exportar modelo Dora
uv run python scripts/export_paradee.py checkpoints/vh_dora/last.pt --text checkpoints/text_s+mlp/last.pt --out models/paradee_ptbr_dora

# Exportar modelo Alex
uv run python scripts/export_paradee.py checkpoints/vh_alex/last.pt --text checkpoints/text_s+mlp_alex/last.pt --out models/paradee_ptbr_alex
```

---

## 6. Teste e Inferência com o Modelo Exportado

Após a exportação, você pode testar a síntese de qualquer texto diretamente com o modelo ONNX int8 de 8.5 MB:

```python
import json, numpy as np, onnxruntime as ort, soundfile as sf
from misaki.espeak import EspeakG2P

g2p = EspeakG2P(language="pt-br")
vocab = json.load(open("models/paradee_ptbr_alex/config.json"))["vocab"]
sess = ort.InferenceSession("models/paradee_ptbr_alex/paradee_int8.onnx", providers=["CPUExecutionProvider"])

texto = "Paradee é um sintetizador de voz ultra-leve em português brasileiro."
phonemes, _ = g2p(texto)
ids = [0] + [vocab[c] for c in phonemes if c in vocab][:510] + [0]

audio = sess.run(None, {
    "input_ids": np.array([ids], dtype=np.int64),
    "speed": np.array([1.0], dtype=np.float32)
})[0][0]

sf.write("teste_saida.wav", audio, 24000)
```
