#ifndef PHONEMAS_PTBR_RULES_H
#define PHONEMAS_PTBR_RULES_H

#include "phonemas_ptbr_types.h"
#include <string_view>

namespace phonemas_ptbr::internal {

/**
 * @brief Classifica o valor fonético contextual da letra 'X' em Português Brasileiro.
 * Suporta detecção de raízes greco-latinas em /ks/ mesmo com prefixos comuns (anti-, auto-, des-).
 */
const char* classify_x_ptbr(std::string_view word, const std::string_view* chars, size_t n, size_t i);

/**
 * @brief Busca binária em O(log N) na tabela embutida de exceções (.rodata).
 */
const char* find_exception(std::string_view word, PhonemasOutputMode mode);

/**
 * @brief Transcreve uma única palavra simples em fonemas IPA, no modo especificado.
 * @param word_str Fatiamento de string_view com a palavra a ser transcrita.
 * @param dst Ponteiro de escrita no buffer de saída.
 * @param dst_end Limite superior de gravação no buffer de saída.
 * @param mode Modo de saída (PHONEMAS_MODE_KOKORO ou PHONEMAS_MODE_STANDARD_IPA).
 */
void phonemize_single_word_buf(std::string_view word_str, char*& dst, const char* dst_end, PhonemasOutputMode mode);

/**
 * @brief Fonetiza uma palavra com suporte especial a compostos hifenizados (ex: além-mar, após-guerra).
 */
void rule_based_ptbr_word_buf(std::string_view word_str, char*& dst, const char* dst_end, PhonemasOutputMode mode);

/**
 * @brief Transcreve um texto completo contendo pontuações, espaços e palavras para fonemas IPA (Zero Heap).
 */
size_t translate_text_buf(std::string_view text, char* out_buf, size_t max_len, PhonemasOutputMode mode);

} // namespace phonemas_ptbr::internal

#endif // PHONEMAS_PTBR_RULES_H
