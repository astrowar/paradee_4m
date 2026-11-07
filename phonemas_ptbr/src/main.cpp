#include "phonemas_ptbr.h"

#include <iostream>
#include <string>
#include <vector>

void print_help(const char* prog) {
    std::cout << "Uso: " << prog << " \"Texto em português brasileiro\" [opções]\n\n"
              << "Opções:\n"
              << "  --ipa, -i          Utiliza modo IPA padrão (compatível com a norma e datasets linguísticos)\n"
              << "  --no-tokens        Exibe apenas os fonemas e sílabas, ocultando os tokens numéricos\n"
              << "  --help, -h         Exibe esta mensagem de ajuda\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_help(argv[0]);
        return 1;
    }

    std::string text;
    bool show_tokens = true;
    PhonemasOutputMode mode = PHONEMAS_MODE_KOKORO;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            print_help(argv[0]);
            return 0;
        } else if (arg == "--no-tokens") {
            show_tokens = false;
        } else if (arg == "--ipa" || arg == "-i" || arg == "--standard-ipa") {
            mode = PHONEMAS_MODE_STANDARD_IPA;
        } else if (text.empty() && arg.rfind("--", 0) != 0) {
            text = arg;
        }
    }

    if (text.empty()) {
        std::cerr << "Erro: Nenhum texto fornecido para fonetização.\n";
        return 1;
    }

    std::string syllables = phonemas_ptbr::syllabify(text, '-');
    std::string phonemes = phonemas_ptbr::text_to_phonemes(text, mode);

    std::cout << "========================================\n";
    std::cout << "Texto de Entrada : " << text << "\n";
    std::cout << "Divisão Silábica : " << syllables << "\n";
    if (mode == PHONEMAS_MODE_STANDARD_IPA) {
        std::cout << "Fonemas pt-BR (IPA Padrão): " << phonemes << "\n";
    } else {
        std::cout << "Fonemas pt-BR (Kokoro)    : " << phonemes << "\n";
    }
    std::cout << "========================================\n";

    if (show_tokens) {
        std::vector<int64_t> tokens = phonemas_ptbr::text_to_tokens(text);
        std::cout << "Tokens Paradee (" << tokens.size() << " tokens embutidos em Flash):\n[";
        for (size_t i = 0; i < tokens.size(); ++i) {
            std::cout << tokens[i] << (i + 1 < tokens.size() ? ", " : "");
        }
        std::cout << "]\n========================================\n";
    }

    return 0;
}
