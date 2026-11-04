"""Recompute every shard's waveform with the CPU teacher decoder from the stored intermediates,
with a fixed per-sentence seed, so the corpus is deterministic and device-canonical (CPU numerics).
Shards already carrying 'audio_cpu' are skipped; the shard is rewritten atomically.
Supports multi-processing across shards."""
import sys, glob, os, time, argparse, torch
from multiprocessing import Pool
sys.path.insert(0, "scripts")
from common import load_teacher
from student import alignment

def process_shard(args):
    path, voice, lang = args
    try:
        rows = torch.load(path)
    except Exception as e:
        print(f"Error loading {path}: {e}")
        return path, 0, 0
    if rows and rows[0].get("audio_cpu"):
        print(f"{path} already has audio_cpu, skipping.")
        return path, 0, 0

    t0 = time.time()
    pipe, m, _ = load_teacher(device="cpu", voice=voice, lang_code=lang, fuse=False)
    if os.path.exists(voice):
        ref = torch.load(voice, map_location="cpu")
    else:
        ref = pipe.load_voice(voice)

    secs = 0
    with torch.no_grad():
        for i, r in enumerate(rows):
            torch.manual_seed(i)
            aln = alignment(r["pred_dur"])
            Fr = aln.shape[1]
            asr = r["t_en"].float()[None] @ aln
            idx_ref = min(max(0, len(r["ids"]) - 3), ref.shape[0] - 1)
            s = ref[idx_ref]
            au = m.decoder(asr, r["F0"][None, :2*Fr], r["N"][None, :2*Fr], s[:, :128]).squeeze()
            r["audio_mps"] = r["audio"]
            r["audio"] = (au.clamp(-1, 1) * 32767).short()
            r["audio_cpu"] = True
            secs += len(au) / 24000

    tmp = path + f".tmp_{os.getpid()}"
    torch.save(rows, tmp)
    os.replace(tmp, path)
    dur = time.time() - t0
    print(f"Done {path}: {len(rows)} sents, {secs/3600:.2f}h audio ({dur:.1f}s, {secs/max(dur, 1e-5):.1f}x RT)", flush=True)
    return path, secs, dur

if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--lang", default="p")
    ap.add_argument("--voice", default="ef_dora.pt")
    ap.add_argument("--workers", type=int, default=8)
    A = ap.parse_args()

    shards = sorted(glob.glob("data/teacher/shard_*.pt"))
    print(f"Starting regen_audio on {len(shards)} shards with {A.workers} workers...", flush=True)
    tasks = [(sh, A.voice, A.lang) for sh in shards]

    t0 = time.time()
    with Pool(processes=A.workers) as pool:
        results = pool.map(process_shard, tasks)

    tot_secs = sum(r[1] for r in results)
    print(f"Finished regen_audio on all shards! Total audio: {tot_secs/3600:.2f}h in {time.time()-t0:.1f}s", flush=True)
