#ifndef PHONEMAS_PTBR_SYLLABIFIER_H
#define PHONEMAS_PTBR_SYLLABIFIER_H

#include "phonemas_ptbr_types.h"
#include <string.h>
#include <ctype.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// 1. PREDICADOS FONOLÓGICOS E CARACTERES UTF-8
// ============================================================================

/**
 * @brief Compara um CharRef UTF-8 com uma string C literal.
 */
static inline bool char_eq(CharRef c, const char* lit) {
    size_t lit_len = strlen(lit);
    return (c.len == lit_len && memcmp(c.p, lit, lit_len) == 0);
}

/**
 * @brief Verifica se o caractere ASCII é pontuação de separação de palavras.
 */
static inline bool is_punct(char c) {
    return c == '.' || c == ',' || c == '!' || c == '?' || c == ';' || c == ':' ||
           c == '(' || c == ')' || c == '"' || c == '\'' || c == '-' || c == '/' ||
           c == '[' || c == ']';
}

/**
 * @brief Identifica se o caractere UTF-8 representa uma vogal (oral, nasal ou acentuada).
 */
static inline bool is_vowel_char(CharRef c) {
    if (c.len == 1) {
        char ch = c.p[0];
        return (ch == 'a' || ch == 'e' || ch == 'i' || ch == 'o' || ch == 'u');
    } else if (c.len == 2 && (unsigned char)c.p[0] == 0xC3) {
        unsigned char b1 = (unsigned char)c.p[1];
        return (b1 == 0xA1 || b1 == 0xA9 || b1 == 0xAD || b1 == 0xB3 || b1 == 0xBA || // á é í ó ú
                b1 == 0xA2 || b1 == 0xAA || b1 == 0xB4 ||                             // â ê ô
                b1 == 0xA3 || b1 == 0xB5 ||                                           // ã õ
                b1 == 0xA0 || b1 == 0xA8 || b1 == 0xB2);                              // à è ò
    }
    return false;
}

/**
 * @brief Identifica se o caractere carrega acento gráfico explícito.
 */
static inline bool has_graphic_accent(CharRef c) {
    if (c.len == 2 && (unsigned char)c.p[0] == 0xC3) {
        unsigned char b1 = (unsigned char)c.p[1];
        return (b1 == 0xA1 || b1 == 0xA9 || b1 == 0xAD || b1 == 0xB3 || b1 == 0xBA || // á é í ó ú
                b1 == 0xA2 || b1 == 0xAA || b1 == 0xB4 ||                             // â ê ô
                b1 == 0xA3 || b1 == 0xB5 ||                                           // ã õ
                b1 == 0xA0 || b1 == 0xA8 || b1 == 0xB2);                              // à è ò
    }
    return false;
}

/**
 * @brief Identifica vogais anteriores (front vowels: e, i e variações acentuadas).
 * Disparam palatalização em 't'/'d' e suavização em 'c'/'g'.
 */
static inline bool is_front_vowel(CharRef c) {
    if (c.len == 1) {
        return (c.p[0] == 'e' || c.p[0] == 'i');
    } else if (c.len == 2 && (unsigned char)c.p[0] == 0xC3) {
        unsigned char b1 = (unsigned char)c.p[1];
        return (b1 == 0xA9 || b1 == 0xAA || b1 == 0xAD || b1 == 0xA8); // é ê í è
    }
    return false;
}

/**
 * @brief Identifica encontros consonantais inseparáveis (ataques complexos e dígrafos).
 * Conforme a gramática do português, permanecem unidos na sílaba seguinte.
 */
static inline bool is_inseparable_cluster(CharRef c1, CharRef c2) {
    if (c1.len == 1 && c2.len == 1) {
        char a = c1.p[0], b = c2.p[0];
        // Dígrafos inseparáveis: ch, lh, nh, qu, gu
        if (b == 'h' && (a == 'c' || a == 'l' || a == 'n')) return true;
        if (b == 'u' && (a == 'q' || a == 'g')) return true;
        // Consoante oclusiva/fricativa + líquida (r ou l): pr, br, tr, dr, cr, gr, fr, vr, pl, bl, tl, cl, gl, fl, vl
        if (b == 'r' && (a == 'p' || a == 'b' || a == 't' || a == 'd' ||
                         a == 'c' || a == 'g' || a == 'f' || a == 'v')) return true;
        if (b == 'l' && (a == 'p' || a == 'b' || a == 't' || a == 'c' ||
                         a == 'g' || a == 'f' || a == 'v')) return true;
    }
    return false;
}

/**
 * @brief Determina se dois caracteres vocálicos consecutivos formam um ditongo decrescente.
 */
static inline bool is_diphthong(CharRef c1, CharRef c2) {
    if (c1.len == 1 && c2.len == 1) {
        char ch1 = c1.p[0], ch2 = c2.p[0];
        if (ch1 == 'e' && ch2 == 'i') return true;
        if (ch1 == 'a' && (ch2 == 'i' || ch2 == 'u')) return true;
        if (ch1 == 'o' && (ch2 == 'i' || ch2 == 'u')) return true;
        if (ch1 == 'e' && ch2 == 'u') return true;
        if (ch1 == 'i' && ch2 == 'u') return true;
        if (ch1 == 'u' && ch2 == 'i') return true;
    } else if (c1.len == 2 && (unsigned char)c1.p[0] == 0xC3 && c2.len == 1) {
        unsigned char b1 = (unsigned char)c1.p[1];
        char ch2 = c2.p[0];
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
// 2. MOTOR FORMAL DE SILABIFICAÇÃO E ANÁLISE FONOLÓGICA (ONSET - NUCLEUS - CODA)
// ============================================================================

/**
 * @brief Realiza a análise fonológica e divisão silábica completa de uma palavra.
 * Executa 100% na stack (zero heap).
 */
static inline void analyze_and_syllabify_word(const char* word_str, size_t word_len, WordAnalysis* wa) {
    memset(wa, 0, sizeof(WordAnalysis));
    if (word_len == 0) return;

    // 1. Normalização para minúsculas
    wa->word_len = word_len < sizeof(wa->lower_word) - 1 ? word_len : sizeof(wa->lower_word) - 1;
    for (size_t k = 0; k < wa->word_len; ++k) {
        wa->lower_word[k] = (char)tolower((unsigned char)word_str[k]);
    }
    wa->lower_word[wa->word_len] = '\0';

    // 2. Extração de caracteres UTF-8
    for (size_t i = 0; i < wa->word_len && wa->num_chars < 64; ) {
        unsigned char c = (unsigned char)wa->lower_word[i];
        uint8_t len = 1;
        if ((c & 0xE0) == 0xC0) len = 2;
        else if ((c & 0xF0) == 0xE0) len = 3;
        else if ((c & 0xF8) == 0xF0) len = 4;
        if (i + len > wa->word_len) len = (uint8_t)(wa->word_len - i);

        wa->orig_chars[wa->num_chars].p = word_str + i;
        wa->orig_chars[wa->num_chars].len = len;
        wa->lower_chars[wa->num_chars].p = wa->lower_word + i;
        wa->lower_chars[wa->num_chars].len = len;
        wa->num_chars++;
        i += len;
    }
    if (wa->num_chars == 0) return;

    // 3. Marcação de vogais (exclui 'u' mudo dos dígrafos qu/gu antes de qualquer vogal)
    bool is_v[64];
    for (size_t i = 0; i < wa->num_chars; ++i) {
        if (is_vowel_char(wa->lower_chars[i])) {
            if (char_eq(wa->lower_chars[i], "u") && i > 0 &&
                (char_eq(wa->lower_chars[i - 1], "q") || char_eq(wa->lower_chars[i - 1], "g")) &&
                i + 1 < wa->num_chars && is_vowel_char(wa->lower_chars[i + 1])) {
                is_v[i] = false; // 'u' é dígrafo mudo ou glide onset [w]
            } else {
                is_v[i] = true;
            }
        } else {
            is_v[i] = false;
        }
    }

    // 4. Identificação de núcleos vocálicos (regras refinadas de hiato vs ditongo)
    for (size_t i = 0; i < wa->num_chars && wa->num_nuclei < 32; ) {
        if (is_v[i]) {
            uint8_t start = (uint8_t)i;
            if (i + 1 < wa->num_chars && is_v[i + 1]) {
                CharRef v1 = wa->lower_chars[i];
                CharRef v2 = wa->lower_chars[i + 1];

                // Hiato se vogais iguais: aa, ee, oo, ii, uu
                bool is_hiatus_equal = (v1.len == v2.len && memcmp(v1.p, v2.p, v1.len) == 0);
                // Hiato se ambas são abertas/médias: a, e, o com a, e, o (ex: teatro, poeta, voar, toalha)
                bool v1_open = (char_eq(v1, "a") || char_eq(v1, "e") || char_eq(v1, "o") ||
                                char_eq(v1, "á") || char_eq(v1, "é") || char_eq(v1, "ó") ||
                                char_eq(v1, "â") || char_eq(v1, "ê") || char_eq(v1, "ô"));
                bool v2_open = (char_eq(v2, "a") || char_eq(v2, "e") || char_eq(v2, "o") ||
                                char_eq(v2, "á") || char_eq(v2, "é") || char_eq(v2, "ó") ||
                                char_eq(v2, "â") || char_eq(v2, "ê") || char_eq(v2, "ô"));
                bool is_hiatus_open = (v1_open && v2_open);
                // Hiato se v2 possui acento gráfico: saúde, país, baú, ruído, ciúme
                bool is_hiatus_accent = has_graphic_accent(v2);
                // Hiato se v2 é i diante de nh: rainha, moinho, tainha
                bool is_hiatus_nh = ((char_eq(v2, "i") || char_eq(v2, "í")) &&
                                     i + 3 < wa->num_chars &&
                                     char_eq(wa->lower_chars[i + 2], "n") && char_eq(wa->lower_chars[i + 3], "h"));
                // Hiato se verbo em -ir no infinitivo (ex: atrair, construir, afluir, sair)
                bool is_hiatus_ir = ((char_eq(v2, "i") || char_eq(v2, "í")) &&
                                     i + 2 == wa->num_chars - 1 && char_eq(wa->lower_chars[i + 2], "r"));
                // Hiatos lexicais específicos sem acento: ainda, amendoim, ruim, raiz, juiz
                bool is_hiatus_lexical = (strcmp(wa->lower_word, "ainda") == 0 && i == 0) ||
                                         (strcmp(wa->lower_word, "amendoim") == 0 && i == 5) ||
                                         (strcmp(wa->lower_word, "ruim") == 0 && i == 1) ||
                                         (strcmp(wa->lower_word, "raiz") == 0 && i == 1) ||
                                         (strcmp(wa->lower_word, "juiz") == 0 && i == 1);

                bool is_diph = is_diphthong(v1, v2);

                if (is_diph && !(is_hiatus_equal || is_hiatus_open || is_hiatus_accent ||
                                 is_hiatus_nh || is_hiatus_ir || is_hiatus_lexical)) {
                    wa->nuclei[wa->num_nuclei].start_idx = start;
                    wa->nuclei[wa->num_nuclei].len = 2;
                    wa->nuclei[wa->num_nuclei].has_accent = (has_graphic_accent(v1) || has_graphic_accent(v2));
                    wa->num_nuclei++;
                    i += 2;
                    continue;
                }
            }
            wa->nuclei[wa->num_nuclei].start_idx = start;
            wa->nuclei[wa->num_nuclei].len = 1;
            wa->nuclei[wa->num_nuclei].has_accent = has_graphic_accent(wa->lower_chars[i]);
            wa->num_nuclei++;
            i += 1;
        } else {
            i += 1;
        }
    }

    if (wa->num_nuclei == 0) {
        wa->cuts[0] = 0;
        wa->cuts[1] = (uint8_t)wa->num_chars;
        wa->num_syllables = 1;
        return;
    }

    // 5. Determinação das fronteiras silábicas (cortes consonantais C-V)
    wa->cuts[0] = 0;
    wa->num_syllables = 1;

    for (size_t k = 0; k + 1 < wa->num_nuclei; ++k) {
        size_t n1_end = wa->nuclei[k].start_idx + wa->nuclei[k].len;
        size_t n2_start = wa->nuclei[k + 1].start_idx;
        size_t num_c = n2_start - n1_end;

        if (num_c == 0) {
            // V-V (hiato direto: corte entre as vogais)
            wa->cuts[wa->num_syllables++] = (uint8_t)n2_start;
        } else if (num_c == 1) {
            // V-CV: consoante única pertence ao ataque da sílaba seguinte
            wa->cuts[wa->num_syllables++] = (uint8_t)n1_end;
        } else if (num_c == 2) {
            CharRef c1 = wa->lower_chars[n1_end];
            CharRef c2 = wa->lower_chars[n1_end + 1];
            if (is_inseparable_cluster(c1, c2)) {
                // V-CCV: encontro inseparável vai para a sílaba seguinte
                wa->cuts[wa->num_syllables++] = (uint8_t)n1_end;
            } else {
                // VC-CV: coda na primeira e ataque na segunda (ex: car-ro, pas-so, por-ta)
                wa->cuts[wa->num_syllables++] = (uint8_t)(n1_end + 1);
            }
        } else if (num_c == 3) {
            CharRef c2 = wa->lower_chars[n1_end + 1];
            CharRef c3 = wa->lower_chars[n1_end + 2];
            if (is_inseparable_cluster(c2, c3)) {
                // VC-CCV: ex: en-trar, es-tra-da
                wa->cuts[wa->num_syllables++] = (uint8_t)(n1_end + 1);
            } else {
                // VCC-CV: ex: trans-por-te, cons-tar
                wa->cuts[wa->num_syllables++] = (uint8_t)(n1_end + 2);
            }
        } else {
            // VCC-CCV: ex: cons-tru-ir, abs-tra-ir
            wa->cuts[wa->num_syllables++] = (uint8_t)(n1_end + 2);
        }
    }
    wa->cuts[wa->num_syllables] = (uint8_t)wa->num_chars;

    // 6. Cálculo da acentuação tônica (Stress Assignment)
    wa->tonic_syl_idx = -1;
    for (size_t idx = 0; idx < wa->num_nuclei; ++idx) {
        if (wa->nuclei[idx].has_accent) {
            wa->tonic_syl_idx = (int)idx;
            break;
        }
    }

    if (wa->tonic_syl_idx == -1 && wa->num_nuclei > 0) {
        bool ends_with_mente = (wa->word_len >= 5 && strcmp(wa->lower_word + wa->word_len - 5, "mente") == 0);
        if (ends_with_mente && wa->num_nuclei >= 2) {
            wa->tonic_syl_idx = (int)wa->num_nuclei - 2;
            wa->stress_type = STRESS_PAROXYTONE;
        } else {
            CharRef last = wa->lower_chars[wa->num_chars - 1];
            // Regra canônica de oxítonas sem acento no português:
            // Terminadas em r, l, z, x, i, u, is, us, im, um, ins, uns, om, ons
            bool oxytone = (char_eq(last, "r") || char_eq(last, "l") || char_eq(last, "z") ||
                            char_eq(last, "x") || char_eq(last, "i") || char_eq(last, "u"));

            if (wa->num_chars >= 2) {
                CharRef penult = wa->lower_chars[wa->num_chars - 2];
                if (char_eq(last, "s") && (char_eq(penult, "i") || char_eq(penult, "u"))) oxytone = true;
                else if (char_eq(last, "m") && (char_eq(penult, "i") || char_eq(penult, "u") || char_eq(penult, "o"))) oxytone = true; // im, um, om (batom, garçom)
            }
            if (wa->num_chars >= 3) {
                CharRef antep = wa->lower_chars[wa->num_chars - 3];
                CharRef penult = wa->lower_chars[wa->num_chars - 2];
                if (char_eq(last, "s") && char_eq(penult, "n") && (char_eq(antep, "i") || char_eq(antep, "u") || char_eq(antep, "o"))) oxytone = true; // ins, uns, ons
            }

            // Casos lexicais de oxítonas sem acento (verbos em -ir com hiato)
            if (wa->word_len >= 3 && (strcmp(wa->lower_word + wa->word_len - 3, "air") == 0 ||
                                      strcmp(wa->lower_word + wa->word_len - 3, "uir") == 0 ||
                                      strcmp(wa->lower_word + wa->word_len - 3, "oir") == 0)) {
                oxytone = true;
            }

            if (oxytone) {
                wa->tonic_syl_idx = (int)wa->num_nuclei - 1;
                wa->stress_type = STRESS_OXYTONE;
            } else {
                wa->tonic_syl_idx = wa->num_nuclei >= 2 ? (int)wa->num_nuclei - 2 : 0;
                wa->stress_type = (wa->num_nuclei >= 2) ? STRESS_PAROXYTONE : STRESS_OXYTONE;
            }
        }
    }

    if (wa->tonic_syl_idx >= 0 && wa->tonic_syl_idx < (int)wa->num_nuclei) {
        wa->tonic_char_idx = (int)wa->nuclei[wa->tonic_syl_idx].start_idx;
    } else {
        wa->tonic_char_idx = -1;
    }

    // 7. Acentuação métrica secundária (ˌ) em polissílabos (>= 3 sílabas)
    wa->sec_syl_idx = -1;
    wa->sec_char_idx = -1;
    if (wa->num_nuclei >= 3 && wa->tonic_syl_idx > 0) {
        wa->sec_syl_idx = 0;
        wa->sec_char_idx = (int)wa->nuclei[0].start_idx;
    }
}

/**
 * @brief Silabifica uma palavra e escreve as sílabas no buffer dst com separador sep.
 */
static inline void syllabify_word_buf(const char* word_str, size_t word_len, char** dst, const char* dst_end, char sep) {
    WordAnalysis wa;
    analyze_and_syllabify_word(word_str, word_len, &wa);

    for (size_t s = 0; s < wa.num_syllables; ++s) {
        if (s > 0 && *dst < dst_end - 1) {
            *(*dst)++ = sep;
        }
        for (size_t idx = wa.cuts[s]; idx < wa.cuts[s + 1] && *dst < dst_end - 1; ++idx) {
            for (uint8_t b = 0; b < wa.orig_chars[idx].len && *dst < dst_end - 1; ++b) {
                *(*dst)++ = wa.orig_chars[idx].p[b];
            }
        }
    }
    **dst = '\0';
}

#ifdef __cplusplus
} // extern "C"
#endif

#endif // PHONEMAS_PTBR_SYLLABIFIER_H
