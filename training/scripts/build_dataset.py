import re, json
from datasets import load_dataset

sentences = []
seen = set()

def add_sentence(s):
    s = s.strip()
    s = re.sub(r'\s+', ' ', s)
    if not (20 <= len(s) <= 160):
        return False
    if not (s[0].isupper() or s[0] in '"“'):
        return False
    if s[-1] not in '.!?':
        return False
    bad = set('[]{}<>|_#@/\\*~`^=+$%&')
    if any(c in bad for c in s):
        return False
    if 'http' in s or 'www.' in s or 'Gutenberg' in s or 'Project' in s:
        return False
    key = s.lower()
    if key not in seen:
        seen.add(key)
        sentences.append(s)
        return True
    return False

# 1. TigreGotico (curated phonetic sentences)
print("Carregando TigreGotico...")
try:
    ds_tigre = load_dataset("TigreGotico/portuguese-sentences-synthetic-g2p", split="train")
    for item in ds_tigre:
        add_sentence(item["sentence"])
    print(f"Sentencas apos TigreGotico: {len(sentences)}")
except Exception as e:
    print("Erro TigreGotico:", e)

# 2. Clarice Lispector
print("Carregando Clarice Lispector...")
try:
    with open("/home/astro/clarice.txt", encoding="utf-8", errors="ignore") as f:
        clarice_text = f.read()
    for raw in re.split(r"(?<=[.!?])\s+|\n+", clarice_text):
        add_sentence(raw)
    print(f"Sentencas apos Clarice: {len(sentences)}")
except Exception as e:
    print("Erro Clarice:", e)

# 3. Project Gutenberg (Portuguese)
print("Carregando Project Gutenberg (pt)...")
try:
    ds_gb = load_dataset("manu/project_gutenberg", split="pt", streaming=True)
    for item in ds_gb:
        text = item["text"]
        for raw in re.split(r"(?<=[.!?])\s+|\n+", text):
            if add_sentence(raw):
                if len(sentences) >= 12000:
                    break
        if len(sentences) >= 12000:
            break
except Exception as e:
    print("Erro Gutenberg:", e)

print(f"Total de sentencas coletadas: {len(sentences)}")
with open("data/portugues_sents.json", "w", encoding="utf-8") as f:
    json.dump(sentences, f, ensure_ascii=False, indent=2)
print("Salvo com sucesso em data/portugues_sents.json!")
