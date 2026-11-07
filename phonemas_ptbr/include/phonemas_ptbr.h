#ifndef PHONEMAS_PTBR_H
#define PHONEMAS_PTBR_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Modos de saída fonética do motor phonemas_ptbr.
 */
typedef enum {
    /**
     * @brief Modo padrão: compatível com o vocabulário neural do Kokoro/Paradee pt-BR (114 tokens).
     * Utiliza convenções treinadas no modelo: 'y' para -e átono final, 'æ' para -a átono final,
     * 'ʊ' para coda L, ditongos 'A', 'I', 'W', 'Y' e africadas 'ʧ', 'ʤ'.
     */
    PHONEMAS_MODE_KOKORO = 0,

    /**
     * @brief Modo IPA Padrão: compatível com a norma culta e datasets linguísticos (ex: TigreGótico / Portal da Língua Portuguesa).
     * Utiliza representação IPA canônica: 'i' para -e final reduzido, 'ə'/'ɐ' para -a final,
     * 'w' para coda L vocalizada, ditongos 'ej', 'aj', 'aw', 'oj' e sequências 'tʃ', 'dʒ'.
     */
    PHONEMAS_MODE_STANDARD_IPA = 1
} PhonemasOutputMode;

/**
 * @brief Converte texto pt-BR para fonemas IPA escrevendo diretamente em um buffer (Zero Heap).
 * @param text Texto UTF-8 de entrada.
 * @param out_buf Buffer pré-alocado pelo chamador (ex: char buf[512]).
 * @param max_len Tamanho máximo do buffer out_buf.
 * @param mode Modo de saída fonética (PHONEMAS_MODE_KOKORO ou PHONEMAS_MODE_STANDARD_IPA).
 * @return Número de bytes gravados no buffer (excluindo o terminador nulo '\0').
 */
size_t phonemas_ptbr_translate_buffer_mode(const char* text, char* out_buf, size_t max_len, PhonemasOutputMode mode);

/**
 * @brief Converte texto pt-BR para fonemas IPA escrevendo no buffer (modo Kokoro por padrão).
 */
size_t phonemas_ptbr_translate_buffer(const char* text, char* out_buf, size_t max_len);

/**
 * @brief Versão com alocação dinâmica (malloc) para desktop ou conveniência.
 * @param text Texto UTF-8 de entrada.
 * @param mode Modo de saída fonética.
 * @return String alocada com malloc (deve ser liberada com phonemas_ptbr_free).
 */
char* phonemas_ptbr_translate_mode(const char* text, PhonemasOutputMode mode);

/**
 * @brief Versão com alocação dinâmica (modo Kokoro por padrão).
 */
char* phonemas_ptbr_translate(const char* text);

/**
 * @brief Libera strings alocadas pelas funções de conveniência.
 */
void phonemas_ptbr_free(char* ptr);

/**
 * @brief Converte texto pt-BR diretamente para tokens inteiros do Paradee (Zero Heap).
 * @param text Texto UTF-8 de entrada.
 * @param out_tokens Buffer pré-alocado pelo chamador (ex: int64_t tokens[512]).
 * @param max_tokens Capacidade máxima do buffer de saída.
 * @return Quantidade de tokens gerados, ou -1 em caso de erro.
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

/**
 * @brief Divide texto ou palavras em português brasileiro em suas sílabas ortográficas (Zero Heap).
 * Escreve diretamente no buffer fornecido pelo chamador, sem nenhuma alocação dinâmica.
 * @param text Texto UTF-8 de entrada.
 * @param out_buf Buffer de saída pré-alocado pelo chamador (ex: char buf[512]).
 * @param max_len Tamanho máximo do buffer out_buf.
 * @param sep Caractere separador de sílaba (ex: '-' ou '|').
 * @return Número de bytes gravados no buffer (excluindo o terminador nulo '\0').
 */
size_t phonemas_ptbr_syllabify_buffer(const char* text, char* out_buf, size_t max_len, char sep);

/**
 * @brief Versão de silabificação com alocação dinâmica (malloc) para desktop ou conveniência.
 * @param text Texto UTF-8 de entrada.
 * @param sep Caractere separador de sílaba (ex: '-' ou '|').
 * @return String alocada com malloc (deve ser liberada com phonemas_ptbr_free).
 */
char* phonemas_ptbr_syllabify(const char* text, char sep);

#ifdef __cplusplus
} // extern "C"

#include <string>
#include <string_view>
#include <vector>

namespace phonemas_ptbr {

/**
 * @brief Divide palavras de um texto em suas sílabas ortográficas (ex: "cons-ti-tu-ci-o-nal").
 * Utiliza zero-copy view na entrada e buffer em stack no processamento.
 * @param text Texto de entrada (std::string, const char* ou std::string_view).
 * @param sep Separador de sílabas (padrão '-').
 */
inline std::string syllabify(std::string_view text, char sep = '-') {
    char buf[2048];
    std::string text_str(text);
    phonemas_ptbr_syllabify_buffer(text_str.c_str(), buf, sizeof(buf), sep);
    return std::string(buf);
}

/**
 * @brief Converte texto UTF-8 em português brasileiro para sequência de fonemas IPA.
 * @param text Texto de entrada.
 * @param mode Modo de saída (PHONEMAS_MODE_KOKORO por padrão, ou PHONEMAS_MODE_STANDARD_IPA).
 */
inline std::string text_to_phonemes(std::string_view text, PhonemasOutputMode mode = PHONEMAS_MODE_KOKORO) {
    char buf[2048];
    std::string text_str(text);
    phonemas_ptbr_translate_buffer_mode(text_str.c_str(), buf, sizeof(buf), mode);
    return std::string(buf);
}

/**
 * @brief Converte texto UTF-8 em português brasileiro diretamente para sequência de tokens inteiros.
 * @param text Texto de entrada.
 */
inline std::vector<int64_t> text_to_tokens(std::string_view text) {
    int64_t tokens[512];
    std::string text_str(text);
    int count = phonemas_ptbr_text_to_tokens(text_str.c_str(), tokens, 512);
    if (count <= 0) return {};
    return std::vector<int64_t>(tokens, tokens + count);
}

/**
 * @brief Converte sequência de fonemas IPA diretamente para tokens inteiros.
 * @param phonemes String de fonemas IPA.
 */
inline std::vector<int64_t> phonemes_to_tokens(std::string_view phonemes) {
    int64_t tokens[512];
    std::string ph_str(phonemes);
    int count = phonemas_ptbr_phonemes_to_tokens(ph_str.c_str(), tokens, 512);
    if (count <= 0) return {};
    return std::vector<int64_t>(tokens, tokens + count);
}

} // namespace phonemas_ptbr

#endif // __cplusplus

#endif // PHONEMAS_PTBR_H
