#ifndef PHONEMAS_PTBR_RULES_H
#define PHONEMAS_PTBR_RULES_H

#include "phonemas_ptbr_types.h"
#include "phonemas_ptbr_syllabifier.h"
#include "embedded_exceptions.h"
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// 1. CLASSIFICADOR CONTEXTUAL DA LETRA X
// ============================================================================

/**
 * @brief Classifica o valor fonético contextual da letra 'X' em Português Brasileiro.
 * Suporta detecção de raízes greco-latinas em /ks/ mesmo com prefixos comuns (anti-, auto-, des-).
 */
static inline const char* classify_x_ptbr(const char* word, const CharRef* chars, size_t n, size_t i) {
    if (!char_eq(chars[i], "x")) return NULL;

    CharRef prev = (i > 0) ? chars[i - 1] : (CharRef){"", 0};
    CharRef next = (i + 1 < n) ? chars[i + 1] : (CharRef){"", 0};

    // 1. X em início absoluto de palavra: SEMPRE /ʃ/ (xícara, xadrez, xarope, xampu, xale, xaveco)
    if (i == 0) return "ʃ";

    // 2. Prefixo en- + X: SEMPRE /ʃ/ (enxame, enxada, enxugar, enxofre, enxaqueca)
    if (i >= 2 && char_eq(chars[i - 2], "e") && char_eq(chars[i - 1], "n")) {
        return "ʃ";
    }

    // 3. Sequência inicial 'ex-'
    if (i == 1 && char_eq(chars[0], "e")) {
        if (is_vowel_char(next)) return "z"; // exame, exemplo, existir, exército, exato
        if (char_eq(next, "p") || char_eq(next, "t") || char_eq(next, "c") ||
            char_eq(next, "s") || char_eq(next, "f")) return "s"; // explicar, extra, excelente
        return "s";
    }

    // 4. Palavras conhecidas com som de /s/ (aproximar, próximo, máximo, auxílio, trouxe)
    if (strncmp(word, "próxim", strlen("próxim")) == 0 || strncmp(word, "proxim", 6) == 0 ||
        strncmp(word, "aproxim", 7) == 0 || strncmp(word, "máxim", strlen("máxim")) == 0 ||
        strncmp(word, "maxim", 5) == 0 || strncmp(word, "auxili", 6) == 0 ||
        strncmp(word, "auxíli", strlen("auxíli")) == 0 || strncmp(word, "troux", 5) == 0) {
        return "s";
    }

    // 5. Raízes cultas greco-latinas com som de /ks/ (detectadas no início ou após prefixos como anti-, auto-, des-)
    const char* ks_roots[] = {
        "fix", "sex", "conex", "complex", "flex", "reflex", "paradox",
        "táxi", "taxi", "tóx", "tox", "léx", "lex", "oxig", "asfix",
        "anex", "nex", "maxil", "prolix", "ortodox", "heterodox", NULL
    };
    for (size_t r = 0; ks_roots[r] != NULL; ++r) {
        if (strstr(word, ks_roots[r]) != NULL) {
            return "ks";
        }
    }

    // 6. Ditongo decrescente + X: som de /ʃ/ (caixa, peixe, frouxo, baixo, feixe)
    if (i >= 2) {
        CharRef preprev = chars[i - 2];
        if ((char_eq(preprev, "a") && char_eq(prev, "i")) ||
            (char_eq(preprev, "e") && char_eq(prev, "i")) ||
            (char_eq(preprev, "o") && char_eq(prev, "u")) ||
            (char_eq(preprev, "a") && char_eq(prev, "u"))) return "ʃ";
    }

    // 7. Após vogais altas 'u' ou 'i': som de /ʃ/ (luxo, puxar, bruxa, lixo)
    if (char_eq(prev, "u") || char_eq(prev, "i")) return "ʃ";

    // 8. X em final de palavra: SEMPRE /ks/ em pt-BR (tórax, clímax, duplex, látex, fênix, córtex)
    if (i == n - 1) return "ks";

    // 9. X antes de consoante interna: som de /s/ (texto, sexta, contexto)
    if (!is_vowel_char(next)) return "s";

    // 10. Fallback canônico para X entre vogais em português: som de /ʃ/ (abacaxi, faxina, graxa, taxa, mexer)
    return "ʃ";
}

// ============================================================================
// 2. BUSCA BINÁRIA NA TABELA DE EXCEÇÕES EM FLASH (.RODATA)
// ============================================================================

/**
 * @brief Busca binária em O(log N) na tabela embutida de exceções.
 */
static inline const char* find_exception(const char* word) {
    if (EMBEDDED_EXCEPTIONS_SIZE == 0) return NULL;
    int low = 0;
    int high = (int)EMBEDDED_EXCEPTIONS_SIZE - 1;
    while (low <= high) {
        int mid = low + (high - low) / 2;
        int cmp = strcmp(word, EMBEDDED_EXCEPTIONS[mid].word);
        if (cmp == 0) {
            return EMBEDDED_EXCEPTIONS[mid].phonemes;
        } else if (cmp < 0) {
            high = mid - 1;
        } else {
            low = mid + 1;
        }
    }
    return NULL;
}

// ============================================================================
// 3. MOTOR DE APLICAÇÃO DE REGRAS FONÉTICAS DE UMA PALAVRA
// ============================================================================

/**
 * @brief Emite uma string literal em um buffer de destino sem alocação dinâmica.
 */
static inline void emit_str(char** dst, const char* end, const char* str) {
    while (*str && *dst < end - 1) {
        *(*dst)++ = *str++;
    }
    **dst = '\0';
}

/**
 * @brief Transcreve uma única palavra simples em fonemas IPA do português brasileiro.
 */
static inline void phonemize_single_word_buf(const char* word_str, size_t word_len, char** dst, const char* dst_end) {
    if (word_len == 0) return;

    // Buffer local para lowercase (máximo 64 bytes para uma única palavra em pt-BR)
    char lower_word[64];
    size_t w_len = word_len < sizeof(lower_word) - 1 ? word_len : sizeof(lower_word) - 1;
    for (size_t k = 0; k < w_len; ++k) {
        lower_word[k] = (char)tolower((unsigned char)word_str[k]);
    }
    lower_word[w_len] = '\0';

    // 1. Busca na tabela de exceções em Flash (.rodata)
    const char* exc = find_exception(lower_word);
    if (exc != NULL) {
        emit_str(dst, dst_end, exc);
        return;
    }

    // 2. Análise silábica estruturada e acentuação formal
    WordAnalysis wa;
    analyze_and_syllabify_word(lower_word, w_len, &wa);

    size_t n = wa.num_chars;
    CharRef* chars = wa.lower_chars;
    int tonic_char_pos = wa.tonic_char_idx;
    int sec_char_pos = wa.sec_char_idx;

    // 3. Varredura e transformação fonética
    for (size_t i = 0; i < n && *dst < dst_end - 1; ) {
        CharRef c = chars[i];
        CharRef next = (i + 1 < n) ? chars[i + 1] : (CharRef){"", 0};
        CharRef next2 = (i + 2 < n) ? chars[i + 2] : (CharRef){"", 0};
        CharRef prev = (i > 0) ? chars[i - 1] : (CharRef){"", 0};

        // Emissão de acento primário (ˈ) e secundário (ˌ)
        if ((int)i == tonic_char_pos) {
            emit_str(dst, dst_end, "ˈ");
        } else if ((int)i == sec_char_pos) {
            emit_str(dst, dst_end, "ˌ");
        }

        // Letra X contextual
        if (char_eq(c, "x")) {
            const char* x_ph = classify_x_ptbr(lower_word, chars, n, i);
            if (x_ph != NULL) {
                emit_str(dst, dst_end, x_ph);
                if (i == 1 && char_eq(next, "c") && is_front_vowel(next2)) i += 2;
                else i++;
                continue;
            }
        }

        // Dígrafos inseparáveis
        if (char_eq(c, "c") && char_eq(next, "h")) { emit_str(dst, dst_end, "ʃ"); i += 2; continue; }
        if (char_eq(c, "l") && char_eq(next, "h")) { emit_str(dst, dst_end, "ʎ"); i += 2; continue; }
        if (char_eq(c, "n") && char_eq(next, "h")) { emit_str(dst, dst_end, "ɲ"); i += 2; continue; }
        if (char_eq(c, "r") && char_eq(next, "r")) { emit_str(dst, dst_end, "x"); i += 2; continue; }
        if (char_eq(c, "s") && char_eq(next, "s")) { emit_str(dst, dst_end, "s"); i += 2; continue; }

        // Q e G antes de U
        if (char_eq(c, "q") && char_eq(next, "u")) {
            if (is_front_vowel(next2)) { emit_str(dst, dst_end, "k"); i += 2; continue; }
            else { emit_str(dst, dst_end, "kw"); i += 2; continue; }
        }
        if (char_eq(c, "g") && char_eq(next, "u")) {
            if (is_front_vowel(next2)) { emit_str(dst, dst_end, "ɡ"); i += 2; continue; }
            else if (is_vowel_char(next2)) { emit_str(dst, dst_end, "ɡw"); i += 2; continue; }
        }

        // C e Ç
        if (char_eq(c, "c")) {
            if (is_front_vowel(next)) emit_str(dst, dst_end, "s");
            else if (!is_vowel_char(next) && !char_eq(next, "r") && !char_eq(next, "l") && !char_eq(next, "h") && i + 1 < n) {
                emit_str(dst, dst_end, "ki"); // Epêntese de C mudo (compacto, aspecto, técnico)
            } else {
                emit_str(dst, dst_end, "k");
            }
            i++; continue;
        }
        if (char_eq(c, "ç")) { emit_str(dst, dst_end, "s"); i++; continue; }

        // G e J
        if (char_eq(c, "g")) {
            if (is_front_vowel(next)) emit_str(dst, dst_end, "ʒ");
            else if (!is_vowel_char(next) && !char_eq(next, "r") && !char_eq(next, "l") && i + 1 < n) {
                emit_str(dst, dst_end, "ɡi"); // Epêntese de G mudo (signo, digno, gnomo)
            } else {
                emit_str(dst, dst_end, "ɡ");
            }
            i++; continue;
        }
        if (char_eq(c, "j")) { emit_str(dst, dst_end, "ʒ"); i++; continue; }

        // H mudo inicial
        if (char_eq(c, "h") && i == 0) { i++; continue; }

        // R (início, tepe intervocálico e vibrante)
        if (char_eq(c, "r")) {
            if (i == 0 || char_eq(prev, "n") || char_eq(prev, "s") || char_eq(prev, "l")) {
                emit_str(dst, dst_end, "x");
            } else if (char_eq(prev, "b") || char_eq(prev, "p") || char_eq(prev, "t") ||
                       char_eq(prev, "d") || char_eq(prev, "c") || char_eq(prev, "g") ||
                       char_eq(prev, "f") || char_eq(prev, "v")) {
                emit_str(dst, dst_end, "r");
            } else {
                emit_str(dst, dst_end, "ɾ");
            }
            i++; continue;
        }

        // S (vozeamento assimilatório diante de sonoras em coda: mesmo, desde, rasgo)
        if (char_eq(c, "s")) {
            if (is_vowel_char(prev) && is_vowel_char(next)) {
                emit_str(dst, dst_end, "z");
            } else if (!is_vowel_char(next) && i + 1 < n &&
                       (char_eq(next, "b") || char_eq(next, "d") || char_eq(next, "g") ||
                        char_eq(next, "v") || char_eq(next, "m") || char_eq(next, "n") ||
                        char_eq(next, "l") || char_eq(next, "r") || char_eq(next, "z"))) {
                emit_str(dst, dst_end, "z");
            } else {
                emit_str(dst, dst_end, "s");
            }
            i++; continue;
        }

        // Z em final de palavra ou antes de surdas vira 's' (luz, feliz, arroz)
        if (char_eq(c, "z")) {
            if (i == n - 1 || (!is_vowel_char(next) && (char_eq(next, "p") || char_eq(next, "t") ||
                char_eq(next, "c") || char_eq(next, "f") || char_eq(next, "s") || char_eq(next, "k")))) {
                emit_str(dst, dst_end, "s");
                i++; continue;
            }
        }

        // T e D (palatalização diante de i/í, e final átono e epêntese diante de consoante)
        if (char_eq(c, "t")) {
            bool is_palatal_e = (char_eq(next, "e") && (int)i + 1 != tonic_char_pos) &&
                                ((i + 1 == n - 1) || (i + 2 == n - 1 && char_eq(next2, "s")));
            if (char_eq(next, "i") || char_eq(next, "í") || is_palatal_e) {
                emit_str(dst, dst_end, "ʧ");
                if (is_palatal_e) {
                    emit_str(dst, dst_end, "y");
                    if (char_eq(next2, "s")) {
                        emit_str(dst, dst_end, "s");
                        i += 3;
                    } else {
                        i += 2;
                    }
                    continue;
                }
            } else if (!is_vowel_char(next) && !char_eq(next, "r") && !char_eq(next, "l") && !char_eq(next, "h") && i + 1 < n) {
                emit_str(dst, dst_end, "ʧi"); // Epêntese paulista de T mudo (ritmo, etnia, atmosfera)
            } else {
                emit_str(dst, dst_end, "t");
            }
            i++; continue;
        }
        if (char_eq(c, "d")) {
            bool is_palatal_e = (char_eq(next, "e") && (int)i + 1 != tonic_char_pos) &&
                                ((i + 1 == n - 1) || (i + 2 == n - 1 && char_eq(next2, "s")));
            if (char_eq(next, "i") || char_eq(next, "í") || is_palatal_e) {
                emit_str(dst, dst_end, "ʤ");
                if (is_palatal_e) {
                    emit_str(dst, dst_end, "y");
                    if (char_eq(next2, "s")) {
                        emit_str(dst, dst_end, "s");
                        i += 3;
                    } else {
                        i += 2;
                    }
                    continue;
                }
            } else if (!is_vowel_char(next) && !char_eq(next, "r") && !char_eq(next, "l") && i + 1 < n) {
                emit_str(dst, dst_end, "ʤi"); // Epêntese paulista de D mudo (advogado, admirar, adquirir)
            } else {
                emit_str(dst, dst_end, "d");
            }
            i++; continue;
        }

        // P, B, F (com epêntese diante de consoante que não seja líquida r/l)
        if (char_eq(c, "p")) {
            if (!is_vowel_char(next) && !char_eq(next, "r") && !char_eq(next, "l") && !char_eq(next, "h") && i + 1 < n) {
                emit_str(dst, dst_end, "pi"); // apto, captar, opção, pneu, psicologia
            } else {
                emit_str(dst, dst_end, "p");
            }
            i++; continue;
        }
        if (char_eq(c, "b")) {
            if (!is_vowel_char(next) && !char_eq(next, "r") && !char_eq(next, "l") && i + 1 < n) {
                emit_str(dst, dst_end, "bi"); // objeto, obter, submarino
            } else {
                emit_str(dst, dst_end, "b");
            }
            i++; continue;
        }
        if (char_eq(c, "f")) {
            if (!is_vowel_char(next) && !char_eq(next, "r") && !char_eq(next, "l") && i + 1 < n) {
                emit_str(dst, dst_end, "fi"); // afta
            } else {
                emit_str(dst, dst_end, "f");
            }
            i++; continue;
        }

        // L em coda silábica (vocalização brasileira para ʊ)
        if (char_eq(c, "l") && (i == n - 1 || (!is_vowel_char(next) && !char_eq(next, "h")))) {
            emit_str(dst, dst_end, "ʊ"); i++; continue;
        }

        // Ditongos nasais gráficos (ão, ãe, õe)
        if (char_eq(c, "ã") && char_eq(next, "o")) { emit_str(dst, dst_end, "ɐ̃ʊ̃"); i += 2; continue; }
        if (char_eq(c, "ã") && char_eq(next, "e")) {
            if (char_eq(next2, "s") && i + 2 == n - 1) {
                emit_str(dst, dst_end, "ɐ̃js"); i += 3; continue;
            } else {
                emit_str(dst, dst_end, "ɐ̃j"); i += 2; continue;
            }
        }
        if (char_eq(c, "õ") && char_eq(next, "e") && char_eq(next2, "s") && i + 2 == n - 1) {
            emit_str(dst, dst_end, "õjs"); i += 3; continue;
        }
        if (char_eq(c, "õ") && char_eq(next, "e")) { emit_str(dst, dst_end, "õj"); i += 2; continue; }
        if (char_eq(c, "ã")) { emit_str(dst, dst_end, "ɐ̃"); i++; continue; }
        if (char_eq(c, "õ")) { emit_str(dst, dst_end, "õ"); i++; continue; }

        // Ditongos orais decrescentes
        if (char_eq(c, "e") && char_eq(next, "i")) { emit_str(dst, dst_end, "A"); i += 2; continue; }
        if (char_eq(c, "a") && char_eq(next, "i")) { emit_str(dst, dst_end, "I"); i += 2; continue; }
        if (char_eq(c, "a") && char_eq(next, "u")) { emit_str(dst, dst_end, "W"); i += 2; continue; }
        if (char_eq(c, "o") && char_eq(next, "i")) { emit_str(dst, dst_end, "Y"); i += 2; continue; }
        if (char_eq(c, "o") && char_eq(next, "u")) { emit_str(dst, dst_end, "ow"); i += 2; continue; }

        // Nasalização diante de M/N em coda
        if (is_vowel_char(c) && (char_eq(next, "m") || char_eq(next, "n")) && !char_eq(next2, "h") &&
            (i + 2 == n || !is_vowel_char(next2))) {
            // Desinência verbal átona final -am -> ditongo nasal ɐ̃ʊ̃ (falaram, cantam, viajam)
            if ((char_eq(c, "a") || char_eq(c, "á") || char_eq(c, "â") || char_eq(c, "à")) &&
                char_eq(next, "m") && i + 2 == n && (int)i != tonic_char_pos) {
                emit_str(dst, dst_end, "ɐ̃ʊ̃"); i += 2; continue;
            }
            // Terminação final -em / -ens -> ditongo nasal ẽj / ẽjs (homem, comem, jovem, viagens)
            if ((char_eq(c, "e") || char_eq(c, "é") || char_eq(c, "ê")) && char_eq(next, "m") && i + 2 == n) {
                emit_str(dst, dst_end, "ẽj"); i += 2; continue;
            }
            if ((char_eq(c, "e") || char_eq(c, "é") || char_eq(c, "ê")) && char_eq(next, "n") &&
                char_eq(next2, "s") && i + 3 == n) {
                emit_str(dst, dst_end, "ẽjs"); i += 3; continue;
            }

            if (char_eq(c, "a") || char_eq(c, "á") || char_eq(c, "â") || char_eq(c, "à")) emit_str(dst, dst_end, "ɐ̃ŋ");
            else if (char_eq(c, "e") || char_eq(c, "é") || char_eq(c, "ê")) emit_str(dst, dst_end, "Aŋ");
            else if (char_eq(c, "i") || char_eq(c, "í")) emit_str(dst, dst_end, "iŋ");
            else if (char_eq(c, "o") || char_eq(c, "ó") || char_eq(c, "ô")) emit_str(dst, dst_end, "oŋ");
            else if (char_eq(c, "u") || char_eq(c, "ú")) emit_str(dst, dst_end, "ũŋ");
            i += 2; continue;
        }

        // Vogais orais, reduções átonas e aberturas morfológicas
        if (char_eq(c, "a") || char_eq(c, "á") || char_eq(c, "à")) {
            bool ends_with_mente = (w_len >= 5 && strcmp(lower_word + w_len - 5, "mente") == 0);
            bool is_pre_mente_a = ends_with_mente && (i + 5 == n);
            bool is_atona_final = ((i == n - 1) || (i + 1 == n - 1 && char_eq(next, "s")) || is_pre_mente_a) &&
                                  ((int)i != tonic_char_pos);
            if (char_eq(c, "a") && is_atona_final) {
                emit_str(dst, dst_end, "æ");
            } else {
                emit_str(dst, dst_end, "a");
            }
        }
        else if (char_eq(c, "â")) emit_str(dst, dst_end, "ɐ");
        else if (char_eq(c, "é")) emit_str(dst, dst_end, "ɛ");
        else if (char_eq(c, "ê")) emit_str(dst, dst_end, "e");
        else if (char_eq(c, "e")) {
            if (((i == n - 1) || (i + 1 == n - 1 && char_eq(next, "s"))) && (int)i != tonic_char_pos) {
                emit_str(dst, dst_end, "y");
            } else if ((int)i == tonic_char_pos && (
                (w_len >= 4 && strcmp(lower_word + w_len - 4, "esta") == 0) ||
                (w_len >= 4 && strcmp(lower_word + w_len - 4, "este") == 0) ||
                (w_len >= 4 && strcmp(lower_word + w_len - 4, "esto") == 0) ||
                (w_len >= 4 && strcmp(lower_word + w_len - 4, "erra") == 0) ||
                (w_len >= 5 && strcmp(lower_word + w_len - 5, "estas") == 0) ||
                (w_len >= 5 && strcmp(lower_word + w_len - 5, "estes") == 0) ||
                (w_len >= 5 && strcmp(lower_word + w_len - 5, "estos") == 0) ||
                (w_len >= 5 && strcmp(lower_word + w_len - 5, "erras") == 0) ||
                strcmp(lower_word, "mulher") == 0 || strcmp(lower_word, "certo") == 0 ||
                strcmp(lower_word, "certa") == 0 || strcmp(lower_word, "certos") == 0 ||
                strcmp(lower_word, "certas") == 0 || strcmp(lower_word, "zero") == 0)) {
                emit_str(dst, dst_end, "ɛ"); // Abertura vocálica morfológica
            } else {
                emit_str(dst, dst_end, "e");
            }
        }
        else if (char_eq(c, "i") || char_eq(c, "í")) emit_str(dst, dst_end, "i");
        else if (char_eq(c, "ó")) emit_str(dst, dst_end, "ɔ");
        else if (char_eq(c, "ô")) emit_str(dst, dst_end, "o");
        else if (char_eq(c, "o")) {
            if (((i == n - 1) || (i + 1 == n - 1 && char_eq(next, "s"))) && (int)i != tonic_char_pos) {
                emit_str(dst, dst_end, "ʊ");
            } else if ((int)i == tonic_char_pos && (
                (w_len >= 4 && strcmp(lower_word + w_len - 4, "orta") == 0) ||
                (w_len >= 4 && strcmp(lower_word + w_len - 4, "orte") == 0) ||
                (w_len >= 5 && strcmp(lower_word + w_len - 5, "ortas") == 0) ||
                (w_len >= 5 && strcmp(lower_word + w_len - 5, "ortes") == 0) ||
                strcmp(lower_word, "sol") == 0 || strcmp(lower_word, "hora") == 0 ||
                strcmp(lower_word, "horas") == 0 || strcmp(lower_word, "agora") == 0 ||
                strcmp(lower_word, "bola") == 0 || strcmp(lower_word, "bolas") == 0 ||
                strcmp(lower_word, "mola") == 0 || strcmp(lower_word, "molas") == 0 ||
                strcmp(lower_word, "escola") == 0 || strcmp(lower_word, "escolas") == 0 ||
                strcmp(lower_word, "nova") == 0 || strcmp(lower_word, "novas") == 0 ||
                strcmp(lower_word, "novo") == 0 || strcmp(lower_word, "novos") == 0 ||
                strcmp(lower_word, "nove") == 0 || strcmp(lower_word, "forte") == 0 ||
                strcmp(lower_word, "fortes") == 0 || strcmp(lower_word, "morte") == 0 ||
                strcmp(lower_word, "mortes") == 0 || strcmp(lower_word, "porta") == 0 ||
                strcmp(lower_word, "portas") == 0)) {
                emit_str(dst, dst_end, "ɔ"); // Abertura vocálica morfológica
            } else {
                emit_str(dst, dst_end, "o");
            }
        }
        else if (char_eq(c, "u") || char_eq(c, "ú")) emit_str(dst, dst_end, "u");
        else {
            // Consoante direta UTF-8
            for (uint8_t b = 0; b < c.len && *dst < dst_end - 1; ++b) {
                *(*dst)++ = c.p[b];
            }
            **dst = '\0';
        }
        i++;
    }
}

/**
 * @brief Fonetiza uma palavra, com suporte especial a compostos hifenizados.
 * Divide subpalavras unidas por hífen (ex: além-mar -> além + - + mar) para que cada
 * subpalavra receba sua correta estrutura fonológica e acentuação independente.
 */
static inline void rule_based_ptbr_word_buf(const char* word_str, size_t word_len, char** dst, const char* dst_end) {
    if (word_len == 0) return;

    // Se a palavra contém hífen interno (ex: além-mar, guarda-chuva, bem-bom)
    const char* hyphen = (const char*)memchr(word_str, '-', word_len);
    if (hyphen != NULL && hyphen > word_str && hyphen < word_str + word_len - 1) {
        size_t part1_len = (size_t)(hyphen - word_str);
        phonemize_single_word_buf(word_str, part1_len, dst, dst_end);
        if (*dst < dst_end - 1) {
            *(*dst)++ = '-';
            **dst = '\0';
        }
        size_t part2_len = word_len - part1_len - 1;
        rule_based_ptbr_word_buf(hyphen + 1, part2_len, dst, dst_end);
        return;
    }

    phonemize_single_word_buf(word_str, word_len, dst, dst_end);
}

#ifdef __cplusplus
} // extern "C"
#endif

#endif // PHONEMAS_PTBR_RULES_H
