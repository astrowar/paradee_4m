#ifndef PHONEMAS_PTBR_H
#define PHONEMAS_PTBR_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Converte texto pt-BR para fonemas IPA escrevendo diretamente em um buffer (Zero Heap).
 * Ideal para ESP32 e microcontroladores. Não realiza nenhuma alocação na heap (malloc/new).
 * @param text Texto UTF-8 de entrada.
 * @param out_buf Buffer pré-alocado pelo chamador (ex: char buf[512]).
 * @param max_len Tamanho máximo do buffer out_buf.
 * @return Número de bytes gravados no buffer (excluindo o terminador nulo '\0').
 */
size_t phonemas_ptbr_translate_buffer(const char* text, char* out_buf, size_t max_len);

/**
 * @brief Versão com alocação dinâmica (malloc) para desktop ou conveniência.
 * @param text Texto UTF-8 de entrada.
 * @return String alocada com malloc (deve ser liberada com phonemas_ptbr_free).
 */
char* phonemas_ptbr_translate(const char* text);

/**
 * @brief Libera memória alocada por phonemas_ptbr_translate.
 */
void phonemas_ptbr_free(char* ptr);

/**
 * @brief Converte texto UTF-8 diretamente para tokens inteiros do Paradee (Zero Heap).
 * Não realiza alocações na heap. Escreve diretamente no array fornecido pelo chamador.
 * @param text Texto UTF-8 de entrada.
 * @param out_tokens Buffer de inteiros pré-alocado (ex: int64_t tokens[128]).
 * @param max_tokens Capacidade máxima do buffer de tokens.
 * @return Número de tokens gravados, ou -1 se o buffer for insuficiente.
 */
int phonemas_ptbr_text_to_tokens(const char* text, int64_t* out_tokens, size_t max_tokens);

/**
 * @brief Converte string de fonemas IPA diretamente para tokens inteiros do Paradee (Zero Heap).
 * @param phonemes String de fonemas IPA.
 * @param out_tokens Buffer de saída.
 * @param max_tokens Capacidade máxima do buffer.
 * @return Número de tokens gravados.
 */
int phonemas_ptbr_phonemes_to_tokens(const char* phonemes, int64_t* out_tokens, size_t max_tokens);

#ifdef __cplusplus
} // extern "C"

#include <string>
#include <vector>

namespace phonemas_ptbr {

/**
 * @brief Converte texto UTF-8 em português brasileiro para sequência de fonemas IPA.
 * Wrapper C++ em cima do motor zero-heap.
 */
inline std::string text_to_phonemes(const std::string& text) {
    char buf[1024];
    phonemas_ptbr_translate_buffer(text.c_str(), buf, sizeof(buf));
    return std::string(buf);
}

/**
 * @brief Converte texto diretamente para IDs de tokens do Paradee (Texto -> Tokens).
 */
inline std::vector<int64_t> text_to_tokens(const std::string& text) {
    int64_t tokens[512];
    int count = phonemas_ptbr_text_to_tokens(text.c_str(), tokens, 512);
    if (count <= 0) return {};
    return std::vector<int64_t>(tokens, tokens + count);
}

/**
 * @brief Converte fonemas para IDs de tokens do Paradee.
 */
inline std::vector<int64_t> phonemes_to_tokens(const std::string& phonemes) {
    int64_t tokens[512];
    int count = phonemas_ptbr_phonemes_to_tokens(phonemes.c_str(), tokens, 512);
    if (count <= 0) return {};
    return std::vector<int64_t>(tokens, tokens + count);
}

} // namespace phonemas_ptbr

#endif // __cplusplus

#endif // PHONEMAS_PTBR_H
