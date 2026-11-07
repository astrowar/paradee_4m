import sqlite3
import ctypes
import re
import json
from collections import defaultdict, Counter

lib = ctypes.CDLL('/home/eraldomr/dev/paradee/phonemas_ptbr/build/libphonemas_ptbr.so')
lib.phonemas_ptbr_translate_buffer.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_size_t]
lib.phonemas_ptbr_translate_buffer.restype = ctypes.c_size_t

def ptbr(w: str) -> str:
    buf = ctypes.create_string_buffer(1024)
    lib.phonemas_ptbr_translate_buffer(w.encode('utf-8'), buf, 1024)
    return buf.value.decode('utf-8')

def extract_primary_stress_vowel(phones: str):
    idx = phones.find('ˈ')
    if idx == -1: return None
    for ch in phones[idx+1:idx+6]:
        if ch in 'aeiouáéíóúâêôãõɛɔɐəɨũĩẽõə̃AE':
            return ch
    return None

def main():
    conn = sqlite3.connect('/home/eraldomr/.gemini/tmp/paradee/lexicon_sp.db')
    c = conn.cursor()
    c.execute('SELECT DISTINCT word, postag, phones, syllables FROM sp_lexicon WHERE region_code = "spx"')
    rows = c.fetchall()
    total_words = len(rows)
    print(f"Total de palavras carregadas do SPX: {total_words}")

    # Contadores de problemas
    stress_wrong = []
    stress_missing = []
    x_errors = []
    open_vowels_closed = [] # /ɛ/ ou /ɔ/ onde virou e/o
    verb_infinitive_r_diff = [] # verbo terminado em r onde spx tem apocope
    coda_l_diff = [] # w vs ʊ
    atona_e_diff = [] # y vs i
    atona_a_diff = [] # æ vs ə/ɐ
    epenthesis_diff = [] # i vs ə/ɨ
    diphthong_sibilant_diff = [] # mas -> majs vs mas
    prefix_des_diff = []

    for w, postag, ref_phones, sylls in rows:
        pred = ptbr(w)
        ref_raw = ref_phones.replace('|', '')

        # 1. Análise de Acento Tônico
        if 'ˈ' in ref_raw:
            if 'ˈ' not in pred:
                stress_missing.append((w, postag, ref_raw, pred))
            else:
                rv = extract_primary_stress_vowel(ref_raw)
                pv = extract_primary_stress_vowel(pred)
                v_map = {'ɐ': 'a', 'ə': 'a', 'æ': 'a', 'á': 'a', 'ã': 'a', 'a': 'a',
                         'ɛ': 'e', 'é': 'e', 'ê': 'e', 'ẽ': 'e', 'e': 'e', 'A': 'e', 'E': 'e',
                         'í': 'i', 'ĩ': 'i', 'i': 'i',
                         'ɔ': 'o', 'ó': 'o', 'ô': 'o', 'õ': 'o', 'ʊ': 'o', 'o': 'o',
                         'ú': 'u', 'ũ': 'u', 'u': 'u'}
                rv_norm = v_map.get(rv, rv)
                pv_norm = v_map.get(pv, pv)
                if rv_norm != pv_norm:
                    stress_wrong.append((w, postag, ref_raw, pred, rv, pv))

        # 2. Análise da Letra X
        if 'x' in w:
            if 'x' in pred and ('ʃ' in ref_raw or 'ks' in ref_raw or 'z' in ref_raw or 's' in ref_raw):
                x_errors.append((w, ref_raw, pred))

        # 3. Análise de Vogais Médias Abertas (/ɛ/, /ɔ/)
        if ('ɛ' in ref_raw or 'ɔ' in ref_raw) and not any(acc in w for acc in 'éóèò'):
            if 'ɛ' not in pred and 'ɔ' not in pred:
                open_vowels_closed.append((w, ref_raw, pred))

        # 4. Apócope de R em verbos no infinitivo
        if postag == 'VERB' and (w.endswith('ar') or w.endswith('er') or w.endswith('ir')):
            if not ref_raw.endswith('ɾ') and pred.endswith('ɾ'):
                verb_infinitive_r_diff.append((w, ref_raw, pred))

        # 5. Coda L: w vs ʊ
        if 'w' in ref_raw and 'ʊ' in pred:
            coda_l_diff.append((w, ref_raw, pred))

        # 6. Vogal átona final e: y vs i/dʒi/tʃi
        if pred.endswith('y') or pred.endswith('ys'):
            atona_e_diff.append((w, ref_raw, pred))

        # 7. Vogal átona final a: æ vs ə
        if 'æ' in pred and 'ə' in ref_raw:
            atona_a_diff.append((w, ref_raw, pred))

        # 8. Epêntese: [i] (phonemas_ptbr) vs [ə]/[ɨ] (SPX)
        if ('i' in pred or 'ʧi' in pred or 'ʤi' in pred) and ('ə' in ref_raw or 'ɨ' in ref_raw):
            if re.search(r'(ct|pt|dv|gn|tm|ps|pn|ft|bj|dj|bs)', w):
                epenthesis_diff.append((w, ref_raw, pred))

        # 9. Ditongação antes de sibilante em coda (ex: arroz -> a|xˈɔjs vs axˈos)
        if 'js' in ref_raw and 'js' not in pred and not any(d in w for d in ['is', 'eis', 'ais', 'ois', 'uis']):
            diphthong_sibilant_diff.append((w, ref_raw, pred))

        # 10. Prefixo des- (em SPX transcrito como dʒis ou des)
        if w.startswith('des') and ('dʒis' in ref_raw or 'dʒiz' in ref_raw) and ('des' in pred or 'dez' in pred):
            prefix_des_diff.append((w, ref_raw, pred))

    print("\n" + "="*60)
    print("         RELATÓRIO DE DIAGNÓSTICO PROFUNDO DO PHONEMAS_PTBR        ")
    print("="*60)
    print(f"Total de Palavras Avaliadas: {total_words}")
    print("-" * 60)
    print("1. FALHAS LINGUÍSTICAS REAIS (ERROS DO MOTOR):")
    print(f" - Vogais Abertas Não-Acentuadas (/ɛ/, /ɔ/) omitidas : {len(open_vowels_closed)} palavras")
    print(f" - Erros de Posição de Acento Tônico                 : {len(stress_wrong)} palavras")
    print(f" - Palavras que Ficaram Sem Acento Tônico            : {len(stress_missing)} palavras")
    print(f" - Erros Residuais na Letra X                        : {len(x_errors)} palavras")
    print("-" * 60)
    print("2. DIVERGÊNCIAS DE REGISTRO / DIALETO (PADRÃO vs COLOQUIAL / SOTAQUE):")
    print(f" - Apócope do R em Verbos no Infinitivo (falá vs falar): {len(verb_infinitive_r_diff)} verbos")
    print(f" - Ditongação Paulista diante de S em Coda (arroz/paz): {len(diphthong_sibilant_diff)} palavras")
    print(f" - Redução do Prefixo des- para dʒis- (desfazer)      : {len(prefix_des_diff)} palavras")
    print("-" * 60)
    print("3. DIVERGÊNCIAS DE CONVENÇÃO DE SÍMBOLOS (KOKORO vs IPA PURO):")
    print(f" - Coda L vocalizada ([ʊ] do Kokoro vs [w] do SPX)    : {len(coda_l_diff)} palavras")
    print(f" - Átona Final -e ([y] do Kokoro vs [i] do SPX)       : {len(atona_e_diff)} palavras")
    print(f" - Átona Final -a ([æ] do Kokoro vs [ə] do SPX)       : {len(atona_a_diff)} palavras")
    print(f" - Vogal Epentética ([i/ʧi/ʤi] vs [ə] do SPX)         : {len(epenthesis_diff)} palavras")
    print("="*60)

    # Detalhar amostras
    print("\n--- AMOSTRAS: ERROS RESIDUAIS DE ACENTO TÔNICO ---")
    for w, pos, ref, pred, rv, pv in stress_wrong[:10]:
        print(f"{w:<18} ({pos:<4}) | REF: {ref:<20} ({rv}) | PRED: {pred:<20} ({pv})")

    if stress_missing:
        print("\n--- AMOSTRAS: PALAVRAS SEM ACENTO TÔNICO ---")
        for w, pos, ref, pred in stress_missing[:5]:
            print(f"{w:<18} | REF: {ref:<20} | PRED: {pred}")

    print("\n--- AMOSTRAS: ERROS RESIDUAIS DA LETRA X ---")
    for w, ref, pred in x_errors[:10]:
        print(f"{w:<18} | REF: {ref:<20} | PRED: {pred}")

    print("\n--- AMOSTRAS: VOGAIS ABERTAS NÃO ACENTUADAS (/ɛ/, /ɔ/) ---")
    for w, ref, pred in open_vowels_closed[:10]:
        print(f"{w:<18} | REF: {ref:<20} | PRED: {pred}")

    print("\n--- AMOSTRAS: DITONGAÇÃO DIANTE DE S NO SPX (arroz, paz, luz) ---")
    for w, ref, pred in diphthong_sibilant_diff[:10]:
        print(f"{w:<18} | REF: {ref:<20} | PRED: {pred}")

if __name__ == '__main__':
    main()
