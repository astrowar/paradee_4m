#include "phonemas_ptbr.h"
#include "embedded_vocab.h"
#include "phonemas_ptbr_types.h"
#include "phonemas_ptbr_syllabifier.h"
#include "phonemas_ptbr_rules.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

extern "C" {

// ============================================================================
// API C PÚBLICA: SILABIFICAÇÃO FORMAL (ZERO HEAP)
// ============================================================================

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

        // Emite a palavra silabificada (Onset-Nucleus-Coda)
        size_t core_len = (size_t)(core_end - core_start);
        if (core_len > 0) {
            syllabify_word_buf(core_start, core_len, &dst, dst_end, sep);
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

// ============================================================================
// API C PÚBLICA: TEXTO -> FONEMAS IPA (ZERO HEAP)
// ============================================================================

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

// ============================================================================
// API C PÚBLICA: FONEMAS -> TOKENS KOKORO/PARADEE (ZERO HEAP)
// ============================================================================

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
