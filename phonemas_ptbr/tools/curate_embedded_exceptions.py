import json
import os

script_dir = os.path.dirname(os.path.abspath(__file__))
exceptions_json_path = os.path.join(script_dir, "exceptions_ptbr.json")
embedded_exceptions_h = os.path.join(script_dir, "../include/embedded_exceptions.h")

# Tabela curada estrita de 96 exceções essenciais do Português Brasileiro:
# 1. Clíticos monossilábicos átonos (nunca recebem acento primário ˈ)
# 2. Palavras de altíssima frequência com vogal aberta (/ɛ/ e /ɔ/) real no Brasil
# 3. Hiatos irregulares consolidados (ainda, amendoim)

curated_map = {
    # Clíticos átonos
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
    'também': {'kokoro': 'tɐ̃mbˈAŋ', 'ipa': 'tɐ̃mbẽj'},

    # Palavras de altíssima frequência com vogal aberta (/ɛ/ e /ɔ/) real no Brasil
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
    'belo': {'kokoro': 'bˈɛlʊ', 'ipa': 'bˈɛlw'},
    'bela': {'kokoro': 'bˈɛlæ', 'ipa': 'bˈɛlə'},
    'belos': {'kokoro': 'bˈɛlʊs', 'ipa': 'bˈɛlws'},
    'belas': {'kokoro': 'bˈɛlæs', 'ipa': 'bˈɛləs'},

    # Hiatos irregulares
    'ainda': {'kokoro': 'aˈĩŋdæ', 'ipa': 'aˈĩdə'},
    'amendoim': {'kokoro': 'ˌamAŋdwˈiŋ', 'ipa': 'amẽdwˈĩ'}
}

sorted_curated = sorted(curated_map.items(), key=lambda x: x[0])

# Salvar exceptions_ptbr.json
with open(exceptions_json_path, 'w', encoding='utf-8') as f:
    json.dump(dict(sorted_curated), f, ensure_ascii=False, indent=2)

# Gerar include/embedded_exceptions.h
with open(embedded_exceptions_h, 'w', encoding='utf-8') as f:
    f.write("#ifndef EMBEDDED_EXCEPTIONS_H\n")
    f.write("#define EMBEDDED_EXCEPTIONS_H\n\n")
    f.write("#include <stddef.h>\n\n")
    f.write("/**\n")
    f.write(" * @brief Tabela essencial e estritamente curada de exceções gramaticais pt-BR.\n")
    f.write(" * Contém unicamente os clíticos átonos, hiatos consolidados e aberturas vocálicas\n")
    f.write(" * de altíssima frequência que fogem de regras morfológicas produtivas.\n")
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

print(f"Sucesso: gerado {embedded_exceptions_h} com exatamente {len(sorted_curated)} exceções puras.")
