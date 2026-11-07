import json
import os

script_dir = os.path.dirname(os.path.abspath(__file__))
json_path = os.path.join(script_dir, "loanwords_ptbr.json")
header_path = os.path.join(script_dir, "../include/embedded_loanwords.h")

data = json.load(open(json_path, encoding="utf-8"))
sorted_words = sorted(data.items(), key=lambda x: x[0])

with open(header_path, "w", encoding="utf-8") as f:
    f.write("#ifndef EMBEDDED_LOANWORDS_H\n")
    f.write("#define EMBEDDED_LOANWORDS_H\n\n")
    f.write("#include <stddef.h>\n\n")
    f.write("/**\n")
    f.write(" * @brief Entrada de palavra estrangeira / empréstimo linguístico (loanword).\n")
    f.write(" * Armazena a transcrição tanto para o vocabulário neural do Kokoro/Paradee\n")
    f.write(" * quanto para o padrão linguístico canônico (Standard IPA).\n")
    f.write(" */\n")
    f.write("struct LoanwordEntry {\n")
    f.write("    const char* word;\n")
    f.write("    const char* phonemes_kokoro;\n")
    f.write("    const char* phonemes_ipa;\n")
    f.write("};\n\n")
    f.write(f"// Dicionário ordenado em Flash (.rodata) para busca binária em O(log N) ({len(sorted_words)} estrangeirismos)\n")
    f.write(f"static const struct LoanwordEntry EMBEDDED_LOANWORDS[{len(sorted_words)}] = {{\n")
    for word, entries in sorted_words:
        esc_w = word.replace("\\", "\\\\").replace('"', '\\"')
        esc_k = entries["kokoro"].replace("\\", "\\\\").replace('"', '\\"')
        esc_ipa = entries["ipa"].replace("\\", "\\\\").replace('"', '\\"')
        f.write(f'    {{"{esc_w}", "{esc_k}", "{esc_ipa}"}},\n')
    f.write("};\n\n")
    f.write(f"static const size_t EMBEDDED_LOANWORDS_SIZE = {len(sorted_words)};\n\n")
    f.write("#endif // EMBEDDED_LOANWORDS_H\n")

print(f"Gerado com sucesso: {header_path} ({len(sorted_words)} estrangeirismos)")
