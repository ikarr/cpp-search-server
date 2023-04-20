#include "string_processing.h"

std::vector<std::string_view> SplitIntoWords(std::string_view str) {
    std::vector<std::string_view> result;
    
    // обработка строк без слов
    if (str.empty() || str.find_first_not_of(' ') == str.npos) {
        return result;
    }
    
    // обработка строк из одного слова
    if (str.find_first_of(' ') == str.npos) {
        result.push_back(str.substr(0));
        return result;
    }
    
    // удаление пробелов перед первым словом
    if (str.find(' ') == 0) {
        str.remove_prefix(std::min(str.size(), str.find_first_not_of(' ')));
    }

    while (str.find_first_not_of(' ') != str.npos) {
        size_t space = str.find(' ');
        
        // последнее слово в строке
        if (space == str.npos) {
            result.push_back(str.substr(0));
            return result;
        }
        else {
            result.push_back(str.substr(0, space));
            str.remove_prefix(std::min(str.size(), str.find_first_not_of(' ', space)));
        }
    }
    
    return result;
}