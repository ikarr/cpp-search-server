#include "search_server.h"
#include "string_processing.h"

SearchServer::SearchServer(const std::string& stop_words) : SearchServer(SplitIntoWords(stop_words)) {}

void SearchServer::AddDocument(int document_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings) {
    if (document_id < 0) {
        throw std::invalid_argument("Попытка добавить документ с отрицательным id"s);
    } else if (documents_.count(document_id) > 0) {
        throw std::invalid_argument("Попытка добавить документ c id ранее добавленного документа"s);
    } else if (!IsValidWord(document)) {
        throw std::invalid_argument("Наличие недопустимых символов в тексте добавляемого документа"s);
    } else {
        const std::vector<std::string> words = SplitIntoWordsNoStop(document);
        double TF = 1.0 / words.size();
        for (const std::string& word : words) {
            word_to_document_freqs_[word][document_id] += TF;
            document_to_word_freqs_[document_id][word] += TF;
        }
        documents_[document_id] = { ComputeAverageRating(ratings), status };
        document_ids_.insert(document_id);
    }
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query, DocumentStatus status) const {
    return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
        return document_status == status;
    });
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query) const {
    return FindTopDocuments(raw_query, [](int document_id, DocumentStatus status, int rating) {
        return status == DocumentStatus::ACTUAL;
    });
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

std::tuple<std::vector<std::string>, DocumentStatus> SearchServer::MatchDocument(const std::string& raw_query, int document_id) const {
    const Query query = ParseQuery(raw_query);
    std::vector<std::string> matched_words;
    // если в запросе есть минус-слово, и оно найдено в документе по заданному id – возвращаем пустой вектор
    for (const auto& minus_word : query.minus_words) {
        if (word_to_document_freqs_.count(minus_word)) {
            if (word_to_document_freqs_.at(minus_word).count(document_id)) {
                return std::tuple{matched_words, documents_.at(document_id).status};
            }
        }
    }
    // если в индексе есть плюс-слово и оно имеется в документе с данным id – добавляем его в вектор
    for (const auto& plus_word : query.plus_words) {
        if (word_to_document_freqs_.count(plus_word)) {
            if (word_to_document_freqs_.at(plus_word).count(document_id)) {
                matched_words.push_back(plus_word);
            }
        }
    }
    sort(matched_words.begin(), matched_words.end());
    return std::tuple{matched_words, documents_.at(document_id).status};
}

const std::map<std::string, double>& SearchServer::GetWordFrequencies(int document_id) const {
    if (!document_to_word_freqs_.at(document_id).empty()) {
        return document_to_word_freqs_.at(document_id);
    }
    static const std::map<std::string, double> no_result_found;
    return no_result_found;
}

void SearchServer::RemoveDocument(int document_id) {
    // 1/4: удаление документа из словаря document_to_word_freqs_
    if (auto it = document_to_word_freqs_.find(document_id); it != document_to_word_freqs_.end()) {
        document_to_word_freqs_.erase(it);
    }
    
    // 2/4: удаление документа из словаря word_to_document_freqs_
    for (std::pair<const std::string, std::map<int, double>>& word_to_id_freq : word_to_document_freqs_) {
        if (std::map<int, double>::const_iterator it = word_to_id_freq.second.find(document_id); it != word_to_id_freq.second.end()) {
            word_to_id_freq.second.erase(it);
        }
    }
    
    // 3/4: удаление документа из словаря documents_
    auto it_to_erase = documents_.find(document_id);
    documents_.erase(it_to_erase);
    
    // 4/4: удаление документа из множества document_ids_
    document_ids_.erase(document_id);
}

std::set<int>::iterator SearchServer::begin() {
    return document_ids_.begin();
}

std::set<int>::iterator SearchServer::end() {
    return document_ids_.end();
}

bool SearchServer::IsMinusWord(const std::string& word) const {
    return word[0] == '-';
}

bool SearchServer::IsStopWord(const std::string& word) const {
    if (word[0] == '-') {
        return stop_words_.count(word.substr(1)) > 0;
    }
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(const std::string& word) {
    return (none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
    })
            && !word.empty() && !(word == "-"s) && !(word[0] == '-' && word[1] == '-'));
}

SearchServer::QueryWord SearchServer::ParseQueryWord(const std::string& word) const {
    return { word, IsMinusWord(word), IsStopWord(word), IsValidWord(word) };
}

std::vector<std::string> SearchServer::SplitIntoWordsNoStop(const std::string& text) const {
    std::vector<std::string> words;
    for (const std::string& word : SplitIntoWords(text)) {
        if (!ParseQueryWord(word).is_stop) {
            words.push_back(word);
        }
    }
    return words;
}

SearchServer::Query SearchServer::ParseQuery(const std::string& text) const {
    Query query;
    for (const std::string& word : SplitIntoWordsNoStop(text)) {
        if (ParseQueryWord(word).is_valid) {
            if (ParseQueryWord(word).is_minus) {
                query.minus_words.insert(word.substr(1));
            } else if (!query.minus_words.count(word)) {
                query.plus_words.insert(word);
            }
        } else if (!none_of(word.begin(), word.end(), [](char c) {
            return c >= '\0' && c < ' ';
        })) {
            throw std::invalid_argument("В словах поискового запроса есть недопустимые символы"s);
        } else if (word[0] == '-' && word[1] == '-') {
            throw std::invalid_argument("Наличие более чем одного минуса перед словами в запросе"s);
        } else if (word == "-"s) {
            throw std::invalid_argument("Отсутствие текста после символа «минус» в поисковом запросе"s);
        }
    }
    return query;
}

int SearchServer::ComputeAverageRating(const std::vector<int>& ratings) {
    if (ratings.empty()) { return 0; }
    return std::accumulate(ratings.begin(), ratings.end(), 0)
        / static_cast<int>(ratings.size());
}

double SearchServer::GetIDF(const std::string& plus_word) const {
    return log(documents_.size() * 1.0 / static_cast<double>(word_to_document_freqs_.at(plus_word).size()));
}