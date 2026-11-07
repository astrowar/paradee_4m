#ifndef EMBEDDED_EXCEPTIONS_H
#define EMBEDDED_EXCEPTIONS_H

#include <stddef.h>

/**
 * @brief Tabela essencial e estritamente curada de exceções gramaticais pt-BR.
 * Contém unicamente os clíticos átonos, hiatos consolidados e aberturas vocálicas
 * de altíssima frequência que fogem de regras morfológicas produtivas.
 */
struct ExceptionEntry {
    const char* word;
    const char* phonemes_kokoro;
    const char* phonemes_ipa;
};

// Tabela estática em Flash (.rodata) ordenada para busca binária em O(log N) (96 exceções)
static const struct ExceptionEntry EMBEDDED_EXCEPTIONS[96] = {
    {"ainda", "aˈĩŋdæ", "aˈĩdə"},
    {"amendoim", "ˌamAŋdwˈiŋ", "amẽdwˈĩ"},
    {"aquela", "akˈɛlæ", "akˈɛlə"},
    {"aquelas", "akˈɛlæs", "akˈɛləs"},
    {"aquele", "akˈɛly", "akˈɛli"},
    {"aqueles", "akˈɛlys", "akˈɛlis"},
    {"as", "as", "as"},
    {"bela", "bˈɛlæ", "bˈɛlə"},
    {"belas", "bˈɛlæs", "bˈɛləs"},
    {"belo", "bˈɛlʊ", "bˈɛlw"},
    {"belos", "bˈɛlʊs", "bˈɛlws"},
    {"chefe", "ʃˈɛfy", "ʃˈɛfi"},
    {"chefes", "ʃˈɛfys", "ʃˈɛfis"},
    {"com", "koŋ", "kõ"},
    {"corpo", "kˈɔɾpʊ", "kˈɔɾpw"},
    {"corpos", "kˈɔɾpʊs", "kˈɔɾpws"},
    {"da", "da", "da"},
    {"das", "das", "das"},
    {"de", "ʤj", "di"},
    {"deve", "dˈɛvy", "dˈɛvi"},
    {"devem", "dˈɛvẽj", "dˈɛvẽj"},
    {"do", "dʊ", "dw"},
    {"dos", "dʊs", "dws"},
    {"droga", "drˈɔɡæ", "dɾˈɔgə"},
    {"drogas", "drˈɔɡæs", "dɾˈɔgəs"},
    {"ela", "ˈɛlæ", "ˈɛlə"},
    {"elas", "ˈɛlæs", "ˈɛləs"},
    {"ele", "ˈely", "ˈeli"},
    {"eles", "ˈelys", "ˈelis"},
    {"embora", "Aŋbˈɔɾæ", "ĩbˈɔɾə"},
    {"era", "ˈɛɾæ", "ˈɛɾə"},
    {"espera", "espˈɛɾæ", "ispˈɛɾə"},
    {"essa", "ˈɛsæ", "ˈɛsə"},
    {"essas", "ˈɛsæs", "ˈɛsəs"},
    {"esse", "ˈɛsy", "ˈɛsi"},
    {"esses", "ˈɛsys", "ˈɛsis"},
    {"esta", "ˈɛstæ", "ˈɛstə"},
    {"estas", "ˈɛstæs", "ˈɛstəs"},
    {"este", "ˈɛsʧy", "ˈɛstʃi"},
    {"estes", "ˈɛsʧys", "ˈɛstʃis"},
    {"fora", "fˈɔɾæ", "fˈɔɾə"},
    {"foto", "fˈɔtʊ", "fˈɔtw"},
    {"fotos", "fˈɔtʊs", "fˈɔtws"},
    {"jovem", "ʒˈɔvẽj", "ʒˈɔvẽj"},
    {"jovens", "ʒˈɔvẽjs", "ʒˈɔvẽjs"},
    {"leva", "lˈɛvæ", "lˈɛvə"},
    {"levam", "lˈɛvɐ̃ʊ̃", "lˈɛvɐ̃w̃"},
    {"lhe", "ljy", "ʎi"},
    {"lhes", "ljys", "ʎis"},
    {"maior", "mIˈɔɾ", "majˈɔɾ"},
    {"maiores", "mIˈɔɾys", "majˈɔɾis"},
    {"me", "my", "mi"},
    {"melhor", "meʎˈɔɾ", "meʎˈɔɾ"},
    {"melhores", "meʎˈɔɾys", "meʎˈɔɾis"},
    {"muito", "mwˈiŋtʊ", "mwˈĩtw"},
    {"na", "na", "na"},
    {"nas", "nas", "nas"},
    {"no", "nʊ", "nw"},
    {"nos", "nʊs", "nws"},
    {"nossa", "nˈɔsæ", "nˈɔsə"},
    {"nossas", "nˈɔsæs", "nˈɔsəs"},
    {"nosso", "nˈɔsʊ", "nˈɔsw"},
    {"nossos", "nˈɔsʊs", "nˈɔsws"},
    {"não", "nˌɐ̃ʊ̃", "nɐ̃w̃"},
    {"os", "ʊs", "ws"},
    {"para", "pˌaɾæ", "paɾə"},
    {"pega", "pˈɛɡæ", "pˈɛgə"},
    {"pegam", "pˈɛɡɐ̃ʊ̃", "pˈɛgɐ̃w̃"},
    {"pela", "pˈɛlæ", "pˈɛlə"},
    {"pelas", "pˈɛlæs", "pˈɛləs"},
    {"pelo", "pˈɛlʊ", "pˈɛlw"},
    {"pelos", "pˈɛlʊs", "pˈɛlws"},
    {"perde", "pˈɛɾʤy", "pˈɛɾdʒi"},
    {"perdem", "pˈɛɾdẽj", "pˈɛɾdẽj"},
    {"pior", "piˈɔɾ", "piˈɔɾ"},
    {"piores", "piˈɔɾys", "piˈɔɾis"},
    {"por", "poɾ", "poɾ"},
    {"pra", "pɾa", "pɾa"},
    {"pro", "pɾʊ", "pɾw"},
    {"qualquer", "kwaʊkˈɛɾ", "kwawkˈɛɾ"},
    {"resto", "xˈɛstʊ", "xˈɛstw"},
    {"se", "sy", "si"},
    {"também", "tɐ̃mbˈAŋ", "tɐ̃mbẽj"},
    {"te", "ʧy", "ti"},
    {"telefone", "ˌteli-fˈɔny", "telefˈɔni"},
    {"telefones", "ˌteli-fˈɔnys", "telefˈɔnis"},
    {"um", "ũŋ", "ũ"},
    {"uma", "ˌumæ", "umə"},
    {"umas", "ˌumæs", "uməs"},
    {"uns", "ũŋs", "ũs"},
    {"velha", "vˈɛʎæ", "vˈɛʎə"},
    {"velhas", "vˈɛʎæs", "vˈɛʎəs"},
    {"velho", "vˈɛʎʊ", "vˈɛʎw"},
    {"velhos", "vˈɛʎʊs", "vˈɛʎws"},
    {"volta", "vˈɔʊtæ", "vˈɔwtə"},
    {"voltam", "vˈɔʊtɐ̃ʊ̃", "vˈɔwtɐ̃w̃"},
};

static const size_t EMBEDDED_EXCEPTIONS_SIZE = 96;

#endif // EMBEDDED_EXCEPTIONS_H
