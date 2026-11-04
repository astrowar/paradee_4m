#include "phonemas_ptbr.h"
#include <iostream>
#include <cassert>
#include <cstring>

int main() {
    std::cout << "Iniciando testes unitarios de phonemas_ptbr (Modulo Autocontido)...\n";

    // Teste 1: Fonetizacao basica
    std::string text1 = "Olá mundo!";
    std::string ph1 = phonemas_ptbr::text_to_phonemes(text1);
    std::cout << "Teste 1 (Texto -> Fonemas): \"" << text1 << "\" -> " << ph1 << "\n";
    assert(!ph1.empty());
    assert(ph1.find("olˈa") != std::string::npos);

    // Teste 2: Silabificacao e acentuacao secundaria
    std::string text2 = "explicar exatamente";
    std::string ph2 = phonemas_ptbr::text_to_phonemes(text2);
    std::cout << "Teste 2 (Acento Secundario): \"" << text2 << "\" -> " << ph2 << "\n";
    assert(ph2.find("ˌesplikˈar") != std::string::npos);
    assert(ph2.find("ˌezatæmˈAŋʧy") != std::string::npos);

    // Teste 3: Tokens C++ (100% embutidos em Flash, zero arquivos externos)
    std::vector<int64_t> tokens = phonemas_ptbr::text_to_tokens("Paradee");
    std::cout << "Teste 3 (Tokens C++ Flash): \"Paradee\" -> " << tokens.size() << " tokens\n";
    assert(tokens.size() >= 3);
    assert(tokens.front() == 0 && tokens.back() == 0); // Pads [0]

    // Teste 4: API C puro para microcontroladores (zero heap, buffer local)
    int64_t c_buffer[64];
    int count = phonemas_ptbr_text_to_tokens("Paradee", c_buffer, 64);
    std::cout << "Teste 4 (API C Puro Buffer): \"Paradee\" -> " << count << " tokens gravados\n";
    assert(count == (int)tokens.size());
    assert(c_buffer[0] == 0 && c_buffer[count - 1] == 0);

    // Teste 5: C string translation
    char* c_ph = phonemas_ptbr_translate("Bom dia!");
    assert(c_ph != nullptr);
    std::cout << "Teste 5 (C Translate): \"Bom dia!\" -> " << c_ph << "\n";
    phonemas_ptbr_free(c_ph);

    std::cout << "\n>>> TODOS OS TESTES PASSARAM COM SUCESSO! ZERO ARQUIVOS EXTERNOS. <<<\n";
    return 0;
}
