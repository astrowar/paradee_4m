"""Distillation corpus: run the Kokoro teacher over sentences and store every
intermediate the half-wise experiments need. Resumable: writes shards of SHARD sentences,
skips shards that already exist.
usage: gen_teacher.py [n_sentences] [--device DEV] [--lang LANG] [--voice VOICE] [--sents FILE] [--start-shard N] [--end-shard N]"""
import sys, os, json, time, warnings, argparse, torch
warnings.filterwarnings("ignore")
from kokoro import KPipeline

ap = argparse.ArgumentParser()
ap.add_argument("n_sentences", nargs="?", type=int, default=12000)
ap.add_argument("--device", default="cuda" if torch.cuda.is_available() else ("mps" if torch.backends.mps.is_available() else "cpu"))
ap.add_argument("--lang", default="p")
ap.add_argument("--voice", default="ef_dora.pt")
ap.add_argument("--sents", default="data/portugues_sents.json")
ap.add_argument("--shard", type=int, default=500)
ap.add_argument("--start-shard", type=int, default=0)
ap.add_argument("--end-shard", type=int, default=None)
A = ap.parse_args()

N = A.n_sentences
SHARD = A.shard
DEV = A.device

sents = json.load(open(A.sents))[:N]
start_sh = A.start_shard * SHARD
end_sh = len(sents) if A.end_shard is None else min(len(sents), A.end_shard * SHARD)
print(f"[{DEV}] Iniciando gen_teacher: sentencas {start_sh} a {end_sh} (shards {A.start_shard}..{(end_sh-1)//SHARD}) no {DEV} (idioma '{A.lang}', voz '{A.voice}')", flush=True)

pipe = KPipeline(lang_code=A.lang, device=DEV, repo_id="hexgrad/Kokoro-82M")
m = pipe.model.eval()

if os.path.exists(A.voice):
    ref = torch.load(A.voice, map_location=DEV)
else:
    ref = pipe.load_voice(A.voice)

os.makedirs("data/teacher", exist_ok=True)

@torch.no_grad()
def run(ps):
    ids = torch.LongTensor([[0, *[m.vocab[c] for c in ps if c in m.vocab], 0]]).to(m.device)
    T = ids.shape[1]
    idx_ref = min(max(0, T - 3), ref.shape[0] - 1)
    r = ref[idx_ref].to(m.device)  # style indexed by phoneme count (== T-2, minus 1)
    L = torch.full((1,), T, device=m.device)
    mask = torch.zeros(1, T, dtype=torch.bool, device=m.device)
    bert_dur = m.bert(ids, attention_mask=(~mask).int())
    d_en = m.bert_encoder(bert_dur).transpose(-1, -2)
    s = r[:, 128:]
    d = m.predictor.text_encoder(d_en, s, L, mask)
    x, _ = m.predictor.lstm(d)
    dur = torch.sigmoid(m.predictor.duration_proj(x)).sum(-1)
    pred_dur = torch.round(dur).clamp(min=1).long().squeeze(0)
    idx = torch.repeat_interleave(torch.arange(T, device=m.device), pred_dur)
    aln = torch.zeros(T, idx.shape[0], device=m.device)
    aln[idx, torch.arange(idx.shape[0])] = 1
    aln = aln[None]
    en = d.transpose(-1, -2) @ aln
    F0, Nn = m.predictor.F0Ntrain(en, s)
    t_en = m.text_encoder(ids, L, mask)
    asr = t_en @ aln
    audio = m.decoder(asr, F0, Nn, r[:, :128]).squeeze()
    c = lambda t: t.squeeze(0).cpu()
    return dict(ids=c(ids), dur=c(dur), pred_dur=pred_dur.cpu(), d=c(d).half(), t_en=c(t_en).half(),
                F0=c(F0), N=c(Nn), audio=(audio.cpu().clamp(-1, 1) * 32767).short())

t0 = time.time(); secs = 0.0
for sh in range(start_sh, end_sh, SHARD):
    path = f"data/teacher/shard_{sh // SHARD:04d}.pt"
    if os.path.exists(path):
        print(f"[{DEV}] {path} ja existe, pulando...", flush=True)
        continue
    rows = []
    sh_t0 = time.time()
    for text in sents[sh:sh + SHARD]:
        ps, _ = pipe.g2p(text)
        if not ps or len(ps) > 400:
            continue
        r = run(ps)
        r["text"] = text
        r["ps"] = ps
        rows.append(r)
        secs += len(r["audio"]) / 24000
    tmp_path = path + f".tmp_{DEV.replace(':', '_')}"
    torch.save(rows, tmp_path)
    os.replace(tmp_path, path)
    elapsed = time.time() - t0
    sh_elapsed = time.time() - sh_t0
    print(f"[{DEV}] {path} {len(rows)} sents ({sh_elapsed:.1f}s) | total audio: {secs/3600:.2f}h | total elapsed: {elapsed:.0f}s ({secs/max(elapsed, 1e-5):.1f}x RT)", flush=True)

print(f"[{DEV}] Concluido range {start_sh}..{end_sh}!")
