#include "syllabifier.h"
#include <string.h>
#include <ctype.h>

namespace phonemas_ptbr::internal {

// ============================================================================
// 1. PREDICADOS FONOLÓGICOS E CARACTERES UTF-8
// ============================================================================

bool is_punct_char(char c) {
    return c == '.' || c == ',' || c == '!' || c == '?' || c == ';' || c == ':' ||
           c == '(' || c == ')' || c == '"' || c == '\'' || c == '-' || c == '/' ||
           c == '[' || c == ']';
}

bool is_vowel_char(std::string_view c) {
    if (c.size() == 1) {
        char ch = c[0];
        return (ch == 'a' || ch == 'e' || ch == 'i' || ch == 'o' || ch == 'u');
    } else if (c.size() == 2 && static_cast<unsigned char>(c[0]) == 0xC3) {
        unsigned char b1 = static_cast<unsigned char>(c[1]);
        return (b1 == 0xA1 || b1 == 0xA9 || b1 == 0xAD || b1 == 0xB3 || b1 == 0xBA || // á é í ó ú
                b1 == 0xA2 || b1 == 0xAA || b1 == 0xB4 ||                             // â ê ô
                b1 == 0xA3 || b1 == 0xB5 ||                                           // ã õ
                b1 == 0xA0 || b1 == 0xA8 || b1 == 0xB2);                              // à è ò
    }
    return false;
}

bool has_graphic_accent(std::string_view c) {
    if (c.size() == 2 && static_cast<unsigned char>(c[0]) == 0xC3) {
        unsigned char b1 = static_cast<unsigned char>(c[1]);
        return (b1 == 0xA1 || b1 == 0xA9 || b1 == 0xAD || b1 == 0xB3 || b1 == 0xBA || // á é í ó ú
                b1 == 0xA2 || b1 == 0xAA || b1 == 0xB4 ||                             // â ê ô
                b1 == 0xA3 || b1 == 0xB5 ||                                           // ã õ
                b1 == 0xA0 || b1 == 0xA8 || b1 == 0xB2);                              // à è ò
    }
    return false;
}

bool is_front_vowel(std::string_view c) {
    if (c.size() == 1) {
        return (c[0] == 'e' || c[0] == 'i');
    } else if (c.size() == 2 && static_cast<unsigned char>(c[0]) == 0xC3) {
        unsigned char b1 = static_cast<unsigned char>(c[1]);
        return (b1 == 0xA9 || b1 == 0xAA || b1 == 0xAD || b1 == 0xA8); // é ê í è
    }
    return false;
}

bool is_inseparable_cluster(std::string_view c1, std::string_view c2) {
    if (c1.size() == 1 && c2.size() == 1) {
        char a = c1[0], b = c2[0];
        // Dígrafos inseparáveis: ch, lh, nh, qu, gu
        if (b == 'h' && (a == 'c' || a == 'l' || a == 'n')) return true;
        if (b == 'u' && (a == 'q' || a == 'g')) return true;
        // Consoante oclusiva/fricativa + líquida (r ou l)
        if (b == 'r' && (a == 'p' || a == 'b' || a == 't' || a == 'd' ||
                         a == 'c' || a == 'g' || a == 'f' || a == 'v')) return true;
        if (b == 'l' && (a == 'p' || a == 'b' || a == 't' || a == 'c' ||
                         a == 'g' || a == 'f' || a == 'v')) return true;
    }
    return false;
}

bool is_diphthong(std::string_view c1, std::string_view c2) {
    if (c1.size() == 1 && c2.size() == 1) {
        char ch1 = c1[0], ch2 = c2[0];
        if (ch1 == 'e' && ch2 == 'i') return true;
        if (ch1 == 'a' && (ch2 == 'i' || ch2 == 'u')) return true;
        if (ch1 == 'o' && (ch2 == 'i' || ch2 == 'u')) return true;
        if (ch1 == 'e' && ch2 == 'u') return true;
        if (ch1 == 'i' && ch2 == 'u') return true;
        if (ch1 == 'u' && ch2 == 'i') return true;
    } else if (c1.size() == 2 && static_cast<unsigned char>(c1[0]) == 0xC3 && c2.size() == 1) {
        unsigned char b1 = static_cast<unsigned char>(c1[1]);
        char ch2 = c2[0];
        if (b1 == 0xA1 && (ch2 == 'i' || ch2 == 'u')) return true; // ái, áu
        if (b1 == 0xA9 && (ch2 == 'i' || ch2 == 'u')) return true; // éi, éu
        if (b1 == 0xAA && (ch2 == 'i' || ch2 == 'u')) return true; // êi, êu
        if (b1 == 0xB3 && (ch2 == 'i' || ch2 == 'u')) return true; // ói, óu
        if (b1 == 0xA3 && (ch2 == 'o' || ch2 == 'e')) return true; // ão, ãe
        if (b1 == 0xB5 && ch2 == 'e') return true;                 // õe
    }
    return false;
}

// ============================================================================
// 2. MOTOR FORMAL DE SILABIFICAÇÃO (ONSET - NUCLEUS - CODA)
// ============================================================================

void analyze_and_syllabify_word(std::string_view word_str, WordAnalysis& wa) {
    memset(&wa, 0, sizeof(WordAnalysis));
    if (word_str.empty()) return;

    // 1. Normalização para minúsculas local na stack
    wa.word_len = word_str.size() < sizeof(wa.lower_word) - 1 ? word_str.size() : sizeof(wa.lower_word) - 1;
    for (size_t k = 0; k < wa.word_len; ++k) {
        wa.lower_word[k] = static_cast<char>(tolower(static_cast<unsigned char>(word_str[k])));
    }
    wa.lower_word[wa.word_len] = '\0';

    // 2. Extração de caracteres UTF-8 como fatias zero-copy de string_view
    for (size_t i = 0; i < wa.word_len && wa.num_chars < 64; ) {
        unsigned char c = static_cast<unsigned char>(wa.lower_word[i]);
        uint8_t len = 1;
        if ((c & 0xE0) == 0xC0) len = 2;
        else if ((c & 0xF0) == 0xE0) len = 3;
        else if ((c & 0xF8) == 0xF0) len = 4;
        if (i + len > wa.word_len) len = static_cast<uint8_t>(wa.word_len - i);

        wa.orig_chars[wa.num_chars] = word_str.substr(i, len);
        wa.lower_chars[wa.num_chars] = std::string_view(wa.lower_word + i, len);
        wa.num_chars++;
        i += len;
    }
    if (wa.num_chars == 0) return;

    // 3. Marcação booleana de vogais (exclui 'u' mudo dos dígrafos qu/gu antes de qualquer vogal)
    bool is_v[64];
    for (size_t i = 0; i < wa.num_chars; ++i) {
        if (is_vowel_char(wa.lower_chars[i])) {
            if (wa.lower_chars[i] == "u" && i > 0 &&
                (wa.lower_chars[i - 1] == "q" || wa.lower_chars[i - 1] == "g") &&
                i + 1 < wa.num_chars && is_vowel_char(wa.lower_chars[i + 1])) {
                is_v[i] = false; // 'u' é dígrafo mudo ou glide onset [w]
            } else {
                is_v[i] = true;
            }
        } else {
            is_v[i] = false;
        }
    }

    // 4. Identificação de núcleos vocálicos (regras de hiato vs ditongo)
    for (size_t i = 0; i < wa.num_chars && wa.num_nuclei < 32; ) {
        if (is_v[i]) {
            uint8_t start = static_cast<uint8_t>(i);
            if (i + 1 < wa.num_chars && is_v[i + 1]) {
                std::string_view v1 = wa.lower_chars[i];
                std::string_view v2 = wa.lower_chars[i + 1];

                // Hiato se vogais iguais: aa, ee, oo, ii, uu
                bool is_hiatus_equal = (v1 == v2);

                // Hiato se ambas são abertas/médias: a, e, o com a, e, o (ex: teatro, poeta, voar, toalha)
                bool v1_open = (v1 == "a" || v1 == "e" || v1 == "o" ||
                                v1 == "á" || v1 == "é" || v1 == "ó" ||
                                v1 == "â" || v1 == "ê" || v1 == "ô");
                bool v2_open = (v2 == "a" || v2 == "e" || v2 == "o" ||
                                v2 == "á" || v2 == "é" || v2 == "ó" ||
                                v2 == "â" || v2 == "ê" || v2 == "ô");
                bool is_hiatus_open = (v1_open && v2_open);

                // Hiato se v2 possui acento gráfico: saúde, país, baú, ruído, ciúme
                bool is_hiatus_accent = has_graphic_accent(v2);

                // Hiato se v2 é i diante de nh: rainha, moinho, tainha
                bool is_hiatus_nh = ((v2 == "i" || v2 == "í") &&
                                     i + 3 < wa.num_chars &&
                                     wa.lower_chars[i + 2] == "n" && wa.lower_chars[i + 3] == "h");

                // Hiato se verbo em -ir no infinitivo (ex: atrair, construir, afluir, sair)
                bool is_hiatus_ir = ((v2 == "i" || v2 == "í") &&
                                     i + 2 == wa.num_chars - 1 && wa.lower_chars[i + 2] == "r");

                // Hiatos lexicais específicos sem acento: ainda, amendoim, ruim, raiz, juiz
                std::string_view lw(wa.lower_word, wa.word_len);
                bool is_hiatus_lexical = (lw == "ainda" && i == 0) ||
                                         (lw == "amendoim" && i == 5) ||
                                         (lw == "ruim" && i == 1) ||
                                         (lw == "raiz" && i == 1) ||
                                         (lw == "juiz" && i == 1);

                bool is_diph = is_diphthong(v1, v2);

                if (is_diph && !(is_hiatus_equal || is_hiatus_open || is_hiatus_accent ||
                                 is_hiatus_nh || is_hiatus_ir || is_hiatus_lexical)) {
                    wa.nuclei[wa.num_nuclei].start_idx = start;
                    wa.nuclei[wa.num_nuclei].len = 2;
                    wa.nuclei[wa.num_nuclei].has_accent = (has_graphic_accent(v1) || has_graphic_accent(v2));
                    wa.num_nuclei++;
                    i += 2;
                    continue;
                }
            }
            wa.nuclei[wa.num_nuclei].start_idx = start;
            wa.nuclei[wa.num_nuclei].len = 1;
            wa.nuclei[wa.num_nuclei].has_accent = has_graphic_accent(wa.lower_chars[i]);
            wa.num_nuclei++;
            i += 1;
        } else {
            i += 1;
        }
    }

    if (wa.num_nuclei == 0) {
        wa.cuts[0] = 0;
        wa.cuts[1] = static_cast<uint8_t>(wa.num_chars);
        wa.num_syllables = 1;
        return;
    }

    // 5. Determinação das fronteiras silábicas (cortes consonantais C-V)
    wa.cuts[0] = 0;
    wa.num_syllables = 1;

    for (size_t k = 0; k + 1 < wa.num_nuclei; ++k) {
        size_t n1_end = wa.nuclei[k].start_idx + wa.nuclei[k].len;
        size_t n2_start = wa.nuclei[k + 1].start_idx;
        size_t num_c = n2_start - n1_end;

        if (num_c == 0) {
            // V-V (hiato direto: corte entre as vogais)
            wa.cuts[wa.num_syllables++] = static_cast<uint8_t>(n2_start);
        } else if (num_c == 1) {
            // V-CV: consoante única pertence ao ataque da sílaba seguinte
            wa.cuts[wa.num_syllables++] = static_cast<uint8_t>(n1_end);
        } else if (num_c == 2) {
            std::string_view c1 = wa.lower_chars[n1_end];
            std::string_view c2 = wa.lower_chars[n1_end + 1];
            if (is_inseparable_cluster(c1, c2)) {
                // V-CCV: encontro inseparável vai para a sílaba seguinte
                wa.cuts[wa.num_syllables++] = static_cast<uint8_t>(n1_end);
            } else {
                // VC-CV: coda na primeira e ataque na segunda (ex: car-ro, pas-so, por-ta)
                wa.cuts[wa.num_syllables++] = static_cast<uint8_t>(n1_end + 1);
            }
        } else if (num_c == 3) {
            std::string_view c2 = wa.lower_chars[n1_end + 1];
            std::string_view c3 = wa.lower_chars[n1_end + 2];
            if (is_inseparable_cluster(c2, c3)) {
                // VC-CCV: ex: en-trar, es-tra-da
                wa.cuts[wa.num_syllables++] = static_cast<uint8_t>(n1_end + 1);
            } else {
                // VCC-CV: ex: trans-por-te, cons-tar
                wa.cuts[wa.num_syllables++] = static_cast<uint8_t>(n1_end + 2);
            }
        } else {
            // VCC-CCV: ex: cons-tru-ir, abs-tra-ir
            wa.cuts[wa.num_syllables++] = static_cast<uint8_t>(n1_end + 2);
        }
    }
    wa.cuts[wa.num_syllables] = static_cast<uint8_t>(wa.num_chars);

    // 6. Cálculo da acentuação tônica (Stress Assignment)
    wa.tonic_syl_idx = -1;
    for (size_t idx = 0; idx < wa.num_nuclei; ++idx) {
        if (wa.nuclei[idx].has_accent) {
            wa.tonic_syl_idx = static_cast<int>(idx);
            break;
        }
    }

    if (wa.tonic_syl_idx == -1 && wa.num_nuclei > 0) {
        std::string_view lw(wa.lower_word, wa.word_len);
        bool ends_with_mente = (lw.size() >= 5 && lw.substr(lw.size() - 5) == "mente");
        if (ends_with_mente && wa.num_nuclei >= 2) {
            wa.tonic_syl_idx = static_cast<int>(wa.num_nuclei) - 2;
            wa.stress_type = StressType::Paroxytone;
        } else {
            std::string_view last = wa.lower_chars[wa.num_chars - 1];
            // Regra canônica de oxítonas sem acento no português:
            // Terminadas em r, l, z, x, i, u, is, us, im, um, ins, uns, om, ons
            bool oxytone = (last == "r" || last == "l" || last == "z" ||
                            last == "x" || last == "i" || last == "u");

            if (wa.num_chars >= 2) {
                std::string_view penult = wa.lower_chars[wa.num_chars - 2];
                if (last == "s" && (penult == "i" || penult == "u")) oxytone = true;
                else if (last == "m" && (penult == "i" || penult == "u" || penult == "o")) oxytone = true; // im, um, om (batom)
            }
            if (wa.num_chars >= 3) {
                std::string_view antep = wa.lower_chars[wa.num_chars - 3];
                std::string_view penult = wa.lower_chars[wa.num_chars - 2];
                if (last == "s" && penult == "n" && (antep == "i" || antep == "u" || antep == "o")) oxytone = true; // ins, uns, ons
            }

            // Casos lexicais de oxítonas sem acento (verbos em -ir com hiato)
            if (lw.size() >= 3 && (lw.substr(lw.size() - 3) == "air" ||
                                  lw.substr(lw.size() - 3) == "uir" ||
                                  lw.substr(lw.size() - 3) == "oir")) {
                oxytone = true;
            }

            if (oxytone) {
                wa.tonic_syl_idx = static_cast<int>(wa.num_nuclei) - 1;
                wa.stress_type = StressType::Oxytone;
            } else {
                wa.tonic_syl_idx = wa.num_nuclei >= 2 ? static_cast<int>(wa.num_nuclei) - 2 : 0;
                wa.stress_type = (wa.num_nuclei >= 2) ? StressType::Paroxytone : StressType::Oxytone;
            }
        }
    }

    if (wa.tonic_syl_idx >= 0 && wa.tonic_syl_idx < static_cast<int>(wa.num_nuclei)) {
        wa.tonic_char_idx = static_cast<int>(wa.nuclei[wa.tonic_syl_idx].start_idx);
    } else {
        wa.tonic_char_idx = -1;
    }

    // 7. Acentuação métrica secundária (ˌ) em polissílabos reais (>= 4 sílabas ou -mente)
    wa.sec_syl_idx = -1;
    wa.sec_char_idx = -1;
    bool ends_with_mente = (wa.word_len >= 5 && strcmp(wa.lower_word + wa.word_len - 5, "mente") == 0);
    if ((wa.num_nuclei >= 4 || ends_with_mente) && wa.tonic_syl_idx > 1) {
        wa.sec_syl_idx = 0;
        wa.sec_char_idx = static_cast<int>(wa.nuclei[0].start_idx);
    }
}

void syllabify_word_buf(std::string_view word_str, char*& dst, const char* dst_end, char sep) {
    if (word_str.empty()) return;

    WordAnalysis wa;
    analyze_and_syllabify_word(word_str, wa);

    for (size_t s = 0; s < wa.num_syllables; ++s) {
        if (s > 0 && dst < dst_end - 1) {
            *dst++ = sep;
        }
        for (size_t idx = wa.cuts[s]; idx < wa.cuts[s + 1] && dst < dst_end - 1; ++idx) {
            std::string_view ch = wa.orig_chars[idx];
            for (char b : ch) {
                if (dst < dst_end - 1) *dst++ = b;
            }
        }
    }
    *dst = '\0';
}

size_t syllabify_text_buf(std::string_view text, char* out_buf, size_t max_len, char sep) {
    if (text.empty() || !out_buf || max_len == 0) return 0;

    char* dst = out_buf;
    const char* dst_end = out_buf + max_len;
    *dst = '\0';

    size_t i = 0;
    size_t text_len = text.size();

    while (i < text_len) {
        // Pula e preserva espaços em branco
        while (i < text_len && isspace(static_cast<unsigned char>(text[i]))) {
            if (dst < dst_end - 1) *dst++ = text[i];
            i++;
        }
        if (i >= text_len) break;

        size_t token_start = i;
        while (i < text_len && !isspace(static_cast<unsigned char>(text[i]))) i++;
        size_t token_end = i;

        // Extrai pontuação à esquerda
        size_t core_start = token_start;
        while (core_start < token_end && is_punct_char(text[core_start])) {
            core_start++;
        }

        // Extrai pontuação à direita
        size_t core_end = token_end;
        while (core_end > core_start && is_punct_char(text[core_end - 1])) {
            core_end--;
        }

        // Emite pontuação à esquerda
        for (size_t k = token_start; k < core_start && dst < dst_end - 1; ++k) {
            *dst++ = text[k];
        }
        *dst = '\0';

        // Emite a palavra silabificada (Onset-Nucleus-Coda)
        if (core_end > core_start) {
            std::string_view core = text.substr(core_start, core_end - core_start);
            syllabify_word_buf(core, dst, dst_end, sep);
        }

        // Emite pontuação à direita
        for (size_t k = core_end; k < token_end && dst < dst_end - 1; ++k) {
            *dst++ = text[k];
        }
        *dst = '\0';
    }

    *dst = '\0';
    return static_cast<size_t>(dst - out_buf);
}

} // namespace phonemas_ptbr::internal
