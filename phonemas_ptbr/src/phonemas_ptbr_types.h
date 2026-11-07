#ifndef PHONEMAS_PTBR_TYPES_H
#define PHONEMAS_PTBR_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string_view>
#include "../include/phonemas_ptbr.h"

/**
 * @brief Classificação da acentuação tônica de uma palavra em Português.
 */
enum class StressType {
    None = 0,         ///< Palavra átona ou sem identificação de tônica (ex: clíticos)
    Oxytone = 1,      ///< Oxítona: acento na última sílaba (ex: ca-fé, a-mor, ba-tom)
    Paroxytone = 2,   ///< Paroxítona: acento na penúltima sílaba (ex: ca-sa, a-ba-ca-te)
    Proparoxytone = 3 ///< Proparoxítona: acento na antepenúltima sílaba (ex: ár-vo-re, lâm-pa-da)
};

/**
 * @brief Representação de um núcleo vocálico (monotongo ou ditongo) na sílaba.
 */
struct Nucleus {
    uint8_t start_idx;  ///< Índice do primeiro caractere no array de caracteres da palavra
    uint8_t len;        ///< Quantidade de caracteres no núcleo (1 = monotongo, 2 = ditongo)
    bool has_accent;    ///< Indica se o núcleo contém acento gráfico explícito (ex: á, ê, ó, ã)
};

/**
 * @brief Estrutura fonológica de uma sílaba completa (Ataque - Núcleo - Coda).
 */
struct Syllable {
    uint8_t start_char_idx; ///< Índice do primeiro caractere da sílaba
    uint8_t end_char_idx;   ///< Índice após o último caractere da sílaba
    uint8_t onset_len;      ///< Tamanho do ataque consonantal (consoantes antes do núcleo)
    uint8_t nucleus_len;    ///< Tamanho do núcleo vocálico
    uint8_t coda_len;       ///< Tamanho da coda consonantal (consoantes após o núcleo)
    bool is_tonic;          ///< Indica se esta sílaba carrega o acento tônico primário (ˈ)
    bool is_secondary;      ///< Indica se esta sílaba carrega acento rítmico secundário (ˌ)
};

/**
 * @brief Estrutura agregada com a análise fonológica completa de uma palavra.
 * Alocada 100% na stack (zero heap), ocupando menos de 400 bytes.
 */
struct WordAnalysis {
    char lower_word[64];              ///< Cópia em minúsculas da palavra, terminada em '\0'
    size_t word_len;                  ///< Comprimento da palavra em bytes

    std::string_view orig_chars[64];  ///< Fatias zero-copy dos caracteres originais (preserva maiúsculas)
    std::string_view lower_chars[64]; ///< Fatias zero-copy dos caracteres em minúsculas
    size_t num_chars;                 ///< Quantidade total de caracteres UTF-8 na palavra

    Nucleus nuclei[32];               ///< Núcleos vocálicos detectados
    size_t num_nuclei;                ///< Quantidade de núcleos vocálicos

    uint8_t cuts[32];                 ///< Índices de corte silábico (fronteiras de sílaba)
    size_t num_syllables;             ///< Quantidade total de sílabas

    int tonic_syl_idx;                ///< Índice da sílaba tônica (0 a num_syllables - 1)
    int tonic_char_idx;               ///< Índice do caractere onde o acento primário (ˈ) deve ser emitido
    int sec_syl_idx;                  ///< Índice da sílaba com acento secundário (ˌ), ou -1 se não houver
    int sec_char_idx;                 ///< Índice do caractere do acento secundário, ou -1 se não houver

    StressType stress_type;           ///< Classificação tônica (oxítona, paroxítona, proparoxítona)
};

#endif // PHONEMAS_PTBR_TYPES_H
