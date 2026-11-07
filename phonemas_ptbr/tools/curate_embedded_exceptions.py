import json
import sqlite3
import os
import ctypes

script_dir = os.path.dirname(os.path.abspath(__file__))
lexicon_db = "/home/eraldomr/.gemini/tmp/paradee/lexicon_sp.db"
freq_file = "/home/eraldomr/.gemini/tmp/paradee/frequencias_ptbr_50k.txt"
exceptions_json_path = os.path.join(script_dir, "exceptions_ptbr.json")
loanwords_json_path = os.path.join(script_dir, "loanwords_ptbr.json")
embedded_exceptions_h = os.path.join(script_dir, "../include/embedded_exceptions.h")

# Carregar biblioteca compartilhada para testar o motor de regras
lib = ctypes.CDLL(os.path.join(script_dir, "../build/libphonemas_ptbr.so"))
lib.phonemas_ptbr_translate_rules_only.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_size_t, ctypes.c_int]
lib.phonemas_ptbr_translate_rules_only.restype = ctypes.c_size_t

def rules_only(w, mode=0):
    buf = ctypes.create_string_buffer(1024)
    lib.phonemas_ptbr_translate_rules_only(w.encode('utf-8'), buf, 1024, mode)
    return buf.value.decode('utf-8')

# 1. Carregar loanwords para evitar duplicatas
with open(loanwords_json_path, encoding='utf-8') as f:
    loanwords = set(json.load(f).keys())

# 2. Conjunto de numerais romanos conhecidos
roman_set = {'i', 'ii', 'iii', 'iv', 'v', 'vi', 'vii', 'viii', 'ix', 'x', 'xi', 'xii', 'xiii', 'xiv', 'xv', 'xvi', 'xvii', 'xviii', 'xix', 'xx', 'xxi'}

# 3. Palavras comprovadamente corrompidas, arcaicas ou grafias antigas de espeak
junk_words = {
    'approximadammte', 'approximando', 'approximava', 'deixal', 'fazelo', 'dizelo',
    'proximo', 'proxima', 'extraordinario', 'extraordinaria', 'inexplicavel', 'inumeros',
    'rapida', 'deux', 'extranho', 'extranhas', 'excellente', 'excellencia', 'exhausto',
    'syntaxe', 'abaxo', 'afrôxar', 'agaxi', 'app', 'backup', 'blog', 'bluetooth',
    'bytes', 'byte', 'chat', 'design', 'designer', 'desktop', 'download', 'email',
    'feedback', 'game', 'hardware', 'internet', 'laptop', 'login', 'logout', 'marketing',
    'mouse', 'pixel', 'post', 'show', 'site', 'smartphone', 'software', 'start',
    'upload', 'web', 'games', 'megabytes', 'gigabytes', 'video', 'view', 'c'
}

# 4. Clíticos gramaticais essenciais
clitics_map = {
    'os': {'kokoro': 'ʊs', 'ipa': 'ws'},
    'as': {'kokoro': 'as', 'ipa': 'as'},
    'de': {'kokoro': 'ʤj', 'ipa': 'di'},
    'do': {'kokoro': 'dʊ', 'ipa': 'dw'},
    'dos': {'kokoro': 'dʊs', 'ipa': 'dws'},
    'da': {'kokoro': 'da', 'ipa': 'da'},
    'das': {'kokoro': 'das', 'ipa': 'das'},
    'no': {'kokoro': 'nʊ', 'ipa': 'nw'},
    'nos': {'kokoro': 'nʊs', 'ipa': 'nws'},
    'na': {'kokoro': 'na', 'ipa': 'na'},
    'nas': {'kokoro': 'nas', 'ipa': 'nas'},
    'um': {'kokoro': 'ũŋ', 'ipa': 'ũ'},
    'uns': {'kokoro': 'ũŋs', 'ipa': 'ũs'},
    'uma': {'kokoro': 'ˌumæ', 'ipa': 'umə'},
    'umas': {'kokoro': 'ˌumæs', 'ipa': 'uməs'},
    'se': {'kokoro': 'sy', 'ipa': 'si'},
    'me': {'kokoro': 'my', 'ipa': 'mi'},
    'te': {'kokoro': 'ʧy', 'ipa': 'ti'},
    'lhe': {'kokoro': 'ljy', 'ipa': 'ʎi'},
    'lhes': {'kokoro': 'ljys', 'ipa': 'ʎis'},
    'com': {'kokoro': 'koŋ', 'ipa': 'kõ'},
    'por': {'kokoro': 'poɾ', 'ipa': 'poɾ'},
    'para': {'kokoro': 'pˌaɾæ', 'ipa': 'paɾə'},
    'pra': {'kokoro': 'pɾa', 'ipa': 'pɾa'},
    'pro': {'kokoro': 'pɾʊ', 'ipa': 'pɾw'},
    'não': {'kokoro': 'nˌɐ̃ʊ̃', 'ipa': 'nɐ̃w̃'},
    'muito': {'kokoro': 'mwˈiŋtʊ', 'ipa': 'mwˈĩtw'},
    'também': {'kokoro': 'tɐ̃mbˈAŋ', 'ipa': 'tɐ̃mbẽj'}
}

# 5. Carregar exceptions_ptbr.json atual
with open(exceptions_json_path, encoding='utf-8') as f:
    current_exceptions = json.load(f)

curated = {}

# Adicionar clíticos
for w, val in clitics_map.items():
    curated[w] = val

# Processar as demais exceções
pruned_counts = {'loan': 0, 'roman': 0, 'junk': 0, 'redundant': 0, 'kept': 0}

for w, val in current_exceptions.items():
    if w in clitics_map:
        continue
    if w in loanwords:
        pruned_counts['loan'] += 1
        continue
    if w in roman_set:
        pruned_counts['roman'] += 1
        continue
    if w in junk_words or len(w) <= 1:
        pruned_counts['junk'] += 1
        continue
        
    exp_k = val['kokoro'] if isinstance(val, dict) else val
    exp_ipa = val['ipa'] if isinstance(val, dict) else val
    
    # Normalizar r final para ɾ
    if exp_k.endswith('r') and not exp_k.endswith('ar'):
        exp_k = exp_k[:-1] + 'ɾ'
    if exp_ipa.endswith('r') and not exp_ipa.endswith('ar'):
        exp_ipa = exp_ipa[:-1] + 'ɾ'
        
    pred_k = rules_only(w, 0)
    pred_ipa = rules_only(w, 1)
    
    # Se a regra pura já gera exatamente o mesmo fonema (sem precisar de exceção)
    if pred_k == exp_k and pred_ipa == exp_ipa:
        pruned_counts['redundant'] += 1
        continue
        
    curated[w] = {'kokoro': exp_k, 'ipa': exp_ipa}
    pruned_counts['kept'] += 1

print('=== RESULTADO DA AUDITORIA E CURADORIA DE EXCEÇÕES ===')
print(f"Total de exceções antes: {len(current_exceptions)}")
print(f" - Removidas duplicatas de loanwords: {pruned_counts['loan']}")
print(f" - Removidos numerais romanos: {pruned_counts['roman']}")
print(f" - Removidos termos arcaicos/corrompidos/lixo: {pruned_counts['junk']}")
print(f" - Removidas palavras onde a REGRA JÁ ACERTA 100%: {pruned_counts['redundant']}")
print(f"Total final de exceções curadas e estritamente necessárias: {len(curated)}")

# Salvar exceptions_ptbr.json limpo e ordenado
sorted_curated = sorted(curated.items(), key=lambda x: x[0])
with open(exceptions_json_path, 'w', encoding='utf-8') as f:
    json.dump(dict(sorted_curated), f, ensure_ascii=False, indent=2)

# Gerar include/embedded_exceptions.h enxuto
with open(embedded_exceptions_h, 'w', encoding='utf-8') as f:
    f.write("#ifndef EMBEDDED_EXCEPTIONS_H\n")
    f.write("#define EMBEDDED_EXCEPTIONS_H\n\n")
    f.write("#include <stddef.h>\n\n")
    f.write("/**\n")
    f.write(" * @brief Tabela curada de exceções gramaticais e irregularidades fonéticas essenciais.\n")
    f.write(" * Armazena a transcrição tanto para o vocabulário do Kokoro quanto para o Standard IPA.\n")
    f.write(" */\n")
    f.write("struct ExceptionEntry {\n")
    f.write("    const char* word;\n")
    f.write("    const char* phonemes_kokoro;\n")
    f.write("    const char* phonemes_ipa;\n")
    f.write("};\n\n")
    f.write(f"// Tabela estática em Flash (.rodata) ordenada para busca binária em O(log N) ({len(sorted_curated)} exceções)\n")
    f.write(f"static const struct ExceptionEntry EMBEDDED_EXCEPTIONS[{len(sorted_curated)}] = {{\n")
    for w, d in sorted_curated:
        esc_w = w.replace("\\", "\\\\").replace('"', '\\"')
        esc_k = d['kokoro'].replace("\\", "\\\\").replace('"', '\\"')
        esc_ipa = d['ipa'].replace("\\", "\\\\").replace('"', '\\"')
        f.write(f'    {{"{esc_w}", "{esc_k}", "{esc_ipa}"}},\n')
    f.write("};\n\n")
    f.write(f"static const size_t EMBEDDED_EXCEPTIONS_SIZE = {len(sorted_curated)};\n\n")
    f.write("#endif // EMBEDDED_EXCEPTIONS_H\n")

print(f'Gerado include/embedded_exceptions.h com sucesso ({len(sorted_curated)} entradas).')
