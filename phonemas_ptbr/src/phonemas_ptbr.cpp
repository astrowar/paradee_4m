#include "phonemas_ptbr.h"
#include "embedded_vocab.h"
#include "phonemas_ptbr_types.h"
#include "syllabifier.h"
#include "phoneme_rules.h"

#include <stdlib.h>
#include <string.h>

extern "C" {

// ============================================================================
// API C PÚBLICA: SILABIFICAÇÃO FORMAL (ZERO HEAP)
// ============================================================================

size_t phonemas_ptbr_syllabify_buffer(const char* text, char* out_buf, size_t max_len, char sep) {
    if (!text || !out_buf || max_len == 0) return 0;
    return phonemas_ptbr::internal::syllabify_text_buf(text, out_buf, max_len, sep);
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
// API C PÚBLICA: TEXTO -> FONEMAS IPA (ZERO HEAP) COM SUPORTE A MODOS
// ============================================================================

size_t phonemas_ptbr_translate_buffer_mode(const char* text, char* out_buf, size_t max_len, PhonemasOutputMode mode) {
    if (!text || !out_buf || max_len == 0) return 0;
    return phonemas_ptbr::internal::translate_text_buf(text, out_buf, max_len, mode);
}

size_t phonemas_ptbr_translate_buffer(const char* text, char* out_buf, size_t max_len) {
    return phonemas_ptbr_translate_buffer_mode(text, out_buf, max_len, PHONEMAS_MODE_KOKORO);
}

size_t phonemas_ptbr_translate_rules_only(const char* word, char* out_buf, size_t max_len, PhonemasOutputMode mode) {
    if (!word || !out_buf || max_len == 0) return 0;
    char* dst = out_buf;
    const char* dst_end = out_buf + max_len;
    *dst = '\0';
    phonemas_ptbr::internal::phonemize_rules_only_buf(word, dst, dst_end, mode);
    *dst = '\0';
    return static_cast<size_t>(dst - out_buf);
}

char* phonemas_ptbr_translate_mode(const char* text, PhonemasOutputMode mode) {
    if (!text) return NULL;
    size_t needed = strlen(text) * 4 + 64;
    char* buf = (char*)malloc(needed);
    if (!buf) return NULL;
    phonemas_ptbr_translate_buffer_mode(text, buf, needed, mode);
    return buf;
}

char* phonemas_ptbr_translate(const char* text) {
    return phonemas_ptbr_translate_mode(text, PHONEMAS_MODE_KOKORO);
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
    phonemas_ptbr_translate_buffer_mode(text, ph_buf, sizeof(ph_buf), PHONEMAS_MODE_KOKORO);
    return phonemas_ptbr_phonemes_to_tokens(ph_buf, out_tokens, max_tokens);
}

} // extern "C"
