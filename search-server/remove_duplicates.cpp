#include "remove_duplicates.h"

void RemoveDuplicates(SearchServer& search_server) {
    std::map<std::set<std::string>, int> words_to_id;
    std::vector<int> ids_to_delete;
    // формирование словаря words_to_id
    for (const int document_id : search_server) {
        std::set<std::string> document_words;
        
        // заполнение множества слов документа
        for (const auto& word_frequencies : search_server.GetWordFrequencies(document_id)) {
            document_words.insert(word_frequencies.first);
        }
        
        // формирование пары {слова_документа, id_документа} c проверкой наличия в словаре дубликата
        if (!words_to_id.count(document_words)) {
            words_to_id[document_words] = document_id;
        } else if (words_to_id[document_words] < document_id) {
            ids_to_delete.push_back(document_id);
        } else {
            ids_to_delete.push_back(words_to_id[document_words]);
        }
    }
    for (const int duplicate_id : ids_to_delete) {
        std::cout << "Found duplicate document id "s << duplicate_id << std::endl;
        search_server.RemoveDocument(duplicate_id);
    }
}