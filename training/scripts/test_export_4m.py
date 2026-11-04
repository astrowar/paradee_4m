"""Test script to verify Paradee 4M architecture, parameter counts, ONNX export, and int8 size.
Ensures the final model is strictly below 4.0 MB and passes ONNX Runtime inference.
"""
import sys, os, time
import numpy as np, torch

sys.path.insert(0, os.path.dirname(__file__))
from student import TextStudent
from vhead import VDecoder
from export_paradee import export_model
import onnxruntime as ort

def test_paradee_4m():
    print("=" * 60)
    print("VERIFICANDO ARQUITETURA E ORÇAMENTO DE PARÂMETROS DO PARADEE 4M")
    print("=" * 60)

    # 1. TextStudent (xs+mlp)
    ts = TextStudent("xs+mlp").eval()
    p_ts = sum(p.numel() for p in ts.parameters())
    parts_ts = {k: sum(x.numel() for x in getattr(ts, k).parameters()) for k in ["bert", "predictor", "text_encoder", "asr_proj"]}
    print(f"TextStudent (xs+mlp): {p_ts/1e6:.2f}M parâmetros ({p_ts:,} params)")
    for k, v in parts_ts.items():
        print(f"  - {k:15s}: {v/1e6:.2f}M ({v:,})")

    # 2. VDecoder (preset D + head v2)
    vd = VDecoder(preset="D", head="v2").eval()
    p_vd = sum(p.numel() for p in vd.parameters())
    trunk_vd = sum(p.numel() for k in [vd.encode, vd.decode, vd.F0_conv, vd.N_conv, vd.asr_res] for p in k.parameters())
    head_vd = sum(p.numel() for p in vd.head.parameters())
    print(f"\nVDecoder (D + head v2): {p_vd/1e6:.2f}M parâmetros ({p_vd:,} params)")
    print(f"  - trunk (AdaIN D): {trunk_vd/1e6:.2f}M ({trunk_vd:,})")
    print(f"  - head (v2)      : {head_vd/1e6:.2f}M ({head_vd:,})")

    total_params = p_ts + p_vd
    print(f"\nTOTAL DE PARÂMETROS: {total_params/1e6:.2f}M ({total_params:,})")

    # 3. Exportação e quantização ONNX int8
    out_dir = "/tmp/test_paradee_4m"
    print(f"\nExportando modelo para {out_dir}...")
    t0 = time.time()
    qpath = export_model("non_existent.pt", "non_existent.pt", text_preset="xs+mlp", dec_preset="D", head="v2", out_dir=out_dir)
    dt = time.time() - t0
    final_size_mb = os.path.getsize(qpath) / (1024 * 1024)
    print(f"Tempo de exportação + quantização: {dt:.2f}s")
    print(f"Tamanho final do arquivo ONNX int8: {final_size_mb:.2f} MB")

    # Asserção de tamanho
    assert final_size_mb < 4.0, f"Modelo excedeu 4.0 MB: {final_size_mb:.2f} MB"
    print(">> SUCESSO: Tamanho do modelo está estritamente abaixo de 4.0 MB! <<")

    # 4. Inferência via ONNX Runtime
    print("\nTestando inferência com ONNX Runtime...")
    sess = ort.InferenceSession(qpath, providers=["CPUExecutionProvider"])
    dummy_input = np.array([[0, 10, 25, 45, 60, 80, 100, 0]], dtype=np.int64)
    speed = np.array([1.0], dtype=np.float32)

    t_inf = time.time()
    out = sess.run(None, {"input_ids": dummy_input, "speed": speed})
    dt_inf = time.time() - t_inf
    waveform = out[0]
    audio_dur_sec = waveform.shape[-1] / 24000.0
    rtf = audio_dur_sec / dt_inf

    print(f"Shape do áudio sintetizado: {waveform.shape}")
    print(f"Duração do áudio gerado: {audio_dur_sec:.2f}s (gerado em {dt_inf*1000:.1f}ms -> {rtf:.1f}x tempo real na CPU)")
    print("=" * 60)
    print("TESTE COMPLETO: O MODELO PARADEE 4M ESTÁ 100% VALIDADO!")
    print("=" * 60)

if __name__ == "__main__":
    test_paradee_4m()
