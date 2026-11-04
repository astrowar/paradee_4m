import json

# 1. Carregar Vocabulário de Tokens do Paradee
vocab_path = "../models/paradee_ptbr_alex/config.json"
vocab_data = json.load(open(vocab_path, encoding="utf-8"))["vocab"]

# Ordenar por tamanho do símbolo decrescente para coincidir prefixos mais longos primeiro
sorted_vocab = sorted(vocab_data.items(), key=lambda x: (-len(x[0].encode("utf-8")), x[0]))

with open("include/embedded_vocab.h", "w", encoding="utf-8") as f:
    f.write("#ifndef EMBEDDED_VOCAB_H\n#define EMBEDDED_VOCAB_H\n\n")
    f.write("#include <stdint.h>\n#include <stddef.h>\n\n")
    f.write("struct VocabEntry {\n    const char* symbol;\n    int64_t id;\n};\n\n")
    f.write(f"// Vocabulário de tokens Kokoro/Paradee pt-BR ({len(sorted_vocab)} símbolos)\n")
    f.write(f"static const struct VocabEntry EMBEDDED_VOCAB[{len(sorted_vocab)}] = {{\n")
    for sym, token_id in sorted_vocab:
        # Escapar para C string
        escaped_sym = sym.replace("\\", "\\\\").replace('"', '\\"')
        f.write(f'    {{"{escaped_sym}", {token_id}}},\n')
    f.write("};\n\n")
    f.write(f"static const size_t EMBEDDED_VOCAB_SIZE = {len(sorted_vocab)};\n\n")
    f.write("#endif // EMBEDDED_VOCAB_H\n")

print(f"Gerado include/embedded_vocab.h ({len(sorted_vocab)} símbolos)")

# 2. Carregar Dicionário de Exceções
exceptions_data = json.load(open("exceptions_ptbr.json", encoding="utf-8"))
sorted_exceptions = sorted(exceptions_data.items(), key=lambda x: x[0])

with open("include/embedded_exceptions.h", "w", encoding="utf-8") as f:
    f.write("#ifndef EMBEDDED_EXCEPTIONS_H\n#define EMBEDDED_EXCEPTIONS_H\n\n")
    f.write("#include <stddef.h>\n\n")
    f.write("struct ExceptionEntry {\n    const char* word;\n    const char* phonemes;\n};\n\n")
    f.write(f"// Dicionário ordenado para busca binária em Flash ({len(sorted_exceptions)} palavras irregulares)\n")
    f.write(f"static const struct ExceptionEntry EMBEDDED_EXCEPTIONS[{len(sorted_exceptions)}] = {{\n")
    for word, ph in sorted_exceptions:
        escaped_w = word.replace("\\", "\\\\").replace('"', '\\"')
        escaped_ph = ph.replace("\\", "\\\\").replace('"', '\\"')
        f.write(f'    {{"{escaped_w}", "{escaped_ph}"}},\n')
    f.write("};\n\n")
    f.write(f"static const size_t EMBEDDED_EXCEPTIONS_SIZE = {len(sorted_exceptions)};\n\n")
    f.write("#endif // EMBEDDED_EXCEPTIONS_H\n")

print(f"Gerado include/embedded_exceptions.h ({len(sorted_exceptions)} exceções)")
