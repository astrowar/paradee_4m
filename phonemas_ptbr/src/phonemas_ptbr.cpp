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
        char ch2 = c2.p[0];
        if (b1 == 0xA1 && (ch2 == 'i' || ch2 == 'u')) return true; // ái, áu
        if (b1 == 0xA9 && (ch2 == 'i' || ch2 == 'u')) return true; // éi, éu
        if (b1 == 0xAA && (ch2 == 'i' || ch2 == 'u')) return true; // êi, êu
        if (b1 == 0xB3 && (ch2 == 'i' || ch2 == 'u')) return true; // ói, óu
        if (b1 == 0xA3 && (ch2 == 'o' || ch2 == 'e')) return true; // ão, ãe
        if (b1 == 0xB5 && ch2 == 'e') return true; // õe
    }
    return false;
}

// Verifica se duas consoantes consecutivas formam encontro consonantal inseparável
static inline bool is_inseparable_cluster(CharRef c1, CharRef c2) {
    if (c1.len == 1 && c2.len == 1) {
        char a = c1.p[0], b = c2.p[0];
        // Dígrafos inseparáveis: ch, lh, nh, qu, gu
        if (b == 'h' && (a == 'c' || a == 'l' || a == 'n')) return true;
        if (b == 'u' && (a == 'q' || a == 'g')) return true;
        // Consoante + líquida (r ou l)
        if (b == 'r' && (a == 'p' || a == 'b' || a == 't' || a == 'd' ||
                         a == 'c' || a == 'g' || a == 'f' || a == 'v')) return true;
        if (b == 'l' && (a == 'p' || a == 'b' || a == 't' || a == 'c' ||
                         a == 'g' || a == 'f' || a == 'v')) return true;
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
static const char* classify_x_ptbr(const char* word, const CharRef* chars, size_t n, size_t i) {
    if (!char_eq(chars[i], "x")) return NULL;

    CharRef prev = (i > 0) ? chars[i - 1] : (CharRef){"", 0};
    CharRef next = (i + 1 < n) ? chars[i + 1] : (CharRef){"", 0};

    // 1. X inicial: SEMPRE som de /ʃ/ (xícara, xadrez, xarope, xampu, xale, xaveco, xerife)
    if (i == 0) return "ʃ";

    // 2. Prefixo en- + X: SEMPRE som de /ʃ/ (enxame, enxada, enxugar, enxofre, enxaqueca)
    if (i >= 2 && char_eq(chars[i - 2], "e") && char_eq(chars[i - 1], "n")) {
        return "ʃ";
    }

    // 3. Sequência 'ex' inicial
    if (i == 1 && char_eq(chars[0], "e")) {
        if (is_vowel_char(next)) return "z"; // exame, exemplo, existir, exército, exato
        if (char_eq(next, "p") || char_eq(next, "t") || char_eq(next, "c") ||
            char_eq(next, "s") || char_eq(next, "f")) return "s"; // explicar, extra, excelente
        return "s";
    }

    // 4. Palavras comuns com som de /s/ (aproximar, próximo, máximo, auxílio, trouxe)
    if (strncmp(word, "próxim", strlen("próxim")) == 0 || strncmp(word, "proxim", 6) == 0 ||
        strncmp(word, "aproxim", 7) == 0 || strncmp(word, "máxim", strlen("máxim")) == 0 ||
        strncmp(word, "maxim", 5) == 0 || strncmp(word, "auxili", 6) == 0 ||
        strncmp(word, "auxíli", strlen("auxíli")) == 0 || strncmp(word, "troux", 5) == 0) {
        return "s";
    }

    // 5. Palavras com som de /ks/ (raízes cultas greco-latinas)
    if (strncmp(word, "fix", 3) == 0 || strncmp(word, "sex", 3) == 0 ||
        strncmp(word, "conex", 5) == 0 || strncmp(word, "complex", 7) == 0 ||
        strncmp(word, "flex", 4) == 0 || strncmp(word, "reflex", 6) == 0 ||
        strncmp(word, "paradox", 7) == 0 || strncmp(word, "táxi", strlen("táxi")) == 0 ||
        strncmp(word, "taxi", 4) == 0 || strncmp(word, "tóx", strlen("tóx")) == 0 ||
        strncmp(word, "tox", 3) == 0 || strncmp(word, "léx", strlen("léx")) == 0 ||
        strncmp(word, "lex", 3) == 0 || strncmp(word, "oxig", 4) == 0 ||
        strncmp(word, "asfix", 5) == 0 || strncmp(word, "anex", 4) == 0 ||
        strncmp(word, "nex", 3) == 0 || strncmp(word, "maxil", 5) == 0 ||
        strncmp(word, "prolix", 6) == 0 || strncmp(word, "ortodox", 7) == 0 ||
        strncmp(word, "heterodox", 9) == 0) {
        return "ks";
    }

    // 6. Ditongo + X: som de /ʃ/ (caixa, peixe, frouxo, baixo, feixe)
    if (i >= 2) {
        CharRef preprev = chars[i - 2];
        if ((char_eq(preprev, "a") && char_eq(prev, "i")) ||
            (char_eq(preprev, "e") && char_eq(prev, "i")) ||
            (char_eq(preprev, "o") && char_eq(prev, "u")) ||
            (char_eq(preprev, "a") && char_eq(prev, "u"))) return "ʃ";
    }

    // 7. Após 'u' ou 'i': som de /ʃ/ (luxo, puxar, bruxa, lixo)
    if (char_eq(prev, "u") || char_eq(prev, "i")) return "ʃ";

    // 8. X em final de palavra: SEMPRE /ks/ em pt-BR (tórax, clímax, duplex, látex, fênix, córtex)
    if (i == n - 1) return "ks";

    // 9. X antes de consoante interna: som de /s/ (texto, sexta, contexto)
    if (!is_vowel_char(next)) return "s";

    // 10. Fallback padrão para X em português: som de /ʃ/ (abacaxi, faxina, graxa, taxa, mexer)
    return "ʃ";
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
        // Se 'u' segue 'q' ou 'g' antes de qualquer vogal, não é núcleo silábico independente
        // (é dígrafo mudo diante de e/i ou glide [w] diante de a/o)
        if (char_eq(chars[i], "u") && i > 0 && (char_eq(chars[i - 1], "q") || char_eq(chars[i - 1], "g")) &&
            i + 1 < n && is_vowel_char(chars[i + 1])) {
            i += 1;
            continue;
        }

        if (is_vowel_char(chars[i])) {
            bool acc = has_graphic_accent(chars[i]);
            if (i + 1 < n && is_vowel_char(chars[i + 1])) {
                CharRef v1 = chars[i];
                CharRef v2 = chars[i + 1];

                // Hiato se duas vogais iguais: aa, ee, oo, ii, uu
                bool is_hiatus_equal = (v1.len == v2.len && memcmp(v1.p, v2.p, v1.len) == 0);
                // Hiato se ambas são abertas/médias: a, e, o com a, e, o
                bool v1_open = (char_eq(v1, "a") || char_eq(v1, "e") || char_eq(v1, "o") ||
                                char_eq(v1, "á") || char_eq(v1, "é") || char_eq(v1, "ó") ||
                                char_eq(v1, "â") || char_eq(v1, "ê") || char_eq(v1, "ô"));
                bool v2_open = (char_eq(v2, "a") || char_eq(v2, "e") || char_eq(v2, "o") ||
                                char_eq(v2, "á") || char_eq(v2, "é") || char_eq(v2, "ó") ||
                                char_eq(v2, "â") || char_eq(v2, "ê") || char_eq(v2, "ô"));
                bool is_hiatus_open = (v1_open && v2_open);
                // Hiato se v2 possui acento gráfico: saúde, país, baú, ruído
                bool is_hiatus_accent = has_graphic_accent(v2);
                // Hiato se v2 é i diante de nh: rainha, moinho
                bool is_hiatus_nh = ((char_eq(v2, "i") || char_eq(v2, "í")) &&
                                     i + 3 < n && char_eq(chars[i + 2], "n") && char_eq(chars[i + 3], "h"));
                // Hiato se verbo em -ir (ex: construir, afluir, retrair, sair)
                bool is_hiatus_ir = ((char_eq(v2, "i") || char_eq(v2, "í")) &&
                                     i + 2 == n - 1 && char_eq(chars[i + 2], "r"));

                bool is_diph = is_diphthong(v1, v2);

                if (is_diph && !(is_hiatus_equal || is_hiatus_open || is_hiatus_accent || is_hiatus_nh || is_hiatus_ir)) {
                    nuclei[num_nuclei].start_idx = (uint8_t)i;
                    nuclei[num_nuclei].len = 2;
                    nuclei[num_nuclei].has_accent = (acc || has_graphic_accent(chars[i + 1]));
                    num_nuclei++;
                    i += 2;
                    continue;
                }
            }
            nuclei[num_nuclei].start_idx = (uint8_t)i;
            nuclei[num_nuclei].len = 1;
            nuclei[num_nuclei].has_accent = acc;
            num_nuclei++;
            i += 1;
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
            const char* x_ph = classify_x_ptbr(lower_word, chars, n, i);
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
        if (char_eq(c, "g") && char_eq(next, "u")) {
            if (is_front_vowel(next2)) { emit_str(dst, dst_end, "ɡ"); i += 2; continue; }
            else if (is_vowel_char(next2)) { emit_str(dst, dst_end, "ɡw"); i += 2; continue; }
        }

        // C e Ç
        if (char_eq(c, "c")) {
            if (is_front_vowel(next)) emit_str(dst, dst_end, "s");
            else if (!is_vowel_char(next) && !char_eq(next, "r") && !char_eq(next, "l") && !char_eq(next, "h") && i + 1 < n) {
                emit_str(dst, dst_end, "ki");
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
                emit_str(dst, dst_end, "ɡi");
            } else {
                emit_str(dst, dst_end, "ɡ");
            }
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

        // S (com vozeamento diante de consoantes sonoras em coda: b, d, g, v, m, n, l, r, z)
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

        // Z em final de palavra ou antes de consoante surda
        if (char_eq(c, "z")) {
            if (i == n - 1 || (!is_vowel_char(next) && (char_eq(next, "p") || char_eq(next, "t") ||
                char_eq(next, "c") || char_eq(next, "f") || char_eq(next, "s") || char_eq(next, "k")))) {
                emit_str(dst, dst_end, "s");
                i++; continue;
            }
        }

        // T e D (palatalização brasileira diante de i/í, e/es átono final ou epêntese diante de consoante)
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
                // Epêntese com palatalização paulista de t diante de consoante (ex: ritmo, etnia, atmosfera)
                emit_str(dst, dst_end, "ʧi");
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
                // Epêntese com palatalização paulista de d diante de consoante (ex: advogado, admirar, adquirir)
                emit_str(dst, dst_end, "ʤi");
            } else {
                emit_str(dst, dst_end, "d");
            }
            i++; continue;
        }

        // P, B, F (com epêntese diante de consoante que não seja líquida r/l)
        if (char_eq(c, "p")) {
            if (!is_vowel_char(next) && !char_eq(next, "r") && !char_eq(next, "l") && !char_eq(next, "h") && i + 1 < n) {
                emit_str(dst, dst_end, "pi");
            } else {
                emit_str(dst, dst_end, "p");
            }
            i++; continue;
        }
        if (char_eq(c, "b")) {
            if (!is_vowel_char(next) && !char_eq(next, "r") && !char_eq(next, "l") && i + 1 < n) {
                emit_str(dst, dst_end, "bi");
            } else {
                emit_str(dst, dst_end, "b");
            }
            i++; continue;
        }
        if (char_eq(c, "f")) {
            if (!is_vowel_char(next) && !char_eq(next, "r") && !char_eq(next, "l") && i + 1 < n) {
                emit_str(dst, dst_end, "fi");
            } else {
                emit_str(dst, dst_end, "f");
            }
            i++; continue;
        }

        // L em coda
        if (char_eq(c, "l") && (i == n - 1 || (!is_vowel_char(next) && !char_eq(next, "h")))) {
            emit_str(dst, dst_end, "ʊ"); i++; continue;
        }

        // Ditongos nasais
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

        // Ditongos orais
        if (char_eq(c, "e") && char_eq(next, "i")) { emit_str(dst, dst_end, "A"); i += 2; continue; }
        if (char_eq(c, "a") && char_eq(next, "i")) { emit_str(dst, dst_end, "I"); i += 2; continue; }
        if (char_eq(c, "a") && char_eq(next, "u")) { emit_str(dst, dst_end, "W"); i += 2; continue; }
        if (char_eq(c, "o") && char_eq(next, "i")) { emit_str(dst, dst_end, "Y"); i += 2; continue; }
        if (char_eq(c, "o") && char_eq(next, "u")) { emit_str(dst, dst_end, "ow"); i += 2; continue; }

        // Nasalização diante de M/N em coda
        if (is_vowel_char(c) && (char_eq(next, "m") || char_eq(next, "n")) && !char_eq(next2, "h") &&
            (i + 2 == n || !is_vowel_char(next2))) {
            // Desinência verbal átona final -am -> ditongo nasal ɐ̃ʊ̃ (ex: falaram, cantam, viajam)
            if ((char_eq(c, "a") || char_eq(c, "á") || char_eq(c, "â") || char_eq(c, "à")) &&
                char_eq(next, "m") && i + 2 == n && (int)i != tonic_char_pos) {
                emit_str(dst, dst_end, "ɐ̃ʊ̃"); i += 2; continue;
            }
            // Terminação final -em / -ens -> ditongo nasal ẽj / ẽjs (ex: homem, comem, jovem, viagens)
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
                emit_str(dst, dst_end, "ɛ");
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
                emit_str(dst, dst_end, "ɔ");
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

// Silabifica uma única palavra UTF-8 escrevendo no buffer dst com separador sep (Zero Heap)
static void syllabify_ptbr_word_buf(const char* word_str, size_t word_len, char** dst, const char* dst_end, char sep) {
    if (word_len == 0) return;

    // Buffer local para lowercase (máximo 64 bytes para uma única palavra em pt-BR)
    char lower_word[64];
    size_t w_len = word_len < sizeof(lower_word) - 1 ? word_len : sizeof(lower_word) - 1;
    for (size_t k = 0; k < w_len; ++k) {
        lower_word[k] = (char)tolower((unsigned char)word_str[k]);
    }
    lower_word[w_len] = '\0';

    // Extração de caracteres UTF-8 mantendo ponteiros para o texto original
    CharRef orig_chars[64];
    CharRef lower_chars[64];
    size_t n = 0;
    for (size_t i = 0; i < w_len && n < 64; ) {
        unsigned char c = (unsigned char)lower_word[i];
        uint8_t len = 1;
        if ((c & 0xE0) == 0xC0) len = 2;
        else if ((c & 0xF0) == 0xE0) len = 3;
        else if ((c & 0xF8) == 0xF0) len = 4;
        if (i + len > w_len) len = (uint8_t)(w_len - i);

        orig_chars[n].p = word_str + i;
        orig_chars[n].len = len;
        lower_chars[n].p = lower_word + i;
        lower_chars[n].len = len;
        n++;
        i += len;
    }
    if (n == 0) return;

    // 1. Identificar vogais (excluindo 'u' dígrafo de qu/gu antes de vogal)
    bool is_v[64];
    for (size_t i = 0; i < n; ++i) {
        if (is_vowel_char(lower_chars[i])) {
            if (char_eq(lower_chars[i], "u") && i > 0 &&
                (char_eq(lower_chars[i - 1], "q") || char_eq(lower_chars[i - 1], "g")) &&
                i + 1 < n && is_vowel_char(lower_chars[i + 1])) {
                is_v[i] = false;
            } else {
                is_v[i] = true;
            }
        } else {
            is_v[i] = false;
        }
    }

    // 2. Identificar núcleos vocálicos (ditongos vs hiatos)
    Nucleus nuclei[32];
    size_t num_nuclei = 0;
    for (size_t i = 0; i < n && num_nuclei < 32; ) {
        if (is_v[i]) {
            uint8_t start = (uint8_t)i;
            if (i + 1 < n && is_v[i + 1]) {
                CharRef v1 = lower_chars[i];
                CharRef v2 = lower_chars[i + 1];

                // Hiato se duas vogais iguais: aa, ee, oo, ii, uu
                bool is_hiatus_equal = (v1.len == v2.len && memcmp(v1.p, v2.p, v1.len) == 0);
                // Hiato se ambas são abertas/médias: a, e, o com a, e, o
                bool v1_open = (char_eq(v1, "a") || char_eq(v1, "e") || char_eq(v1, "o") ||
                                char_eq(v1, "á") || char_eq(v1, "é") || char_eq(v1, "ó") ||
                                char_eq(v1, "â") || char_eq(v1, "ê") || char_eq(v1, "ô"));
                bool v2_open = (char_eq(v2, "a") || char_eq(v2, "e") || char_eq(v2, "o") ||
                                char_eq(v2, "á") || char_eq(v2, "é") || char_eq(v2, "ó") ||
                                char_eq(v2, "â") || char_eq(v2, "ê") || char_eq(v2, "ô"));
                bool is_hiatus_open = (v1_open && v2_open);
                // Hiato se v2 possui acento gráfico: saúde, país, baú, ruído
                bool is_hiatus_accent = has_graphic_accent(v2);
                // Hiato se v2 é i diante de nh: rainha, moinho
                bool is_hiatus_nh = ((char_eq(v2, "i") || char_eq(v2, "í")) &&
                                     i + 3 < n && char_eq(lower_chars[i + 2], "n") && char_eq(lower_chars[i + 3], "h"));
                // Hiato se verbo em -ir (ex: construir, afluir, retrair, sair)
                bool is_hiatus_ir = ((char_eq(v2, "i") || char_eq(v2, "í")) &&
                                     i + 2 == n - 1 && char_eq(lower_chars[i + 2], "r"));

                bool is_diph = is_diphthong(v1, v2);

                if (is_diph && !(is_hiatus_equal || is_hiatus_open || is_hiatus_accent || is_hiatus_nh || is_hiatus_ir)) {
                    nuclei[num_nuclei].start_idx = start;
                    nuclei[num_nuclei].len = 2;
                    nuclei[num_nuclei].has_accent = (has_graphic_accent(v1) || has_graphic_accent(v2));
                    num_nuclei++;
                    i += 2;
                    continue;
                }
            }
            nuclei[num_nuclei].start_idx = start;
            nuclei[num_nuclei].len = 1;
            nuclei[num_nuclei].has_accent = has_graphic_accent(lower_chars[i]);
            num_nuclei++;
            i += 1;
        } else {
            i += 1;
        }
    }

    if (num_nuclei == 0) {
        for (size_t k = 0; k < n; ++k) {
            for (uint8_t b = 0; b < orig_chars[k].len && *dst < dst_end - 1; ++b) {
                *(*dst)++ = orig_chars[k].p[b];
            }
        }
        **dst = '\0';
        return;
    }

    // 3. Determinar cortes entre núcleos
    uint8_t cuts[32];
    size_t num_cuts = 0;
    cuts[num_cuts++] = 0;

    for (size_t k = 0; k + 1 < num_nuclei; ++k) {
        size_t n1_end = nuclei[k].start_idx + nuclei[k].len;
        size_t n2_start = nuclei[k + 1].start_idx;
        size_t num_c = n2_start - n1_end;

        if (num_c == 0) {
            cuts[num_cuts++] = (uint8_t)n2_start;
        } else if (num_c == 1) {
            cuts[num_cuts++] = (uint8_t)n1_end;
        } else if (num_c == 2) {
            CharRef c1 = lower_chars[n1_end];
            CharRef c2 = lower_chars[n1_end + 1];
            if (is_inseparable_cluster(c1, c2)) {
                cuts[num_cuts++] = (uint8_t)n1_end;
            } else {
                cuts[num_cuts++] = (uint8_t)(n1_end + 1);
            }
        } else if (num_c == 3) {
            CharRef c2 = lower_chars[n1_end + 1];
            CharRef c3 = lower_chars[n1_end + 2];
            if (is_inseparable_cluster(c2, c3)) {
                cuts[num_cuts++] = (uint8_t)(n1_end + 1);
            } else {
                cuts[num_cuts++] = (uint8_t)(n1_end + 2);
            }
        } else {
            cuts[num_cuts++] = (uint8_t)(n1_end + 2);
        }
    }
    cuts[num_cuts++] = (uint8_t)n;

    // 4. Emissão das sílabas separadas por sep
    for (size_t s = 0; s + 1 < num_cuts; ++s) {
        if (s > 0 && *dst < dst_end - 1) {
            *(*dst)++ = sep;
        }
        for (size_t idx = cuts[s]; idx < cuts[s + 1] && *dst < dst_end - 1; ++idx) {
            for (uint8_t b = 0; b < orig_chars[idx].len && *dst < dst_end - 1; ++b) {
                *(*dst)++ = orig_chars[idx].p[b];
            }
        }
    }
    **dst = '\0';
}

extern "C" {

size_t phonemas_ptbr_syllabify_buffer(const char* text, char* out_buf, size_t max_len, char sep) {
    if (!text || !out_buf || max_len == 0) return 0;

    char* dst = out_buf;
    const char* dst_end = out_buf + max_len;
    *dst = '\0';

    const char* p = text;

    while (*p) {
        // Pula e copia espaços em branco
        while (*p && isspace((unsigned char)*p)) {
            if (dst < dst_end - 1) *dst++ = *p;
            p++;
        }
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

        // Emite pontuação à esquerda
        while (token_start < core_start && dst < dst_end - 1) {
            *dst++ = *token_start++;
            *dst = '\0';
        }

        // Emite a palavra silabificada
        size_t core_len = (size_t)(core_end - core_start);
        if (core_len > 0) {
            syllabify_ptbr_word_buf(core_start, core_len, &dst, dst_end, sep);
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

char* phonemas_ptbr_syllabify(const char* text, char sep) {
    if (!text) return NULL;
    size_t needed = strlen(text) * 2 + 64;
    char* buf = (char*)malloc(needed);
    if (!buf) return NULL;
    phonemas_ptbr_syllabify_buffer(text, buf, needed, sep);
    return buf;
}

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
