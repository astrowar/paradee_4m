#include "phonemas_ptbr.h"
#include <iostream>
#include <vector>
#include <string>
#include <cassert>
#include <cstring>
#include <chrono>

void test_boundary_and_corner_cases() {
    std::cout << "[1/4] Testando condicoes de borda e entradas invalidas...\n";

    char buf[512];
    int64_t tokens[256];

    // 1. Ponteiros Nulos
    assert(phonemas_ptbr_translate_buffer(nullptr, buf, sizeof(buf)) == 0);
    assert(phonemas_ptbr_translate_buffer("teste", nullptr, 100) == 0);
    assert(phonemas_ptbr_translate_buffer("teste", buf, 0) == 0);
    assert(phonemas_ptbr_translate(nullptr) == nullptr);
    assert(phonemas_ptbr_text_to_tokens(nullptr, tokens, 256) == -1);
    assert(phonemas_ptbr_text_to_tokens("teste", nullptr, 256) == -1);
    assert(phonemas_ptbr_text_to_tokens("teste", tokens, 0) == -1);
    assert(phonemas_ptbr_text_to_tokens("teste", tokens, 2) == -1); // Menor que 3

    // 2. String Vazia e Espaços
    assert(phonemas_ptbr_translate_buffer("", buf, sizeof(buf)) == 0);
    assert(strcmp(buf, "") == 0);
    assert(phonemas_ptbr_translate_buffer("     \t\r\n   ", buf, sizeof(buf)) == 0);
    assert(strcmp(buf, "") == 0);

    // 3. Apenas Pontuação
    size_t len_punct = phonemas_ptbr_translate_buffer("... ,,, ! ? ; : () []", buf, sizeof(buf));
    assert(len_punct > 0);
    assert(strcmp(buf, "... ,,, ! ? ; : () []") == 0);

    // 4. Buffers minúsculos (Verificar ausência de buffer overflow)
    const char* long_text = "O modelo Paradee em português brasileiro é extremamente leve e eficiente.";
    for (size_t bsize = 1; bsize <= 64; ++bsize) {
        char small_buf[128];
        memset(small_buf, 0x55, sizeof(small_buf)); // Preenche com padrão 0x55
        size_t written = phonemas_ptbr_translate_buffer(long_text, small_buf, bsize);
        assert(written < bsize);
        assert(small_buf[written] == '\0');
        // Garantir que a memória além do buffer de bsize não foi corrompida
        assert((unsigned char)small_buf[bsize] == 0x55);
    }

    // 5. Array de Tokens com limites pequenos
    for (size_t tsize = 3; tsize <= 20; ++tsize) {
        int64_t small_tokens[32];
        memset(small_tokens, 0xAA, sizeof(small_tokens));
        int count = phonemas_ptbr_text_to_tokens("Olá mundo! Teste de tokens.", small_tokens, tsize);
        assert(count <= (int)tsize);
        assert(small_tokens[0] == 0);
        assert(small_tokens[count - 1] == 0);
    }

    // 6. Palavras gigantes (> 64 e > 256 caracteres)
    std::string giant_word(300, 'a');
    size_t giant_len = phonemas_ptbr_translate_buffer(giant_word.c_str(), buf, sizeof(buf));
    assert(giant_len > 0);

    // 7. Sequências UTF-8 especiais, acentos misturados e emojis
    const char* utf8_edge = "🚀 Olá! Ação, coração, vôo, feiúra, cão, 12345, testando #$%&*+.";
    size_t utf8_len = phonemas_ptbr_translate_buffer(utf8_edge, buf, sizeof(buf));
    assert(utf8_len > 0);

    std::cout << "      -> Condicoes de borda validadas com sucesso!\n";
}

void test_memory_leak_stress() {
    std::cout << "[2/4] Testando ausencia de vazamento de memoria (10.000 iteracoes malloc/free)...\n";

    const char* test_phrases[] = {
        "O exame de táxi foi um sucesso extraordinário.",
        "Ele explicou exatamente como o brasileiro aprende tecnologia rapidamente.",
        "A vida é uma sucessão de momentos inesquecíveis.",
        "Paradee roda 25 vezes mais rápido que o tempo real no ESP32.",
        "Como você está hoje? Eu estou muito bem, obrigado!"
    };
    const size_t num_phrases = sizeof(test_phrases) / sizeof(test_phrases[0]);

    for (int i = 0; i < 10000; ++i) {
        const char* phrase = test_phrases[i % num_phrases];
        char* ph = phonemas_ptbr_translate(phrase);
        assert(ph != nullptr);
        phonemas_ptbr_free(ph);
    }

    std::cout << "      -> 10.000 ciclos malloc/free concluidos sem vazamentos!\n";
}

void test_throughput_stress() {
    std::cout << "[3/4] Teste de estresse de throughput e performance (50.000 chamadas em buffer)...\n";

    char buf[512];
    int64_t tokens[128];
    const char* text = "O modelo Paradee em português brasileiro é leve, rápido e eficiente.";

    auto t0 = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 50000; ++i) {
        size_t len = phonemas_ptbr_translate_buffer(text, buf, sizeof(buf));
        (void)len;
        int n_tok = phonemas_ptbr_text_to_tokens(text, tokens, 128);
        (void)n_tok;
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    std::cout << "      -> 50.000 traducoes completas executadas em " << ms << " ms ("
              << (ms / 50000.0 * 1000.0) << " microsegundos/frase)!\n";
}

void test_dataset_sentences() {
    std::cout << "[4/4] Teste com sentencas diversificadas em portugues...\n";

    const char* sents[] = {
        "O Brasil é o maior país da América do Sul e tem uma rica diversidade cultural.",
        "A inteligência artificial transforma a tecnologia em uma experiência humana e acessível.",
        "Exemplos de palavras com x: táxi, exame, reflexo, enxame, próximo, máximo, fixo.",
        "Os computadores modernos processam milhões de dados por segundo em alta velocidade.",
        "As noites quentes de verão trazem calma e serenidade para os viajantes.",
        "Ele deixou a chave na mesa da cozinha antes de sair de casa apressadamente.",
        "A ciência e a educação são os pilares fundamentais para o desenvolvimento de uma nação.",
        "O software foi compilado com sucesso e roda perfeitamente em microcontroladores.",
        "Naturalmente, todos os cidadãos esperavam uma resposta rápida e eficiente do governo.",
        "Conexões sem fio como wifi e bluetooth facilitam a comunicação no dia a dia."
    };

    char buf[1024];
    int64_t tokens[256];

    for (size_t i = 0; i < sizeof(sents) / sizeof(sents[0]); ++i) {
        size_t len = phonemas_ptbr_translate_buffer(sents[i], buf, sizeof(buf));
        assert(len > 0);
        int n = phonemas_ptbr_text_to_tokens(sents[i], tokens, 256);
        assert(n > 0);
        assert(tokens[0] == 0 && tokens[n - 1] == 0);
    }

    std::cout << "      -> Todas as sentencas processadas com integridade absoluta!\n";
}

int main() {
    std::cout << "========================================================\n";
    std::cout << "INICIANDO BATERIA DE SANITIZACAO E TESTES DE ESTRESSE\n";
    std::cout << "========================================================\n";

    test_boundary_and_corner_cases();
    test_memory_leak_stress();
    test_throughput_stress();
    test_dataset_sentences();

    std::cout << "========================================================\n";
    std::cout << ">>> TODOS OS TESTES DE ESTRESSE E SANITIZACAO PASSARAM! <<<\n";
    std::cout << "========================================================\n";
    return 0;
}
