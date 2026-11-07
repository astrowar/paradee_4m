#include "phoneme_rules.h"
#include "syllabifier.h"
#include "embedded_exceptions.h"
#include "embedded_loanwords.h"
#include <string.h>
#include <ctype.h>

namespace phonemas_ptbr::internal {

// ============================================================================
// 1. UTILITÁRIO DE EMISSÃO SEM ALOCAÇÃO (ZERO HEAP)
// ============================================================================

static inline void emit_str(char*& dst, const char* end, std::string_view str) {
    for (char ch : str) {
        if (dst < end - 1) {
            *dst++ = ch;
        }
    }
    *dst = '\0';
}

// ============================================================================
// 2. BUSCA BINÁRIA NO DICIONÁRIO DE ESTRANGEIRISMOS (LOANWORDS EM FLASH)
// ============================================================================

const char* find_loanword(std::string_view word, PhonemasOutputMode mode) {
    if (EMBEDDED_LOANWORDS_SIZE == 0) return nullptr;
    int low = 0;
    int high = static_cast<int>(EMBEDDED_LOANWORDS_SIZE) - 1;
    while (low <= high) {
        int mid = low + (high - low) / 2;
        int cmp = word.compare(EMBEDDED_LOANWORDS[mid].word);
        if (cmp == 0) {
            return (mode == PHONEMAS_MODE_STANDARD_IPA)
                ? EMBEDDED_LOANWORDS[mid].phonemes_ipa
                : EMBEDDED_LOANWORDS[mid].phonemes_kokoro;
        } else if (cmp < 0) {
            high = mid - 1;
        } else {
            low = mid + 1;
        }
    }
    return nullptr;
}

// ============================================================================
// 2. CLASSIFICADOR CONTEXTUAL DA LETRA X
// ============================================================================

const char* classify_x_ptbr(std::string_view word, const std::string_view* chars, size_t n, size_t i) {
    if (chars[i] != "x") return nullptr;

    std::string_view prev = (i > 0) ? chars[i - 1] : std::string_view{};
    std::string_view next = (i + 1 < n) ? chars[i + 1] : std::string_view{};

    // 1. X em início absoluto de palavra: SEMPRE /ʃ/ (xícara, xadrez, xarope, xampu, xale, xaveco)
    if (i == 0) return "ʃ";

    // 2. Prefixo en- + X: SEMPRE /ʃ/ (enxame, enxada, enxugar, enxofre, enxaqueca)
    if (i >= 2 && chars[i - 2] == "e" && chars[i - 1] == "n") {
        return "ʃ";
    }

    // 3. Sequência inicial 'ex-'
    if (i == 1 && chars[0] == "e") {
        if (is_vowel_char(next)) return "z"; // exame, exemplo, existir, exército, exato
        if (next == "p" || next == "t" || next == "c" || next == "s" || next == "f") return "s"; // explicar, extra, excelente
        return "s";
    }

    // 4. Palavras conhecidas com som de /s/ (aproximar, próximo, máximo, auxílio, trouxe)
    if (word.rfind("próxim", 0) == 0 || word.rfind("proxim", 0) == 0 ||
        word.rfind("aproxim", 0) == 0 || word.rfind("máxim", 0) == 0 ||
        word.rfind("maxim", 0) == 0 || word.rfind("auxili", 0) == 0 ||
        word.rfind("auxíli", 0) == 0 || word.rfind("troux", 0) == 0) {
        return "s";
    }

    // 5. Raízes cultas greco-latinas com som de /ks/ (detectadas no início ou após prefixos como anti-, auto-, des-)
    static const std::string_view ks_roots[] = {
        "fix", "sex", "conex", "complex", "flex", "reflex", "paradox",
        "táxi", "taxi", "tóx", "tox", "léx", "lex", "oxig", "asfix",
        "anex", "nex", "maxil", "prolix", "ortodox", "heterodox"
    };
    for (const auto& root : ks_roots) {
        if (word.find(root) != std::string_view::npos) {
            return "ks";
        }
    }

    // 6. Ditongo decrescente + X: som de /ʃ/ (caixa, peixe, frouxo, baixo, feixe)
    if (i >= 2) {
        std::string_view preprev = chars[i - 2];
        if ((preprev == "a" && prev == "i") ||
            (preprev == "e" && prev == "i") ||
            (preprev == "o" && prev == "u") ||
            (preprev == "a" && prev == "u")) return "ʃ";
    }

    // 7. Após vogais altas 'u' ou 'i': som de /ʃ/ (luxo, puxar, bruxa, lixo)
    if (prev == "u" || prev == "i") return "ʃ";

    // 8. X em final de palavra: SEMPRE /ks/ em pt-BR (tórax, clímax, duplex, látex, fênix, córtex)
    if (i == n - 1) return "ks";

    // 9. X antes de consoante interna: som de /s/ (texto, sexta, contexto)
    if (!is_vowel_char(next)) return "s";

    // 10. Fallback canônico para X entre vogais em português: som de /ʃ/ (abacaxi, faxina, graxa, taxa, mexer)
    return "ʃ";
}

// ============================================================================
// 3. BUSCA BINÁRIA NA TABELA DE EXCEÇÕES (.RODATA)
// ============================================================================

const char* find_exception(std::string_view word, PhonemasOutputMode mode) {
    if (EMBEDDED_EXCEPTIONS_SIZE == 0) return nullptr;
    int low = 0;
    int high = static_cast<int>(EMBEDDED_EXCEPTIONS_SIZE) - 1;
    while (low <= high) {
        int mid = low + (high - low) / 2;
        int cmp = word.compare(EMBEDDED_EXCEPTIONS[mid].word);
        if (cmp == 0) {
            return (mode == PHONEMAS_MODE_STANDARD_IPA)
                ? EMBEDDED_EXCEPTIONS[mid].phonemes_ipa
                : EMBEDDED_EXCEPTIONS[mid].phonemes_kokoro;
        } else if (cmp < 0) {
            high = mid - 1;
        } else {
            low = mid + 1;
        }
    }
    return nullptr;
}

// Expande inteiros (0 a 9999) para palavras em português por extenso
static void expand_number_ptbr(int n, char*& dst, const char* dst_end, PhonemasOutputMode mode) {
    if (n < 0 || n > 9999) return;
    if (n == 0) { phonemize_single_word_buf("zero", dst, dst_end, mode); return; }
    if (n == 100) { phonemize_single_word_buf("cem", dst, dst_end, mode); return; }

    static const char* unidades[] = {"", "um", "dois", "três", "quatro", "cinco", "seis", "sete", "oito", "nove"};
    static const char* especiais[] = {"dez", "onze", "doze", "treze", "quatorze", "quinze", "dezesseis", "dezessete", "dezoito", "dezenove"};
    static const char* dezenas[] = {"", "", "vinte", "trinta", "quarenta", "cinquenta", "sessenta", "setenta", "oitenta", "noventa"};
    static const char* centenas[] = {"", "cento", "duzentos", "trezentos", "quatrocentos", "quinhentos", "seiscentos", "setecentos", "oitocentos", "novecentos"};

    bool first = true;
    auto emit_word = [&](const char* w) {
        if (!first && dst < dst_end - 1) *dst++ = ' ';
        phonemize_single_word_buf(w, dst, dst_end, mode);
        first = false;
    };

    if (n >= 1000) {
        int milhar = n / 1000;
        n %= 1000;
        if (milhar > 1) {
            expand_number_ptbr(milhar, dst, dst_end, mode);
            first = false;
        }
        emit_word("mil");
        if (n > 0 && n < 100) emit_word("e");
    }

    if (n >= 100) {
        int c = n / 100;
        n %= 100;
        emit_word(centenas[c]);
        if (n > 0) emit_word("e");
    }

    if (n >= 20) {
        int d = n / 10;
        int u = n % 10;
        emit_word(dezenas[d]);
        if (u > 0) {
            emit_word("e");
            emit_word(unidades[u]);
        }
    } else if (n >= 10) {
        emit_word(especiais[n - 10]);
    } else if (n > 0) {
        emit_word(unidades[n]);
    }
}

// ============================================================================
// 4. MOTOR DE TRANSCRIÇÃO FONÉTICA DE PALAVRA INDIVIDUAL
// ============================================================================

void phonemize_single_word_buf(std::string_view word_str, char*& dst, const char* dst_end, PhonemasOutputMode mode) {
    if (word_str.empty()) return;

    // Buffer local para lowercase (máximo 64 bytes para uma única palavra em pt-BR)
    char lower_word[64];
    size_t w_len = word_str.size() < sizeof(lower_word) - 1 ? word_str.size() : sizeof(lower_word) - 1;
    for (size_t k = 0; k < w_len; ++k) {
        lower_word[k] = static_cast<char>(tolower(static_cast<unsigned char>(word_str[k])));
    }
    lower_word[w_len] = '\0';
    std::string_view lw(lower_word, w_len);

    // 0. Clíticos átonos e letras isoladas essenciais
    if (lw == "em") {
        emit_str(dst, dst_end, "ẽj");
        return;
    }
    if (lw == "que") {
        emit_str(dst, dst_end, (mode == PHONEMAS_MODE_STANDARD_IPA) ? "ki" : "ky");
        return;
    }
    if (lw == "c") {
        emit_str(dst, dst_end, "sˈe"); // Letra C / C++ / Vitamina C
        return;
    }

    // 1. Dicionário dedicado de Estrangeirismos e Empréstimos Linguísticos (Loanwords em Flash)
    const char* loan = find_loanword(lw, mode);
    if (loan != nullptr) {
        emit_str(dst, dst_end, loan);
        return;
    }

    // 2. Busca na tabela de exceções gerais em Flash (.rodata)
    const char* exc = find_exception(lw, mode);
    if (exc != nullptr) {
        emit_str(dst, dst_end, exc);
        return;
    }

    // 2. Análise silábica estruturada e acentuação formal
    WordAnalysis wa;
    analyze_and_syllabify_word(lw, wa);

    size_t n = wa.num_chars;
    const std::string_view* chars = wa.lower_chars;
    int tonic_char_pos = wa.tonic_char_idx;
    int sec_char_pos = wa.sec_char_idx;

    // 3. Varredura e transformação fonética
    for (size_t i = 0; i < n && dst < dst_end - 1; ) {
        std::string_view c = chars[i];
        std::string_view next = (i + 1 < n) ? chars[i + 1] : std::string_view{};
        std::string_view next2 = (i + 2 < n) ? chars[i + 2] : std::string_view{};
        std::string_view prev = (i > 0) ? chars[i - 1] : std::string_view{};

        // Emissão de acento primário (ˈ) e secundário (ˌ)
        if (static_cast<int>(i) == tonic_char_pos) {
            emit_str(dst, dst_end, "ˈ");
        } else if (static_cast<int>(i) == sec_char_pos) {
            emit_str(dst, dst_end, "ˌ");
        }

        // Letra X contextual
        if (c == "x") {
            const char* x_ph = classify_x_ptbr(lw, chars, n, i);
            if (x_ph != nullptr) {
                emit_str(dst, dst_end, x_ph);
                if (i == 1 && next == "c" && is_front_vowel(next2)) i += 2;
                else i++;
                continue;
            }
        }

        // Dígrafos inseparáveis
        if (c == "c" && next == "h") { emit_str(dst, dst_end, "ʃ"); i += 2; continue; }
        if (c == "l" && next == "h") { emit_str(dst, dst_end, "ʎ"); i += 2; continue; }
        if (c == "n" && next == "h") { emit_str(dst, dst_end, "ɲ"); i += 2; continue; }
        if (c == "r" && next == "r") { emit_str(dst, dst_end, "x"); i += 2; continue; }
        if (c == "s" && next == "s") { emit_str(dst, dst_end, "s"); i += 2; continue; }

        // Q e G antes de U
        if (c == "q" && next == "u") {
            if (is_front_vowel(next2)) { emit_str(dst, dst_end, "k"); i += 2; continue; }
            else { emit_str(dst, dst_end, "kw"); i += 2; continue; }
        }
        if (c == "g" && next == "u") {
            if (is_front_vowel(next2)) { emit_str(dst, dst_end, "ɡ"); i += 2; continue; }
            else if (is_vowel_char(next2)) { emit_str(dst, dst_end, "ɡw"); i += 2; continue; }
        }

        // C e Ç
        if (c == "c") {
            if (is_front_vowel(next)) emit_str(dst, dst_end, "s");
            else if (!is_vowel_char(next) && next != "r" && next != "l" && next != "h" && i + 1 < n) {
                emit_str(dst, dst_end, "ki"); // Epêntese de C mudo (compacto, aspecto, técnico)
            } else {
                emit_str(dst, dst_end, "k");
            }
            i++; continue;
        }
        if (c == "ç") { emit_str(dst, dst_end, "s"); i++; continue; }

        // G e J
        if (c == "g") {
            if (is_front_vowel(next)) emit_str(dst, dst_end, "ʒ");
            else if (!is_vowel_char(next) && next != "r" && next != "l" && i + 1 < n) {
                emit_str(dst, dst_end, "ɡi"); // Epêntese de G mudo (signo, digno, gnomo)
            } else {
                emit_str(dst, dst_end, "ɡ");
            }
            i++; continue;
        }
        if (c == "j") { emit_str(dst, dst_end, "ʒ"); i++; continue; }

        // H mudo inicial
        if (c == "h" && i == 0) { i++; continue; }

        // R (início, tepe intervocálico e vibrante)
        if (c == "r") {
            if (i == 0 || prev == "n" || prev == "s" || prev == "l") {
                emit_str(dst, dst_end, "x");
            } else if (prev == "b" || prev == "p" || prev == "t" ||
                       prev == "d" || prev == "c" || prev == "g" ||
                       prev == "f" || prev == "v") {
                emit_str(dst, dst_end, "r");
            } else {
                emit_str(dst, dst_end, "ɾ");
            }
            i++; continue;
        }

        // S (vozeamento assimilatório diante de sonoras em coda: mesmo, desde, rasgo)
        if (c == "s") {
            if (is_vowel_char(prev) && is_vowel_char(next)) {
                emit_str(dst, dst_end, "z");
            } else if (!is_vowel_char(next) && i + 1 < n &&
                       (next == "b" || next == "d" || next == "g" ||
                        next == "v" || next == "m" || next == "n" ||
                        next == "l" || next == "r" || next == "z")) {
                emit_str(dst, dst_end, "z");
            } else {
                emit_str(dst, dst_end, "s");
            }
            i++; continue;
        }

        // Z em final de palavra ou antes de surdas vira 's' (luz, feliz, arroz)
        if (c == "z") {
            if (i == n - 1 || (!is_vowel_char(next) && (next == "p" || next == "t" ||
                next == "c" || next == "f" || next == "s" || next == "k"))) {
                emit_str(dst, dst_end, "s");
                i++; continue;
            }
        }

        // T e D (palatalização diante de i/í, e final átono e epêntese diante de consoante)
        if (c == "t") {
            bool is_palatal_e = (next == "e" && static_cast<int>(i) + 1 != tonic_char_pos) &&
                                ((i + 1 == n - 1) || (i + 2 == n - 1 && next2 == "s"));
            if (next == "i" || next == "í" || is_palatal_e) {
                const char* affricate_t = (mode == PHONEMAS_MODE_STANDARD_IPA) ? "tʃ" : "ʧ";
                emit_str(dst, dst_end, affricate_t);
                if (is_palatal_e) {
                    emit_str(dst, dst_end, (mode == PHONEMAS_MODE_STANDARD_IPA) ? "i" : "y");
                    if (next2 == "s") {
                        emit_str(dst, dst_end, "s");
                        i += 3;
                    } else {
                        i += 2;
                    }
                    continue;
                }
            } else if (!is_vowel_char(next) && next != "r" && next != "l" && next != "h" && i + 1 < n) {
                emit_str(dst, dst_end, (mode == PHONEMAS_MODE_STANDARD_IPA) ? "tʃi" : "ʧi"); // Epêntese paulista
            } else {
                emit_str(dst, dst_end, "t");
            }
            i++; continue;
        }
        if (c == "d") {
            bool is_palatal_e = (next == "e" && static_cast<int>(i) + 1 != tonic_char_pos) &&
                                ((i + 1 == n - 1) || (i + 2 == n - 1 && next2 == "s"));
            if (next == "i" || next == "í" || is_palatal_e) {
                const char* affricate_d = (mode == PHONEMAS_MODE_STANDARD_IPA) ? "dʒ" : "ʤ";
                emit_str(dst, dst_end, affricate_d);
                if (is_palatal_e) {
                    emit_str(dst, dst_end, (mode == PHONEMAS_MODE_STANDARD_IPA) ? "i" : "y");
                    if (next2 == "s") {
                        emit_str(dst, dst_end, "s");
                        i += 3;
                    } else {
                        i += 2;
                    }
                    continue;
                }
            } else if (!is_vowel_char(next) && next != "r" && next != "l" && i + 1 < n) {
                emit_str(dst, dst_end, (mode == PHONEMAS_MODE_STANDARD_IPA) ? "dʒi" : "ʤi"); // Epêntese paulista
            } else {
                emit_str(dst, dst_end, "d");
            }
            i++; continue;
        }

        // P, B, F (com epêntese diante de consoante que não seja líquida r/l)
        if (c == "p") {
            if (!is_vowel_char(next) && next != "r" && next != "l" && next != "h" && i + 1 < n) {
                emit_str(dst, dst_end, "pi");
            } else {
                emit_str(dst, dst_end, "p");
            }
            i++; continue;
        }
        if (c == "b") {
            if (!is_vowel_char(next) && next != "r" && next != "l" && i + 1 < n) {
                emit_str(dst, dst_end, "bi");
            } else {
                emit_str(dst, dst_end, "b");
            }
            i++; continue;
        }
        if (c == "f") {
            if (!is_vowel_char(next) && next != "r" && next != "l" && i + 1 < n) {
                emit_str(dst, dst_end, "fi");
            } else {
                emit_str(dst, dst_end, "f");
            }
            i++; continue;
        }

        // L em coda silábica (vocalização brasileira: [ʊ] no Kokoro, [w] no IPA padrão)
        if (c == "l" && (i == n - 1 || (!is_vowel_char(next) && next != "h"))) {
            emit_str(dst, dst_end, (mode == PHONEMAS_MODE_STANDARD_IPA) ? "w" : "ʊ");
            i++; continue;
        }

        // Ditongos nasais gráficos (ão, ãe, õe)
        if (c == "ã" && next == "o") {
            emit_str(dst, dst_end, (mode == PHONEMAS_MODE_STANDARD_IPA) ? "ɐ̃w̃" : "ɐ̃ʊ̃");
            i += 2; continue;
        }
        if (c == "ã" && next == "e") {
            if (next2 == "s" && i + 2 == n - 1) {
                emit_str(dst, dst_end, "ɐ̃js"); i += 3; continue;
            } else {
                emit_str(dst, dst_end, "ɐ̃j"); i += 2; continue;
            }
        }
        if (c == "õ" && next == "e" && next2 == "s" && i + 2 == n - 1) {
            emit_str(dst, dst_end, "õjs"); i += 3; continue;
        }
        if (c == "õ" && next == "e") { emit_str(dst, dst_end, "õj"); i += 2; continue; }
        if (c == "ã") { emit_str(dst, dst_end, "ɐ̃"); i++; continue; }
        if (c == "õ") { emit_str(dst, dst_end, "õ"); i++; continue; }

        // Ditongos orais decrescentes
        if (c == "e" && next == "i") {
            emit_str(dst, dst_end, (mode == PHONEMAS_MODE_STANDARD_IPA) ? "ej" : "A");
            i += 2; continue;
        }
        if (c == "a" && next == "i") {
            emit_str(dst, dst_end, (mode == PHONEMAS_MODE_STANDARD_IPA) ? "aj" : "I");
            i += 2; continue;
        }
        if (c == "a" && next == "u") {
            emit_str(dst, dst_end, (mode == PHONEMAS_MODE_STANDARD_IPA) ? "aw" : "W");
            i += 2; continue;
        }
        if (c == "o" && next == "i") {
            emit_str(dst, dst_end, (mode == PHONEMAS_MODE_STANDARD_IPA) ? "oj" : "Y");
            i += 2; continue;
        }
        if (c == "o" && next == "u") { emit_str(dst, dst_end, "ow"); i += 2; continue; }

        // Nasalização diante de M/N em coda
        if (is_vowel_char(c) && (next == "m" || next == "n") && next2 != "h" &&
            (i + 2 == n || !is_vowel_char(next2))) {
            // Desinência verbal átona final -am -> ditongo nasal ɐ̃ʊ̃ (falaram, cantam, viajam)
            if ((c == "a" || c == "á" || c == "â" || c == "à") &&
                next == "m" && i + 2 == n && static_cast<int>(i) != tonic_char_pos) {
                emit_str(dst, dst_end, (mode == PHONEMAS_MODE_STANDARD_IPA) ? "ɐ̃w̃" : "ɐ̃ʊ̃");
                i += 2; continue;
            }
            // Terminação final -em / -ens -> ditongo nasal ẽj / ẽjs (homem, comem, jovem, viagens)
            if ((c == "e" || c == "é" || c == "ê") && next == "m" && i + 2 == n) {
                emit_str(dst, dst_end, "ẽj"); i += 2; continue;
            }
            if ((c == "e" || c == "é" || c == "ê") && next == "n" && next2 == "s" && i + 3 == n) {
                emit_str(dst, dst_end, "ẽjs"); i += 3; continue;
            }

            // Nasais internas
            if (c == "a" || c == "á" || c == "â" || c == "à") {
                emit_str(dst, dst_end, (mode == PHONEMAS_MODE_STANDARD_IPA) ? "ɐ̃" : "ɐ̃ŋ");
            } else if (c == "e" || c == "é" || c == "ê") {
                emit_str(dst, dst_end, (mode == PHONEMAS_MODE_STANDARD_IPA) ? "ẽ" : "Aŋ");
            } else if (c == "i" || c == "í") {
                emit_str(dst, dst_end, (mode == PHONEMAS_MODE_STANDARD_IPA) ? "ĩ" : "iŋ");
            } else if (c == "o" || c == "ó" || c == "ô") {
                emit_str(dst, dst_end, (mode == PHONEMAS_MODE_STANDARD_IPA) ? "õ" : "oŋ");
            } else if (c == "u" || c == "ú") {
                emit_str(dst, dst_end, (mode == PHONEMAS_MODE_STANDARD_IPA) ? "ũ" : "ũŋ");
            }
            i += 2; continue;
        }

        // Vogais orais, reduções átonas e aberturas morfológicas
        if (c == "a" || c == "á" || c == "à") {
            bool ends_with_mente = (lw.size() >= 5 && lw.substr(lw.size() - 5) == "mente");
            bool is_pre_mente_a = ends_with_mente && (i + 5 == n);
            bool is_atona_final = ((i == n - 1) || (i + 1 == n - 1 && next == "s") || is_pre_mente_a) &&
                                  (static_cast<int>(i) != tonic_char_pos);
            if (c == "a" && is_atona_final) {
                emit_str(dst, dst_end, (mode == PHONEMAS_MODE_STANDARD_IPA) ? "ə" : "æ");
            } else {
                emit_str(dst, dst_end, "a");
            }
        }
        else if (c == "â") emit_str(dst, dst_end, "ɐ");
        else if (c == "é") emit_str(dst, dst_end, "ɛ");
        else if (c == "ê") emit_str(dst, dst_end, "e");
        else if (c == "e") {
            if (((i == n - 1) || (i + 1 == n - 1 && next == "s")) && static_cast<int>(i) != tonic_char_pos) {
                emit_str(dst, dst_end, (mode == PHONEMAS_MODE_STANDARD_IPA) ? "i" : "y");
            } else if (static_cast<int>(i) == tonic_char_pos && (
                (lw.size() >= 4 && (lw.substr(lw.size() - 4) == "esta" ||
                                    lw.substr(lw.size() - 4) == "este" ||
                                    lw.substr(lw.size() - 4) == "esto" ||
                                    lw.substr(lw.size() - 4) == "erra")) ||
                (lw.size() >= 5 && (lw.substr(lw.size() - 5) == "estas" ||
                                    lw.substr(lw.size() - 5) == "estes" ||
                                    lw.substr(lw.size() - 5) == "estos" ||
                                    lw.substr(lw.size() - 5) == "erras")) ||
                lw == "mulher" || lw == "certo" || lw == "certa" ||
                lw == "certos" || lw == "certas" || lw == "zero")) {
                emit_str(dst, dst_end, "ɛ"); // Abertura vocálica morfológica
            } else {
                emit_str(dst, dst_end, "e");
            }
        }
        else if (c == "i" || c == "í") emit_str(dst, dst_end, "i");
        else if (c == "ó") emit_str(dst, dst_end, "ɔ");
        else if (c == "ô") emit_str(dst, dst_end, "o");
        else if (c == "o") {
            if (((i == n - 1) || (i + 1 == n - 1 && next == "s")) && static_cast<int>(i) != tonic_char_pos) {
                emit_str(dst, dst_end, "ʊ");
            } else if (static_cast<int>(i) == tonic_char_pos && (
                (lw.size() >= 4 && (lw.substr(lw.size() - 4) == "orta" ||
                                    lw.substr(lw.size() - 4) == "orte")) ||
                (lw.size() >= 5 && (lw.substr(lw.size() - 5) == "ortas" ||
                                    lw.substr(lw.size() - 5) == "ortes")) ||
                lw == "sol" || lw == "hora" || lw == "horas" || lw == "agora" ||
                lw == "bola" || lw == "bolas" || lw == "mola" || lw == "molas" ||
                lw == "escola" || lw == "escolas" || lw == "nova" || lw == "novas" ||
                lw == "novo" || lw == "novos" || lw == "nove" || lw == "forte" ||
                lw == "fortes" || lw == "morte" || lw == "mortes" || lw == "porta" ||
                lw == "portas")) {
                emit_str(dst, dst_end, "ɔ"); // Abertura vocálica morfológica
            } else {
                emit_str(dst, dst_end, "o");
            }
        }
        else if (c == "u" || c == "ú") emit_str(dst, dst_end, "u");
        else {
            // Consoante direta UTF-8
            emit_str(dst, dst_end, c);
        }
        i++;
    }
}

void rule_based_ptbr_word_buf(std::string_view word_str, char*& dst, const char* dst_end, PhonemasOutputMode mode) {
    if (word_str.empty()) return;

    // 1. Números puros (ex: "17", "32", "2026")
    bool all_digits = true;
    for (char c : word_str) {
        if (!isdigit(static_cast<unsigned char>(c))) { all_digits = false; break; }
    }
    if (all_digits && word_str.size() <= 4) {
        int val = 0;
        for (char c : word_str) val = val * 10 + (c - '0');
        expand_number_ptbr(val, dst, dst_end, mode);
        return;
    }

    // 2. Se a palavra contém hífen interno (ex: além-mar, guarda-chuva, bem-bom)
    size_t hyphen_pos = word_str.find('-');
    if (hyphen_pos != std::string_view::npos && hyphen_pos > 0 && hyphen_pos < word_str.size() - 1) {
        std::string_view part1 = word_str.substr(0, hyphen_pos);
        rule_based_ptbr_word_buf(part1, dst, dst_end, mode);
        if (dst < dst_end - 1) {
            *dst++ = '-';
            *dst = '\0';
        }
        std::string_view part2 = word_str.substr(hyphen_pos + 1);
        rule_based_ptbr_word_buf(part2, dst, dst_end, mode);
        return;
    }

    // 3. Se a palavra contém underscore interno (ex: string_view)
    size_t underscore_pos = word_str.find('_');
    if (underscore_pos != std::string_view::npos && underscore_pos > 0 && underscore_pos < word_str.size() - 1) {
        std::string_view part1 = word_str.substr(0, underscore_pos);
        rule_based_ptbr_word_buf(part1, dst, dst_end, mode);
        if (dst < dst_end - 1) {
            *dst++ = ' ';
            *dst = '\0';
        }
        std::string_view part2 = word_str.substr(underscore_pos + 1);
        rule_based_ptbr_word_buf(part2, dst, dst_end, mode);
        return;
    }

    // 4. Se a palavra contém o símbolo '+' (ex: C++, C++17)
    size_t plus_pos = word_str.find('+');
    if (plus_pos != std::string_view::npos) {
        std::string_view part1 = word_str.substr(0, plus_pos);
        if (!part1.empty()) {
            rule_based_ptbr_word_buf(part1, dst, dst_end, mode);
            if (dst < dst_end - 1) *dst++ = ' ';
        }
        size_t plus_end = plus_pos;
        while (plus_end < word_str.size() && word_str[plus_end] == '+') {
            if (plus_end > plus_pos && dst < dst_end - 1) *dst++ = ' ';
            phonemize_single_word_buf("mais", dst, dst_end, mode);
            plus_end++;
        }
        if (plus_end < word_str.size()) {
            if (dst < dst_end - 1) *dst++ = ' ';
            std::string_view remainder = word_str.substr(plus_end);
            rule_based_ptbr_word_buf(remainder, dst, dst_end, mode);
        }
        return;
    }

    // 5. Se a palavra contém letras seguidas de números (ex: ESP32)
    size_t digit_start = 0;
    while (digit_start < word_str.size() && !isdigit(static_cast<unsigned char>(word_str[digit_start]))) {
        digit_start++;
    }
    if (digit_start > 0 && digit_start < word_str.size()) {
        std::string_view letters = word_str.substr(0, digit_start);
        std::string_view digits = word_str.substr(digit_start);
        rule_based_ptbr_word_buf(letters, dst, dst_end, mode);
        if (dst < dst_end - 1) *dst++ = ' ';
        rule_based_ptbr_word_buf(digits, dst, dst_end, mode);
        return;
    }

    phonemize_single_word_buf(word_str, dst, dst_end, mode);
}

size_t translate_text_buf(std::string_view text, char* out_buf, size_t max_len, PhonemasOutputMode mode) {
    if (text.empty() || !out_buf || max_len == 0) return 0;

    char* dst = out_buf;
    const char* dst_end = out_buf + max_len;
    *dst = '\0';

    size_t i = 0;
    size_t text_len = text.size();
    bool first_token = true;

    while (i < text_len) {
        // Pula espaços em branco
        while (i < text_len && isspace(static_cast<unsigned char>(text[i]))) i++;
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

        if (!first_token && dst < dst_end - 1) {
            *dst++ = ' ';
            *dst = '\0';
        }
        first_token = false;

        // Emite pontuação à esquerda
        for (size_t k = token_start; k < core_start && dst < dst_end - 1; ++k) {
            *dst++ = text[k];
        }
        *dst = '\0';

        // Emite a palavra fonetizada
        if (core_end > core_start) {
            std::string_view core = text.substr(core_start, core_end - core_start);
            rule_based_ptbr_word_buf(core, dst, dst_end, mode);
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
