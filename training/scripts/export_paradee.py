"""Export the Paradee 4M pipeline with the frame-rate head (vhead.py, head v2) as ONE ONNX graph:
input_ids [1,T] int64, speed [1] float -> waveform [1,S] float at 24 kHz.
The head is rewritten without complex numbers (conv1d DFT bases), checked against the torch head on the
same noise, then written as fp32 and as the small copy (int8 weights, fp16 vectors, DFT bases generated at load time).
Resulting int8 ONNX model is ~3.9 MB (< 4 MB).

usage: python scripts/export_paradee.py [DEC_CKPT] [--text CKPT] [--text-preset xs+mlp] [--dec-preset D] [--out models/paradee_ptbr_4m]
"""
import sys, glob, os, time, argparse, warnings, json
import torch, numpy as np, torch.nn as nn, torch.nn.functional as F
warnings.filterwarnings("ignore")
sys.path.insert(0, os.path.dirname(__file__))
from student import TextStudent, alignment
from vhead import VDecoder, SR, FHOP
from torch.nn.utils import remove_weight_norm
from torch.nn.utils.parametrize import is_parametrized, remove_parametrizations

def strip(m):
    for x in m.modules():
        try:
            remove_weight_norm(x)
        except Exception:
            pass
        if is_parametrized(x, "weight"):
            try:
                remove_parametrizations(x, "weight", leave_parametrized=True)
            except Exception:
                pass

class ConvSTFT(nn.Module):
    """torch.stft / torch.istft (hann, centre, reflect) as conv1d / conv_transpose1d with real and imaginary parts kept apart."""
    def __init__(self, n, hop):
        super().__init__()
        self.n, self.hop, self.K = n, hop, n // 2 + 1
        w = torch.hann_window(n, dtype=torch.float64)
        k = torch.arange(self.K, dtype=torch.float64)[:, None]
        ang = 2 * torch.pi * ((k * torch.arange(n, dtype=torch.float64)) % n) / n
        c = torch.full((self.K, 1), 2.0, dtype=torch.float64)
        c[0] = c[-1] = 1
        self.register_buffer("fwd", (torch.cat([torch.cos(ang), -torch.sin(ang)]) * w).float()[:, None])
        self.register_buffer("inv", (torch.cat([torch.cos(ang) * c, -torch.sin(ang) * c]) / n * w).float()[:, None])
        self.register_buffer("w2", (w ** 2).float()[None, None])

    def transform(self, x):   # [B,S] -> re, im [B,K,frames]
        y = F.conv1d(F.pad(x[:, None], (self.n // 2, self.n // 2), mode="reflect"), self.fwd, stride=self.hop)
        return y[:, :self.K], y[:, self.K:]

    def inverse(self, re, im):
        y = F.conv_transpose1d(torch.cat([re, im], 1), self.inv, stride=self.hop)[:, 0]
        env = F.conv_transpose1d(torch.ones_like(re[:1, :1]), self.w2, stride=self.hop)[:, 0]
        return (y / env.clamp_min(1e-8))[:, self.n // 2: -(self.n // 2)]

class HeadX(nn.Module):
    """SourceFilterHead2.forward without complex ops, float64 on-device tricks or repeat_interleave."""
    def __init__(self, h):
        super().__init__()
        self.h = h
        self.st = ConvSTFT(h.n_fft, h.hop)
        self.sb = ConvSTFT(150, 75)
        self.register_buffer("k", torch.arange(1, h.K + 1).float()[None, :, None])
        self.register_buffer("fr", torch.linspace(0, SR / 2, h.nb)[None, :, None])

    def forward(self, x, f0, noise=None):
        h = self.h
        n = x.shape[2]
        T = n * h.up
        S = n * FHOP
        z = h.norm0(h.inp(x).transpose(1, 2)).transpose(1, 2)
        for b in h.blocks:
            z = b(z)
        hn = h.norm1(z.transpose(1, 2))
        o = h.out(hn).view(1, T, h.C * h.G + 1)
        o = torch.cat([o, o[:, -1:]], 1)
        g = (o[..., :-1].reshape(1, T + 1, h.C, h.G) @ h.M.T).permute(0, 2, 3, 1)
        gh, gn = torch.exp(g[:, 0].clamp(max=6)), torch.exp(g[:, 3].clamp(max=6))
        um = torch.sqrt(g[:, 1] ** 2 + g[:, 2] ** 2).clamp_min(1e-4)
        ur, ui = g[:, 1] / um, g[:, 2] / um

        # sine bank: running phase in float64 at frame rate, wrapped, then interpolated between frame centres
        r = f0.double() * FHOP / SR
        c = torch.cumsum(r, 1)
        c = (c - torch.floor(c)).float()
        d = torch.cat([r[:, 1:], torch.zeros_like(r[:, :1])], 1).float()
        t = torch.arange(S, device=x.device)
        p = (t.float() + 0.5) / FHOP - 0.5
        i = torch.clamp(torch.floor(p), min=0).long()
        i = torch.minimum(i, (n - 1) * torch.ones_like(i))
        frac = torch.clamp(p - i.float(), 0, 1)
        cyc = c[:, i] + frac * d[:, i]
        f0s = f0[:, t // FHOP]
        uv = (f0s > 10).float()
        amp = ((f0s[:, None] * self.k < 11000).float() * uv[:, None])
        e = (torch.sin(2 * torch.pi * self.k * cyc[:, None]) * amp).sum(1) * 0.1
        nz = torch.randn_like(e) if noise is None else noise
        Er, Ei = self.st.transform(e)
        Nr, Ni = self.st.transform(nz)
        j = 0
        for m in range(1, h.mod + 1):
            for fn in (torch.cos, torch.sin):
                a = g[:, 4 + j]
                mr, mi = self.st.transform(nz * uv * fn(2 * torch.pi * m * cyc))
                Nr = Nr + a * mr
                Ni = Ni + a * mi
                j += 1
        f0f = torch.cat([f0[:, torch.arange(T, device=x.device) // h.up], f0[:, -1:]], 1).clamp_min(60)
        ang = 2 * torch.pi * (self.fr / f0f[:, None]) * (0.5 * torch.tanh(o[..., -1])[:, None])
        Hr = gh * (ur * torch.cos(ang) - ui * torch.sin(ang))
        Hi = gh * (ur * torch.sin(ang) + ui * torch.cos(ang))
        y = self.st.inverse(Er * Hr - Ei * Hi + Nr * gn, Er * Hi + Ei * Hr + Ni * gn)
        if h.burst:
            gb = torch.exp((h.out_b(hn).view(1, n * 4, h.Gb) @ h.Mb.T).clamp(max=6)).transpose(1, 2)
            gb = torch.cat([gb, gb[..., -1:]], -1)
            br, bi = self.sb.transform(nz.flip(-1))
            y = y + self.sb.inverse(br * gb, bi * gb)
        return y

class Paradee2(nn.Module):
    def __init__(self, ts, vd):
        super().__init__()
        self.ts, self.vd, self.head = ts, vd, HeadX(vd.head)

    def text(self, ids):
        t = self.ts
        p = t.predictor
        s = t.style
        d_en = t.bert_encoder(t.bert(ids, attention_mask=torch.ones_like(ids))).transpose(-1, -2)
        x = d_en
        T = ids.shape[1]
        sx = s[:, :, None].expand(-1, -1, T)
        x = torch.cat([x, sx], 1)
        for b in p.text_encoder.lstms:
            if isinstance(b, nn.LSTM):
                x = b(x.transpose(-1, -2))[0].transpose(-1, -2)
            else:
                x = torch.cat([b(x.transpose(-1, -2), s).transpose(-1, -2), sx], 1)
        d = x.transpose(-1, -2)
        dur = torch.sigmoid(p.duration_proj(p.lstm(d)[0])).sum(-1)[0]
        e = t.text_encoder
        h = e.embedding(ids).transpose(1, 2)
        for c in e.cnn:
            h = c(h)
        return d, dur, t.asr_proj(e.lstm(h.transpose(1, 2))[0]).transpose(-1, -2)

    def trunk(self, asr, F0_curve, N):
        v = self.vd
        s = v.style
        F0 = v.F0_conv(F0_curve.unsqueeze(1))
        Nn = v.N_conv(N.unsqueeze(1))
        x = v.encode(torch.cat([asr, F0, Nn], 1), s)
        asr_res = v.asr_res(asr)
        res = True
        for block in v.decode:
            if res:
                x = torch.cat([x, asr_res, F0, Nn], 1)
            x = block(x, s)
            if block.upsample_type != "none":
                res = False
        return x

    def forward(self, input_ids, speed):
        d, dur, asr_tok = self.text(input_ids)
        n = torch.clamp(torch.round(dur / speed), min=1).long()
        end = torch.cumsum(n, 0)
        start = end - n
        f = torch.arange(end[-1])[None]
        aln = ((f >= start[:, None]) & (f < end[:, None])).float()[None]
        F0, N = self.ts.predictor.F0Ntrain(d.transpose(-1, -2) @ aln, self.ts.style)
        return self.head(self.trunk(asr_tok @ aln, F0, N), F0)

def basis(n, inverse):
    w = torch.hann_window(n, dtype=torch.float64)
    k = torch.arange(n // 2 + 1, dtype=torch.float64)[:, None]
    ang = 2 * torch.pi * ((k * torch.arange(n, dtype=torch.float64)) % n) / n
    c = torch.full((n // 2 + 1, 1), 2.0, dtype=torch.float64)
    c[0] = c[-1] = 1
    return ((torch.cat([torch.cos(ang) * c, -torch.sin(ang) * c]) / n * w) if inverse else (torch.cat([torch.cos(ang), -torch.sin(ang)]) * w)).float()[:, None].numpy()

def basis_nodes(name, n, inverse):
    """[2K,1,n] = cat(cos, -sin)(2 pi k t / n) * per-row scale * hann(t), with k*t reduced mod n in int64 so fp32 cos stays exact"""
    import onnx
    from onnx import numpy_helper, helper, TensorProto
    p = name + "_gen_"
    K = n // 2 + 1
    row = np.ones((2 * K, 1), np.float32)
    if inverse:
        row[:] = 2 / n
        row[[0, K - 1, K, 2 * K - 1]] = 1 / n
    initializers = [
        numpy_helper.from_array(np.arange(K, dtype=np.int64)[:, None], p + "k"),
        numpy_helper.from_array(np.arange(n, dtype=np.int64)[None], p + "n"),
        numpy_helper.from_array(np.array(n, np.int64), p + "N"),
        numpy_helper.from_array(np.array(2 * np.pi / n, np.float32), p + "c"),
        numpy_helper.from_array(torch.hann_window(n).numpy()[None], p + "w"),
        numpy_helper.from_array(row, p + "row"),
        numpy_helper.from_array(np.array([1], np.int64), p + "ax")
    ]
    nodes = [
        helper.make_node("Mul", [p + "k", p + "n"], [p + "kn"]),
        helper.make_node("Mod", [p + "kn", p + "N"], [p + "m"]),
        helper.make_node("Cast", [p + "m"], [p + "mf"], to=TensorProto.FLOAT),
        helper.make_node("Mul", [p + "mf", p + "c"], [p + "a"]),
        helper.make_node("Cos", [p + "a"], [p + "cos"]),
        helper.make_node("Sin", [p + "a"], [p + "sin"]),
        helper.make_node("Neg", [p + "sin"], [p + "nsin"]),
        helper.make_node("Concat", [p + "cos", p + "nsin"], [p + "cs"], axis=0),
        helper.make_node("Mul", [p + "cs", p + "w"], [p + "b0"]),
        helper.make_node("Mul", [p + "b0", p + "row"], [p + "b"]),
        helper.make_node("Unsqueeze", [p + "b", p + "ax"], [name])
    ]
    return initializers, nodes

def export_model(dec_path, text_path, text_preset="xs+mlp", dec_preset="D", head="v2", out_dir="models/paradee_ptbr_4m"):
    os.makedirs(out_dir, exist_ok=True)
    ts = TextStudent(text_preset).eval()
    if os.path.exists(text_path):
        ts.load_state_dict(torch.load(text_path, map_location="cpu")["model"])
        print(f"Carregou TextStudent de {text_path}")
    else:
        print(f"Aviso: Checkpoint {text_path} não encontrado, exportando com inicialização padrão")
    
    vd = VDecoder(preset=dec_preset, head=head).eval()
    if os.path.exists(dec_path):
        vd.load_ckpt(dec_path)
        print(f"Carregou VDecoder de {dec_path}")
    else:
        print(f"Aviso: Checkpoint {dec_path} não encontrado, exportando com inicialização padrão")

    strip(ts)
    strip(vd)
    m = Paradee2(ts, vd).eval()

    path = f"{out_dir}/paradee.onnx"
    ids = torch.randint(1, 100, (1, 20))
    torch.onnx.export(m, (ids, torch.ones(1)), path, input_names=["input_ids", "speed"], output_names=["waveform"],
                      dynamic_axes={"input_ids": {1: "tokens"}, "waveform": {1: "samples"}}, opset_version=17, dynamo=False)
    print(f"Exportado FP32: {path} ({os.path.getsize(path)/1e6:.2f} MB)")

    # Copiar config.json de vocabulário se existir
    cfg_path = f"{out_dir}/config.json"
    if not os.path.exists(cfg_path):
        for candidate in ["data/config.json", "models/config.json", "../models/paradee_ptbr_alex/config.json"]:
            if os.path.exists(candidate):
                import shutil
                shutil.copy(candidate, cfg_path)
                print(f"Vocabulário copiado de {candidate} para {cfg_path}")
                break

    # Quantização int8 + fp16
    import onnx
    from onnx import numpy_helper, helper, TensorProto
    g = onnx.load(path)
    nq = nh = nb = 0
    new_nodes = []

    for init in list(g.graph.initializer):
        w = numpy_helper.to_array(init)
        name = init.name
        if w.dtype != np.float32:
            continue
        hit = [(n, inv) for n in (600, 150) for inv in (False, True) if w.shape == (n + 2, 1, n) and np.abs(w - basis(n, inv)).max() < 1e-4 * np.abs(w).max()]
        if hit:
            g.graph.initializer.remove(init)
            inits, nodes = basis_nodes(name, *hit[0])
            g.graph.initializer.extend(inits)
            new_nodes += nodes
            nb += 1
        elif w.ndim >= 2 and w.size >= 1024:
            sc = np.abs(w).reshape(w.shape[0], -1).max(1) / 127
            sc[sc == 0] = 1
            q = np.clip(np.round(w / sc.reshape((-1,) + (1,) * (w.ndim - 1))), -127, 127).astype(np.int8)
            g.graph.initializer.remove(init)
            g.graph.initializer.extend([
                numpy_helper.from_array(q, name + "_q"),
                numpy_helper.from_array(sc.astype(np.float16), name + "_s16"),
                numpy_helper.from_array(np.zeros(len(sc), np.int8), name + "_z")
            ])
            new_nodes += [
                helper.make_node("Cast", [name + "_s16"], [name + "_s"], to=TensorProto.FLOAT),
                helper.make_node("DequantizeLinear", [name + "_q", name + "_s", name + "_z"], [name], axis=0)
            ]
            nq += 1
        elif w.size >= 64 and np.abs(w).max() < 6e4:
            g.graph.initializer.remove(init)
            g.graph.initializer.append(numpy_helper.from_array(w.astype(np.float16), name + "_16"))
            new_nodes.append(helper.make_node("Cast", [name + "_16"], [name], to=TensorProto.FLOAT))
            nh += 1

    for n_ in reversed(new_nodes):
        g.graph.node.insert(0, n_)

    qpath = f"{out_dir}/paradee_int8.onnx"
    onnx.checker.check_model(g)
    onnx.save(g, qpath)
    final_size = os.path.getsize(qpath) / 1024 / 1024
    print(f"Exportado INT8: {qpath} ({final_size:.2f} MB)")
    print(f"Pesos int8: {nq} tensores, fp16: {nh}, bases DFT: {nb}")
    return qpath

def main():
    ap = argparse.ArgumentParser(description="Export Paradee 4M ONNX")
    ap.add_argument("dec", nargs="?", default="checkpoints/vh_4m/last.pt", help="Checkpoint VDecoder")
    ap.add_argument("--text", default="checkpoints/text_xs+mlp/last.pt", help="Checkpoint TextStudent")
    ap.add_argument("--text-preset", default="xs+mlp", help="Preset TextStudent (xs+mlp, 4m+mlp)")
    ap.add_argument("--dec-preset", default="D", help="Preset VDecoder (D, 4M, nano)")
    ap.add_argument("--head", default="v2", help="VDecoder head (v2)")
    ap.add_argument("--out", default="models/paradee_ptbr_4m", help="Diretório de saída")
    A = ap.parse_args()
    export_model(A.dec, A.text, A.text_preset, A.dec_preset, A.head, A.out)

if __name__ == "__main__":
    main()
