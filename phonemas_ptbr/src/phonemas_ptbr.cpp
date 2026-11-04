#include "phonemas_ptbr.h"
#include "embedded_vocab.h"
#include "embedded_exceptions.h"

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>

// Referência a um caractere UTF-8 sem cópia ou alocação
typedef struct {
    const char* p;
    uint8_t len;
} CharRef;

// Estrutura de núcleo silábico (monotongo ou ditongo)
typedef struct {
    uint8_t start_idx;
    uint8_t len;
    bool has_accent;
} Nucleus;

// Busca binária na tabela de exceções em Flash (.rodata)
static const char* find_exception(const char* word) {
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

// Verifica se caractere ASCII é pontuação padrão
static inline bool is_punct(char c) {
    return c == '.' || c == ',' || c == '!' || c == '?' || c == ';' || c == ':' ||
           c == '(' || c == ')' || c == '"' || c == '\'' || c == '-' || c == '/' ||
           c == '[' || c == ']';
}

static inline bool char_eq(CharRef c, const char* lit) {
    size_t lit_len = strlen(lit);
    return (c.len == lit_len && memcmp(c.p, lit, lit_len) == 0);
}

static inline bool is_vowel_char(CharRef c) {
    if (c.len == 1) {
        char ch = c.p[0];
        return (ch == 'a' || ch == 'e' || ch == 'i' || ch == 'o' || ch == 'u');
    } else if (c.len == 2 && (unsigned char)c.p[0] == 0xC3) {
        unsigned char b1 = (unsigned char)c.p[1];
        return (b1 == 0xA1 || b1 == 0xA9 || b1 == 0xAD || b1 == 0xB3 || b1 == 0xBA || // á é í ó ú
                b1 == 0xA2 || b1 == 0xAA || b1 == 0xB4 ||                             // â ê ô
                b1 == 0xA3 || b1 == 0xB5 ||                                           // ã õ
                b1 == 0xA0);                                                          // à
    }
    return false;
}

static inline bool has_graphic_accent(CharRef c) {
    if (c.len == 2 && (unsigned char)c.p[0] == 0xC3) {
        unsigned char b1 = (unsigned char)c.p[1];
        return (b1 == 0xA1 || b1 == 0xA9 || b1 == 0xAD || b1 == 0xB3 || b1 == 0xBA || // á é í ó ú
                b1 == 0xA2 || b1 == 0xAA || b1 == 0xB4 ||                             // â ê ô
                b1 == 0xA3 || b1 == 0xB5 ||                                           // ã õ
                b1 == 0xA0);                                                          // à
    }
    return false;
}

static inline bool is_front_vowel(CharRef c) {
    if (c.len == 1) {
        return (c.p[0] == 'e' || c.p[0] == 'i');
    } else if (c.len == 2 && (unsigned char)c.p[0] == 0xC3) {
        unsigned char b1 = (unsigned char)c.p[1];
        return (b1 == 0xA9 || b1 == 0xAA || b1 == 0xAD || b1 == 0xA8); // é ê í è
    }
    return false;
}

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
        if (b1 == 0xA3 && (c2.p[0] == 'o' || c2.p[0] == 'e')) return true; // ão, ãe
        if (b1 == 0xB5 && c2.p[0] == 'e') return true; // õe
    }
    return false;
}

// Emissão direta no buffer de saída sem alocação
static inline void emit_str(char** dst, const char* end, const char* str) {
    while (*str && *dst < end - 1) {
        *(*dst)++ = *str++;
    }
    **dst = '\0';
}

static inline bool prefix_match(const CharRef* chars, size_t n, const char* pfx) {
    size_t k = 0;
    for (const char* p = pfx; *p; ++p, ++k) {
        if (k >= n || chars[k].len != 1 || chars[k].p[0] != *p) return false;
    }
    return true;
}

// Classificador contextual de X usando ponteiros diretos (Zero Alocações)
static const char* classify_x_ptbr(const CharRef* chars, size_t n, size_t i) {
    if (!char_eq(chars[i], "x")) return NULL;

    CharRef prev = (i > 0) ? chars[i - 1] : (CharRef){"", 0};
    CharRef next = (i + 1 < n) ? chars[i + 1] : (CharRef){"", 0};
    CharRef next2 = (i + 2 < n) ? chars[i + 2] : (CharRef){"", 0};

    // Sequência 'ex' inicial
    if (i == 1 && char_eq(chars[0], "e")) {
        if (is_vowel_char(next)) return "z";
        if (char_eq(next, "p") || char_eq(next, "t") || char_eq(next, "c") ||
            char_eq(next, "s") || char_eq(next, "f")) return "s";
        return NULL;
    }

    // Ditongo + X
    if (i >= 2) {
        CharRef preprev = chars[i - 2];
        if ((char_eq(preprev, "a") && char_eq(prev, "i")) ||
            (char_eq(preprev, "e") && char_eq(prev, "i")) ||
            (char_eq(preprev, "o") && char_eq(prev, "u"))) return "ʃ";
    }

    if (char_eq(prev, "u")) return "ʃ"; // luxo, puxar, bruxa

    if (prefix_match(chars, n, "fix") || prefix_match(chars, n, "sex") ||
        prefix_match(chars, n, "conex") || prefix_match(chars, n, "complex") ||
        prefix_match(chars, n, "flex") || prefix_match(chars, n, "reflex") ||
        prefix_match(chars, n, "paradox")) return "ks";

    if (n >= 4 && char_eq(chars[0], "t") && char_eq(chars[1], "á") &&
        char_eq(chars[2], "x") && i == 2) return "ks";

    if (prefix_match(chars, n, "mex") || prefix_match(chars, n, "vex") ||
        prefix_match(chars, n, "bex")) return "ʃ";

    return NULL;
}

// Fonetizador de palavra única (Zero Heap: executa em buffers da stack)
static void rule_based_ptbr_word_buf(const char* word_str, size_t word_len, char** dst, const char* dst_end) {
    if (word_len == 0) return;

    // Buffer local para lowercase (máximo 64 bytes para uma única palavra em pt-BR)
    char lower_word[64];
    size_t w_len = word_len < sizeof(lower_word) - 1 ? word_len : sizeof(lower_word) - 1;
    for (size_t k = 0; k < w_len; ++k) {
        lower_word[k] = (char)tolower((unsigned char)word_str[k]);
    }
    lower_word[w_len] = '\0';

    // 1. Busca binária na tabela de exceções em Flash (.rodata)
    const char* exc = find_exception(lower_word);
    if (exc != NULL) {
        emit_str(dst, dst_end, exc);
        return;
    }

    // 2. Extração de caracteres UTF-8 em array local na stack
    CharRef chars[64];
    size_t n = 0;
    for (size_t i = 0; i < w_len && n < 64; ) {
        unsigned char c = (unsigned char)lower_word[i];
        uint8_t len = 1;
        if ((c & 0xE0) == 0xC0) len = 2;
        else if ((c & 0xF0) == 0xE0) len = 3;
        else if ((c & 0xF8) == 0xF0) len = 4;
        if (i + len > w_len) len = (uint8_t)(w_len - i);

        chars[n].p = lower_word + i;
        chars[n].len = len;
        n++;
        i += len;
    }
    if (n == 0) return;

    // 3. Silabificação e identificação dos núcleos vocálicos (ditongos e monotongos)
    Nucleus nuclei[32];
    size_t num_nuclei = 0;
    for (size_t i = 0; i < n && num_nuclei < 32; ) {
        if (is_vowel_char(chars[i])) {
            bool acc = has_graphic_accent(chars[i]);
            if (i + 1 < n && is_diphthong(chars[i], chars[i + 1])) {
                nuclei[num_nuclei].start_idx = (uint8_t)i;
                nuclei[num_nuclei].len = 2;
                nuclei[num_nuclei].has_accent = (acc || has_graphic_accent(chars[i + 1]));
                num_nuclei++;
                i += 2;
            } else {
                nuclei[num_nuclei].start_idx = (uint8_t)i;
                nuclei[num_nuclei].len = 1;
                nuclei[num_nuclei].has_accent = acc;
                num_nuclei++;
                i += 1;
            }
        } else {
            i += 1;
        }
    }

    int tonic_nucleus_idx = -1;
    for (size_t idx = 0; idx < num_nuclei; ++idx) {
        if (nuclei[idx].has_accent) {
            tonic_nucleus_idx = (int)idx;
            break;
        }
    }

    if (tonic_nucleus_idx == -1 && num_nuclei > 0) {
        bool ends_with_mente = (w_len >= 5 && strcmp(lower_word + w_len - 5, "mente") == 0);
        if (ends_with_mente && num_nuclei >= 2) {
            tonic_nucleus_idx = (int)num_nuclei - 2;
        } else {
            CharRef last = chars[n - 1];
            bool oxytone = (char_eq(last, "r") || char_eq(last, "l") || char_eq(last, "z") ||
                            char_eq(last, "x") || char_eq(last, "i") || char_eq(last, "u"));

            if (n >= 2) {
                if (char_eq(last, "s")) {
                    CharRef penult = chars[n - 2];
                    if (char_eq(penult, "i") || char_eq(penult, "u")) oxytone = true;
                } else if (char_eq(last, "m")) {
                    CharRef penult = chars[n - 2];
                    if (char_eq(penult, "i") || char_eq(penult, "u")) oxytone = true;
                }
            }

            if (oxytone) {
                tonic_nucleus_idx = (int)num_nuclei - 1;
            } else {
                tonic_nucleus_idx = num_nuclei >= 2 ? (int)num_nuclei - 2 : 0;
            }
        }
    }

    int sec_nucleus_idx = -1;
    if (num_nuclei >= 3 && tonic_nucleus_idx > 0) {
        sec_nucleus_idx = 0;
    }

    int tonic_char_pos = (tonic_nucleus_idx >= 0 && tonic_nucleus_idx < (int)num_nuclei)
                         ? (int)nuclei[tonic_nucleus_idx].start_idx : -1;
    int sec_char_pos = (sec_nucleus_idx >= 0 && sec_nucleus_idx < (int)num_nuclei)
                       ? (int)nuclei[sec_nucleus_idx].start_idx : -1;

    // 4. Aplicação das transformações fonéticas
    for (size_t i = 0; i < n && *dst < dst_end - 1; ) {
        CharRef c = chars[i];
        CharRef next = (i + 1 < n) ? chars[i + 1] : (CharRef){"", 0};
        CharRef next2 = (i + 2 < n) ? chars[i + 2] : (CharRef){"", 0};
        CharRef prev = (i > 0) ? chars[i - 1] : (CharRef){"", 0};

        if ((int)i == tonic_char_pos) {
            emit_str(dst, dst_end, "ˈ");
        } else if ((int)i == sec_char_pos) {
            emit_str(dst, dst_end, "ˌ");
        }

        // X contextual
        if (char_eq(c, "x")) {
            const char* x_ph = classify_x_ptbr(chars, n, i);
            if (x_ph != NULL) {
                emit_str(dst, dst_end, x_ph);
                if (i == 1 && char_eq(next, "c") && is_front_vowel(next2)) i += 2;
                else i++;
                continue;
            }
        }

        // Dígrafos
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
        if (char_eq(c, "g") && char_eq(next, "u") && is_front_vowel(next2)) {
            emit_str(dst, dst_end, "ɡ"); i += 2; continue;
        }

        // C e Ç
        if (char_eq(c, "c")) {
            if (is_front_vowel(next)) emit_str(dst, dst_end, "s");
            else emit_str(dst, dst_end, "k");
            i++; continue;
        }
        if (char_eq(c, "ç")) { emit_str(dst, dst_end, "s"); i++; continue; }

        // G e J
        if (char_eq(c, "g")) {
            if (is_front_vowel(next)) emit_str(dst, dst_end, "ʒ");
            else emit_str(dst, dst_end, "ɡ");
            i++; continue;
        }
        if (char_eq(c, "j")) { emit_str(dst, dst_end, "ʒ"); i++; continue; }

        // H mudo
        if (char_eq(c, "h") && i == 0) { i++; continue; }

        // R
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

        // S
        if (char_eq(c, "s")) {
            if (is_vowel_char(prev) && is_vowel_char(next)) {
                emit_str(dst, dst_end, "z");
            } else {
                emit_str(dst, dst_end, "s");
            }
            i++; continue;
        }

        // T e D (palatalização brasileira diante de i/í ou e/es átono final)
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
            } else {
                emit_str(dst, dst_end, "d");
            }
            i++; continue;
        }

        // L em coda
        if (char_eq(c, "l") && (i == n - 1 || (!is_vowel_char(next) && !char_eq(next, "h")))) {
            emit_str(dst, dst_end, "ʊ"); i++; continue;
        }

        // Ditongos nasais
        if (char_eq(c, "ã") && char_eq(next, "o")) { emit_str(dst, dst_end, "ɐ̃ʊ̃"); i += 2; continue; }
        if (char_eq(c, "õ") && char_eq(next, "e") && char_eq(next2, "s") && i + 2 == n - 1) {
            emit_str(dst, dst_end, "õjs"); i += 3; continue;
        }
        if (char_eq(c, "õ") && char_eq(next, "e")) { emit_str(dst, dst_end, "õj"); i += 2; continue; }
        if (char_eq(c, "ã")) { emit_str(dst, dst_end, "ɐ̃"); i++; continue; }
        if (char_eq(c, "õ")) { emit_str(dst, dst_end, "õ"); i++; continue; }

        // Ditongos orais
        if (char_eq(c, "e") && char_eq(next, "i")) { emit_str(dst, dst_end, "A"); i += 2; continue; }
        if (char_eq(c, "a") && char_eq(next, "i")) { emit_str(dst, dst_end, "I"); i += 2; continue; }
        if (char_eq(c, "a") && char_eq(next, "u")) { emit_str(dst, dst_end, "W"); i += 2; continue; }
        if (char_eq(c, "o") && char_eq(next, "i")) { emit_str(dst, dst_end, "Y"); i += 2; continue; }
        if (char_eq(c, "o") && char_eq(next, "u")) { emit_str(dst, dst_end, "ow"); i += 2; continue; }

        // Nasalização diante de M/N em coda
        if (is_vowel_char(c) && (char_eq(next, "m") || char_eq(next, "n")) && !char_eq(next2, "h") &&
            (i + 2 == n || !is_vowel_char(next2))) {
            if (char_eq(c, "a") || char_eq(c, "á")) emit_str(dst, dst_end, "ɐ̃ŋ");
            else if (char_eq(c, "e") || char_eq(c, "é") || char_eq(c, "ê")) emit_str(dst, dst_end, "Aŋ");
            else if (char_eq(c, "i") || char_eq(c, "í")) emit_str(dst, dst_end, "iŋ");
            else if (char_eq(c, "o") || char_eq(c, "ó") || char_eq(c, "ô")) emit_str(dst, dst_end, "oŋ");
            else if (char_eq(c, "u") || char_eq(c, "ú")) emit_str(dst, dst_end, "ũŋ");
            i += 2; continue;
        }

        // Vogais orais e reduções
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

extern "C" {

size_t phonemas_ptbr_translate_buffer(const char* text, char* out_buf, size_t max_len) {
    if (!text || !out_buf || max_len == 0) return 0;

    char* dst = out_buf;
    const char* dst_end = out_buf + max_len;
    *dst = '\0';

    const char* p = text;
    bool first_token = true;

    while (*p) {
        // Pula espaços em branco
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;

        const char* token_start = p;
        while (*p && !isspace((unsigned char)*p)) p++;
        const char* token_end = p;

        // Extrai pontuação à esquerda
        const char* core_start = token_start;
        while (core_start < token_end && is_punct(*core_start)) {
            core_start++;
        }

        // Extrai pontuação à direita
        const char* core_end = token_end;
        while (core_end > core_start && is_punct(*(core_end - 1))) {
            core_end--;
        }

        if (!first_token && dst < dst_end - 1) {
            *dst++ = ' ';
            *dst = '\0';
        }
        first_token = false;

        // Emite pontuação à esquerda
        while (token_start < core_start && dst < dst_end - 1) {
            *dst++ = *token_start++;
            *dst = '\0';
        }

        // Emite a palavra fonetizada
        size_t core_len = (size_t)(core_end - core_start);
        if (core_len > 0) {
            rule_based_ptbr_word_buf(core_start, core_len, &dst, dst_end);
        }

        // Emite pontuação à direita
        while (core_end < token_end && dst < dst_end - 1) {
            *dst++ = *core_end++;
            *dst = '\0';
        }
    }

    *dst = '\0';
    return (size_t)(dst - out_buf);
}

char* phonemas_ptbr_translate(const char* text) {
    if (!text) return NULL;
    size_t needed = strlen(text) * 4 + 64;
    char* buf = (char*)malloc(needed);
    if (!buf) return NULL;
    phonemas_ptbr_translate_buffer(text, buf, needed);
    return buf;
}

void phonemas_ptbr_free(char* ptr) {
    if (ptr) free(ptr);
}

int phonemas_ptbr_phonemes_to_tokens(const char* phonemes, int64_t* out_tokens, size_t max_tokens) {
    if (!phonemes || !out_tokens || max_tokens < 3) return -1;

    size_t count = 0;
    out_tokens[count++] = 0; // Pad inicial [0]

    size_t ph_len = strlen(phonemes);
    for (size_t i = 0; i < ph_len && count < max_tokens - 1; ) {
        bool matched = false;
        for (size_t v = 0; v < EMBEDDED_VOCAB_SIZE; ++v) {
            const char* sym = EMBEDDED_VOCAB[v].symbol;
            size_t sym_len = strlen(sym);
            if (i + sym_len <= ph_len && memcmp(phonemes + i, sym, sym_len) == 0) {
                out_tokens[count++] = EMBEDDED_VOCAB[v].id;
                i += sym_len;
                matched = true;
                break;
            }
        }
        if (!matched) {
            i++;
        }
    }

    out_tokens[count++] = 0; // Pad final [0]
    return (int)count;
}

int phonemas_ptbr_text_to_tokens(const char* text, int64_t* out_tokens, size_t max_tokens) {
    if (!text || !out_tokens || max_tokens < 3) return -1;

    // Buffer local na stack para os fonemas intermediários (zero heap)
    char ph_buf[512];
    phonemas_ptbr_translate_buffer(text, ph_buf, sizeof(ph_buf));
    return phonemas_ptbr_phonemes_to_tokens(ph_buf, out_tokens, max_tokens);
}

} // extern "C"
