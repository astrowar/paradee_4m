import sqlite3
import json
import os

script_dir = os.path.dirname(os.path.abspath(__file__))
lexicon_db = os.path.join(script_dir, "../../.gemini/tmp/paradee/lexicon_sp.db")
if not os.path.exists(lexicon_db):
    lexicon_db = "/home/eraldomr/.gemini/tmp/paradee/lexicon_sp.db"

freq_file = "/home/eraldomr/.gemini/tmp/paradee/frequencias_ptbr_50k.txt"
exceptions_json_path = os.path.join(script_dir, "exceptions_ptbr.json")
embedded_exceptions_h = os.path.join(script_dir, "../include/embedded_exceptions.h")

# 1. Carregar Léxico SPX
conn = sqlite3.connect(lexicon_db)
c = conn.cursor()
c.execute('SELECT word, phones, postag FROM sp_lexicon WHERE region_code = "spx"')
spx_dict = {w: (ph.replace('|', ''), pos) for w, ph, pos in c.fetchall()}

# 2. Carregar FrequencyWords 50k
freq_words = []
with open(freq_file, encoding='utf-8') as f:
    for rank, line in enumerate(f, 1):
        parts = line.strip().split()
        if len(parts) == 2:
            freq_words.append((rank, parts[0], int(parts[1])))

# 3. Função de conversão SPX para Kokoro
def spx_to_kokoro(ipa_str):
    k = ipa_str
    k = k.replace('tʃ', 'ʧ').replace('dʒ', 'ʤ')
    k = k.replace('ej', 'A').replace('aj', 'I').replace('aw', 'W').replace('oj', 'Y')
    k = k.replace('w', 'ʊ')
    if k.endswith('i'):
        k = k[:-1] + 'y'
    elif k.endswith('is') and not k.endswith('ˈis'):
        k = k[:-2] + 'ys'
    if k.endswith('ə'):
        k = k[:-1] + 'æ'
    elif k.endswith('əs'):
        k = k[:-2] + 'æs'
    elif k.endswith('a') and not k.endswith('ˈa'):
        k = k[:-1] + 'æ'
    elif k.endswith('as') and not k.endswith('ˈas'):
        k = k[:-2] + 'æs'
    k = k.replace('ə̃', 'ɐ̃ŋ').replace('ẽj', 'ẽj').replace('ẽ', 'Aŋ').replace('õ', 'oŋ').replace('ũ', 'ũŋ')
    return k

# 4. Palavras-chave manuais de altíssima frequência
manual_high_freq = {
    'era': {'kokoro': 'ˈɛɾæ', 'ipa': 'ˈɛɾə'},
    'essa': {'kokoro': 'ˈɛsæ', 'ipa': 'ˈɛsə'},
    'esse': {'kokoro': 'ˈɛsy', 'ipa': 'ˈɛsi'},
    'esses': {'kokoro': 'ˈɛsys', 'ipa': 'ˈɛsis'},
    'essas': {'kokoro': 'ˈɛsæs', 'ipa': 'ˈɛsəs'},
    'esta': {'kokoro': 'ˈɛstæ', 'ipa': 'ˈɛstə'},
    'estas': {'kokoro': 'ˈɛstæs', 'ipa': 'ˈɛstəs'},
    'este': {'kokoro': 'ˈɛsʧy', 'ipa': 'ˈɛstʃi'},
    'estes': {'kokoro': 'ˈɛsʧys', 'ipa': 'ˈɛstʃis'},
    'ela': {'kokoro': 'ˈɛlæ', 'ipa': 'ˈɛlə'},
    'elas': {'kokoro': 'ˈɛlæs', 'ipa': 'ˈɛləs'},
    'ele': {'kokoro': 'ˈely', 'ipa': 'ˈeli'},
    'eles': {'kokoro': 'ˈelys', 'ipa': 'ˈelis'},
    'melhor': {'kokoro': 'meʎˈɔɾ', 'ipa': 'meʎˈɔɾ'},
    'melhores': {'kokoro': 'meʎˈɔɾys', 'ipa': 'meʎˈɔɾis'},
    'pelo': {'kokoro': 'pˈɛlʊ', 'ipa': 'pˈɛlw'},
    'pela': {'kokoro': 'pˈɛlæ', 'ipa': 'pˈɛlə'},
    'pelos': {'kokoro': 'pˈɛlʊs', 'ipa': 'pˈɛlws'},
    'pelas': {'kokoro': 'pˈɛlæs', 'ipa': 'pˈɛləs'},
    'deve': {'kokoro': 'dˈɛvy', 'ipa': 'dˈɛvi'},
    'devem': {'kokoro': 'dˈɛvẽj', 'ipa': 'dˈɛvẽj'},
    'fora': {'kokoro': 'fˈɔɾæ', 'ipa': 'fˈɔɾə'},
    'qualquer': {'kokoro': 'kwaʊkˈɛɾ', 'ipa': 'kwawkˈɛɾ'},
    'nosso': {'kokoro': 'nˈɔsʊ', 'ipa': 'nˈɔsw'},
    'nossa': {'kokoro': 'nˈɔsæ', 'ipa': 'nˈɔsə'},
    'nossos': {'kokoro': 'nˈɔsʊs', 'ipa': 'nˈɔsws'},
    'nossas': {'kokoro': 'nˈɔsæs', 'ipa': 'nˈɔsəs'},
    'volta': {'kokoro': 'vˈɔʊtæ', 'ipa': 'vˈɔwtə'},
    'voltam': {'kokoro': 'vˈɔʊtɐ̃ʊ̃', 'ipa': 'vˈɔwtɐ̃w̃'},
    'embora': {'kokoro': 'Aŋbˈɔɾæ', 'ipa': 'ĩbˈɔɾə'},
    'perto': {'kokoro': 'pˈɛɾtʊ', 'ipa': 'pˈɛɾtw'},
    'aquela': {'kokoro': 'akˈɛlæ', 'ipa': 'akˈɛlə'},
    'aquelas': {'kokoro': 'akˈɛlæs', 'ipa': 'akˈɛləs'},
    'aquele': {'kokoro': 'akˈɛly', 'ipa': 'akˈɛli'},
    'aqueles': {'kokoro': 'akˈɛlys', 'ipa': 'akˈɛlis'},
    'chefe': {'kokoro': 'ʃˈɛfy', 'ipa': 'ʃˈɛfi'},
    'chefes': {'kokoro': 'ʃˈɛfys', 'ipa': 'ʃˈɛfis'},
    'espera': {'kokoro': 'espˈɛɾæ', 'ipa': 'ispˈɛɾə'},
    'droga': {'kokoro': 'drˈɔɡæ', 'ipa': 'dɾˈɔgə'},
    'drogas': {'kokoro': 'drˈɔɡæs', 'ipa': 'dɾˈɔgəs'},
    'maior': {'kokoro': 'mIˈɔɾ', 'ipa': 'majˈɔɾ'},
    'maiores': {'kokoro': 'mIˈɔɾys', 'ipa': 'majˈɔɾis'},
    'velho': {'kokoro': 'vˈɛʎʊ', 'ipa': 'vˈɛʎw'},
    'velha': {'kokoro': 'vˈɛʎæ', 'ipa': 'vˈɛʎə'},
    'velhos': {'kokoro': 'vˈɛʎʊs', 'ipa': 'vˈɛʎws'},
    'velhas': {'kokoro': 'vˈɛʎæs', 'ipa': 'vˈɛʎəs'},
    'guerra': {'kokoro': 'ɡˈɛxæ', 'ipa': 'gˈɛxə'},
    'guerras': {'kokoro': 'ɡˈɛxæs', 'ipa': 'gˈɛxəs'},
    'telefone': {'kokoro': 'ˌteli-fˈɔny', 'ipa': 'telefˈɔni'},
    'telefones': {'kokoro': 'ˌteli-fˈɔnys', 'ipa': 'telefˈɔnis'},
    'jovem': {'kokoro': 'ʒˈɔvẽj', 'ipa': 'ʒˈɔvẽj'},
    'jovens': {'kokoro': 'ʒˈɔvẽjs', 'ipa': 'ʒˈɔvẽjs'},
    'pior': {'kokoro': 'piˈɔɾ', 'ipa': 'piˈɔɾ'},
    'piores': {'kokoro': 'piˈɔɾys', 'ipa': 'piˈɔɾis'},
    'resto': {'kokoro': 'xˈɛstʊ', 'ipa': 'xˈɛstw'},
    'foto': {'kokoro': 'fˈɔtʊ', 'ipa': 'fˈɔtw'},
    'fotos': {'kokoro': 'fˈɔtʊs', 'ipa': 'fˈɔtws'},
    'corpo': {'kokoro': 'kˈɔɾpʊ', 'ipa': 'kˈɔɾpw'},
    'corpos': {'kokoro': 'kˈɔɾpʊs', 'ipa': 'kˈɔɾpws'},
    'perde': {'kokoro': 'pˈɛɾʤy', 'ipa': 'pˈɛɾdʒi'},
    'perdem': {'kokoro': 'pˈɛɾdẽj', 'ipa': 'pˈɛɾdẽj'},
    'leva': {'kokoro': 'lˈɛvæ', 'ipa': 'lˈɛvə'},
    'levam': {'kokoro': 'lˈɛvɐ̃ʊ̃', 'ipa': 'lˈɛvɐ̃w̃'},
    'pega': {'kokoro': 'pˈɛɡæ', 'ipa': 'pˈɛgə'},
    'pegam': {'kokoro': 'pˈɛɡɐ̃ʊ̃', 'ipa': 'pˈɛgɐ̃w̃'},
    'aberto': {'kokoro': 'ˌabˈɛɾtʊ', 'ipa': 'abˈɛɾtw'},
    'aberta': {'kokoro': 'ˌabˈɛɾtæ', 'ipa': 'abˈɛɾtə'},
    'abertos': {'kokoro': 'ˌabˈɛɾtʊs', 'ipa': 'abˈɛɾtws'},
    'abertas': {'kokoro': 'ˌabˈɛɾtæs', 'ipa': 'abˈɛɾtəs'},
    'belo': {'kokoro': 'bˈɛlʊ', 'ipa': 'bˈɛlw'},
    'bela': {'kokoro': 'bˈɛlæ', 'ipa': 'bˈɛlə'},
    'belos': {'kokoro': 'bˈɛlʊs', 'ipa': 'bˈɛlws'},
    'belas': {'kokoro': 'bˈɛlæs', 'ipa': 'bˈɛləs'},
    'ainda': {'kokoro': 'aˈĩŋdæ', 'ipa': 'aˈĩdə'},
    'amendoim': {'kokoro': 'ˌamAŋdwˈiŋ', 'ipa': 'amẽdwˈĩ'},
    'ruim': {'kokoro': 'xuˈiŋ', 'ipa': 'xuˈĩ'},
    'raiz': {'kokoro': 'xaˈis', 'ipa': 'xaˈis'},
    'juiz': {'kokoro': 'ʒuˈis', 'ipa': 'ʒuˈis'},
    'batom': {'kokoro': 'batˈoŋ', 'ipa': 'batˈõ'},
    'marrom': {'kokoro': 'maxˈoŋ', 'ipa': 'maxˈõ'},
    'garçom': {'kokoro': 'ɡaɾsˈoŋ', 'ipa': 'ɡaɾsˈõ'},
    'cupom': {'kokoro': 'kupˈoŋ', 'ipa': 'kupˈõ'},
    'bombom': {'kokoro': 'boŋbˈoŋ', 'ipa': 'bõbˈõ'}
}

# 5. Carregar exceções existentes
current_exceptions = json.load(open(exceptions_json_path, encoding='utf-8'))
unified_exceptions = {}

for w, val in current_exceptions.items():
    if isinstance(val, dict):
        unified_exceptions[w] = val
    else:
        # String única legada -> replica para kokoro e ipa básico
        unified_exceptions[w] = {'kokoro': val, 'ipa': val}

for w, d in manual_high_freq.items():
    unified_exceptions[w] = d

# Adicionar top 350 palavras de maior frequência com /ɛ/ e /ɔ/
added_cnt = 0
for rank, w, cnt in freq_words:
    if w not in unified_exceptions and w in spx_dict and not any(acc in w for acc in 'éóèò'):
        ph, pos = spx_dict[w]
        if ('ɛ' in ph or 'ɔ' in ph) and len(w) >= 3:
            k_ph = spx_to_kokoro(ph)
            unified_exceptions[w] = {'kokoro': k_ph, 'ipa': ph}
            added_cnt += 1
            if added_cnt >= 350:
                break

print(f"Total de exceções gramaticais unificadas: {len(unified_exceptions)}")

# Salvar exceptions_ptbr.json
with open(exceptions_json_path, 'w', encoding='utf-8') as f:
    json.dump(unified_exceptions, f, ensure_ascii=False, indent=2)

# Gerar include/embedded_exceptions.h
sorted_exc = sorted(unified_exceptions.items(), key=lambda x: x[0])
with open(embedded_exceptions_h, 'w', encoding='utf-8') as f:
    f.write("#ifndef EMBEDDED_EXCEPTIONS_H\n")
    f.write("#define EMBEDDED_EXCEPTIONS_H\n\n")
    f.write("#include <stddef.h>\n\n")
    f.write("/**\n")
    f.write(" * @brief Entrada da tabela de exceções gramaticais e irregularidades fonéticas.\n")
    f.write(" * Armazena a transcrição tanto para o vocabulário do Kokoro quanto para o Standard IPA.\n")
    f.write(" */\n")
    f.write("struct ExceptionEntry {\n")
    f.write("    const char* word;\n")
    f.write("    const char* phonemes_kokoro;\n")
    f.write("    const char* phonemes_ipa;\n")
    f.write("};\n\n")
    f.write(f"// Tabela estática em Flash (.rodata) para busca binária em O(log N) ({len(sorted_exc)} entradas)\n")
    f.write(f"static const struct ExceptionEntry EMBEDDED_EXCEPTIONS[{len(sorted_exc)}] = {{\n")
    for w, d in sorted_exc:
        esc_w = w.replace("\\", "\\\\").replace('"', '\\"')
        esc_k = d['kokoro'].replace("\\", "\\\\").replace('"', '\\"')
        esc_ipa = d['ipa'].replace("\\", "\\\\").replace('"', '\\"')
        f.write(f'    {{"{esc_w}", "{esc_k}", "{esc_ipa}"}},\n')
    f.write("};\n\n")
    f.write(f"static const size_t EMBEDDED_EXCEPTIONS_SIZE = {len(sorted_exc)};\n\n")
    f.write("#endif // EMBEDDED_EXCEPTIONS_H\n")

print(f"Gerado com sucesso: {embedded_exceptions_h} ({len(sorted_exc)} exceções)")
