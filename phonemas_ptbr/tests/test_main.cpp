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

    // Teste 6: Preservação de sílabas nasais circunflexas (ân, âm)
    std::string ph6_1 = phonemas_ptbr::text_to_phonemes("infância");
    std::string ph6_2 = phonemas_ptbr::text_to_phonemes("lâmpada");
    std::cout << "Teste 6 (Circunflexo Nasal): \"infância\" -> " << ph6_1 << ", \"lâmpada\" -> " << ph6_2 << "\n";
    assert(ph6_1.find("ɐ̃ŋ") != std::string::npos);
    assert(ph6_2.find("ɐ̃ŋ") != std::string::npos);

    // Teste 7: Acentuação tônica em palavras com dígrafos qu / gu
    std::string ph7_1 = phonemas_ptbr::text_to_phonemes("porque");
    std::string ph7_2 = phonemas_ptbr::text_to_phonemes("pesquisa");
    std::cout << "Teste 7 (Acento em qu/gu): \"porque\" -> " << ph7_1 << ", \"pesquisa\" -> " << ph7_2 << "\n";
    assert(ph7_1.find("pˈoɾ") != std::string::npos);
    assert(ph7_2.find("kˈiz") != std::string::npos);

    // Teste 8: Resolução da letra X (ʃ, ks, s)
    std::string ph8_1 = phonemas_ptbr::text_to_phonemes("abacaxi");
    std::string ph8_2 = phonemas_ptbr::text_to_phonemes("tóxico");
    std::string ph8_3 = phonemas_ptbr::text_to_phonemes("máxima");
    std::cout << "Teste 8 (Letra X): \"abacaxi\" -> " << ph8_1 << ", \"tóxico\" -> " << ph8_2 << ", \"máxima\" -> " << ph8_3 << "\n";
    assert(ph8_1.find("ʃ") != std::string::npos);
    assert(ph8_2.find("ks") != std::string::npos);
    assert(ph8_3.find("s") != std::string::npos);

    // Teste 9: Ditongo nasal ãe e dessonorização de z final
    std::string ph9_1 = phonemas_ptbr::text_to_phonemes("mãe");
    std::string ph9_2 = phonemas_ptbr::text_to_phonemes("luz");
    std::cout << "Teste 9 (ãe e z final): \"mãe\" -> " << ph9_1 << ", \"luz\" -> " << ph9_2 << "\n";
    assert(ph9_1.find("ɐ̃j") != std::string::npos);
    assert(ph9_2.find("lˈus") != std::string::npos);

    // Teste 10: Divisão Silábica Estruturada C++ e C (Zero Heap)
    std::string s1 = phonemas_ptbr::syllabify("constitucional");
    std::string s2 = phonemas_ptbr::syllabify("abacate");
    std::string s3 = phonemas_ptbr::syllabify("carro");
    std::string s4 = phonemas_ptbr::syllabify("rainha");
    std::string s5 = phonemas_ptbr::syllabify("advogado");
    std::string s6 = phonemas_ptbr::syllabify("construir");
    std::cout << "Teste 10 (Silabificacao C++): \"constitucional\" -> " << s1 << "\n";
    std::cout << "                            \"abacate\"        -> " << s2 << "\n";
    std::cout << "                            \"carro\"          -> " << s3 << "\n";
    std::cout << "                            \"rainha\"         -> " << s4 << "\n";
    std::cout << "                            \"advogado\"       -> " << s5 << "\n";
    std::cout << "                            \"construir\"      -> " << s6 << "\n";
    assert(s1 == "cons-ti-tu-ci-o-nal");
    assert(s2 == "a-ba-ca-te");
    assert(s3 == "car-ro");
    assert(s4 == "ra-i-nha");
    assert(s5 == "ad-vo-ga-do");
    assert(s6 == "cons-tru-ir");

    // Teste 10b: API C pura com buffer local estático (Zero Heap)
    char c_syl_buf[128];
    size_t c_syl_len = phonemas_ptbr_syllabify_buffer("O abacaxi e o carro.", c_syl_buf, sizeof(c_syl_buf), '-');
    std::cout << "Teste 10b (API C Buffer Silabas): \"" << c_syl_buf << "\" (" << c_syl_len << " bytes)\n";
    assert(std::string(c_syl_buf) == "O a-ba-ca-xi e o car-ro.");

    // Teste 11: Vozeamento assimilatório de S em coda diante de sonora
    std::string ph11_1 = phonemas_ptbr::text_to_phonemes("mesmo");
    std::string ph11_2 = phonemas_ptbr::text_to_phonemes("desde");
    std::cout << "Teste 11 (Vozeamento de S): \"mesmo\" -> " << ph11_1 << ", \"desde\" -> " << ph11_2 << "\n";
    assert(ph11_1.find("mˈezm") != std::string::npos);
    assert(ph11_2.find("dˈezʤ") != std::string::npos);

    // Teste 12: Epêntese vocálica em consoantes mudas
    std::string ph12_1 = phonemas_ptbr::text_to_phonemes("advogado");
    std::string ph12_2 = phonemas_ptbr::text_to_phonemes("ritmo");
    std::cout << "Teste 12 (Epêntese Vocálica): \"advogado\" -> " << ph12_1 << ", \"ritmo\" -> " << ph12_2 << "\n";
    assert(ph12_1.find("ˌaʤivo") != std::string::npos);
    assert(ph12_2.find("xˈiʧim") != std::string::npos);

    // Teste 13: Desinências nasais -am e -em
    std::string ph13_1 = phonemas_ptbr::text_to_phonemes("falaram");
    std::string ph13_2 = phonemas_ptbr::text_to_phonemes("comem");
    std::cout << "Teste 13 (Nasais Finais): \"falaram\" -> " << ph13_1 << ", \"comem\" -> " << ph13_2 << "\n";
    assert(ph13_1.find("ɐ̃ʊ̃") != std::string::npos);
    assert(ph13_2.find("ẽj") != std::string::npos);

    // Teste 14: Vogais médias abertas em terminações produtivas (/ɛ/ e /ɔ/)
    std::string ph14_1 = phonemas_ptbr::text_to_phonemes("festa");
    std::string ph14_2 = phonemas_ptbr::text_to_phonemes("porta");
    std::cout << "Teste 14 (Vogais Abertas): \"festa\" -> " << ph14_1 << ", \"porta\" -> " << ph14_2 << "\n";
    assert(ph14_1.find("fˈɛst") != std::string::npos);
    assert(ph14_2.find("pˈɔɾt") != std::string::npos);

    // Teste 15: Modo Duplo (Kokoro vs Standard IPA / Dataset)
    std::string ph15_k = phonemas_ptbr::text_to_phonemes("abacate leite sol", PHONEMAS_MODE_KOKORO);
    std::string ph15_ipa = phonemas_ptbr::text_to_phonemes("abacate leite sol", PHONEMAS_MODE_STANDARD_IPA);
    std::cout << "Teste 15 (Modo Kokoro)      : \"abacate leite sol\" -> " << ph15_k << "\n";
    std::cout << "         (Modo Standard IPA): \"abacate leite sol\" -> " << ph15_ipa << "\n";
    assert(ph15_k.find("ˌabakˈaʧy") != std::string::npos);
    assert(ph15_k.find("lˈAʧy") != std::string::npos);
    assert(ph15_k.find("sˈɔʊ") != std::string::npos);
    assert(ph15_ipa.find("ˌabakˈatʃi") != std::string::npos);
    assert(ph15_ipa.find("lˈejtʃi") != std::string::npos);
    assert(ph15_ipa.find("sˈɔw") != std::string::npos);

    // Teste 16: Dicionário Dedicado de Estrangeirismos (Loanwords em Flash)
    std::string ph16_1 = phonemas_ptbr::text_to_phonemes("wifi");
    std::string ph16_2 = phonemas_ptbr::text_to_phonemes("bluetooth");
    std::string ph16_3 = phonemas_ptbr::text_to_phonemes("software");
    std::string ph16_4 = phonemas_ptbr::text_to_phonemes("feedback");
    std::string ph16_5 = phonemas_ptbr::text_to_phonemes("mouse");
    std::cout << "Teste 16 (Estrangeirismos): \"wifi\"      -> " << ph16_1 << "\n";
    std::cout << "                          \"bluetooth\" -> " << ph16_2 << "\n";
    std::cout << "                          \"software\"  -> " << ph16_3 << "\n";
    std::cout << "                          \"feedback\"  -> " << ph16_4 << "\n";
    std::cout << "                          \"mouse\"     -> " << ph16_5 << "\n";
    assert(ph16_1.find("wˌajfˈaj") != std::string::npos);
    assert(ph16_2.find("blˌutˈufi") != std::string::npos);
    assert(ph16_3.find("sˈɔftwɛɾ") != std::string::npos);
    assert(ph16_4.find("fˌidibˈɛki") != std::string::npos);
    assert(ph16_5.find("mˈaws") != std::string::npos);

    // Teste 17: Algarismos Romanos Contextuais (Ordinais <= 10 e Cardinais > 10)
    std::string ph17_1 = phonemas_ptbr::text_to_phonemes("século XXI");
    std::string ph17_2 = phonemas_ptbr::text_to_phonemes("Dom Pedro II");
    std::string ph17_3 = phonemas_ptbr::text_to_phonemes("Capítulo IV");
    std::cout << "Teste 17 (Algarismos Romanos): \"século XXI\"    -> " << ph17_1 << "\n";
    std::cout << "                             \"Dom Pedro II\"  -> " << ph17_2 << "\n";
    std::cout << "                             \"Capítulo IV\"   -> " << ph17_3 << "\n";
    assert(ph17_1.find("vˈiŋʧy") != std::string::npos); // vinte e um
    assert(ph17_2.find("seɡˈũŋdʊ") != std::string::npos); // segundo
    assert(ph17_3.find("kwˈaɾtʊ") != std::string::npos); // quarto

    std::cout << "\n>>> TODOS OS TESTES PASSARAM COM SUCESSO! ZERO ARQUIVOS EXTERNOS. <<<\n";
    return 0;
}
