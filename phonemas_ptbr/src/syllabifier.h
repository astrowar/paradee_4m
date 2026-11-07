#ifndef PHONEMAS_PTBR_SYLLABIFIER_H
#define PHONEMAS_PTBR_SYLLABIFIER_H

#include "phonemas_ptbr_types.h"
#include <string_view>

namespace phonemas_ptbr::internal {

/**
 * @brief Verifica se um caractere ASCII é pontuação comum delimitadora.
 */
bool is_punct_char(char c);

/**
 * @brief Identifica se o caractere UTF-8 representa uma vogal (oral, nasal ou acentuada).
 */
bool is_vowel_char(std::string_view c);

/**
 * @brief Identifica se o caractere carrega acento gráfico explícito.
 */
bool has_graphic_accent(std::string_view c);

/**
 * @brief Identifica vogais anteriores (front vowels: e, i e variações acentuadas).
 */
bool is_front_vowel(std::string_view c);

/**
 * @brief Identifica encontros consonantais inseparáveis (ataques complexos e dígrafos).
 */
bool is_inseparable_cluster(std::string_view c1, std::string_view c2);

/**
 * @brief Determina se dois caracteres vocálicos consecutivos formam um ditongo decrescente.
 */
bool is_diphthong(std::string_view c1, std::string_view c2);

/**
 * @brief Realiza a análise fonológica e divisão silábica formal de uma palavra (Onset-Nucleus-Coda).
 * @param word_str Fatiamento de string_view com a palavra a ser analisada.
 * @param wa Estrutura WordAnalysis que receberá a análise completa (alocada na stack).
 */
void analyze_and_syllabify_word(std::string_view word_str, WordAnalysis& wa);

/**
 * @brief Silabifica uma única palavra e escreve as sílabas no buffer dst com o separador fornecido.
 */
void syllabify_word_buf(std::string_view word_str, char*& dst, const char* dst_end, char sep);

/**
 * @brief Silabifica um texto completo contendo múltiplas palavras, espaços e pontuações (Zero Heap).
 */
size_t syllabify_text_buf(std::string_view text, char* out_buf, size_t max_len, char sep);

} // namespace phonemas_ptbr::internal

#endif // PHONEMAS_PTBR_SYLLABIFIER_H
