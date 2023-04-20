#include "search_server.h"

SearchServer::SearchServer(std::string_view stop_words)
    : SearchServer(SplitIntoWords(stop_words)) {}

SearchServer::SearchServer(const std::string& stop_words)
    : SearchServer(SplitIntoWords(std::string_view(stop_words))) {}

void SearchServer::AddDocument(int document_id, std::string_view document, DocumentStatus status, const std::vector<int>& ratings) {
    if (document_id < 0) {
        throw std::invalid_argument("Trying to add a document with a negative id"s);
    } else if (documents_.count(document_id) > 0) {
        throw std::invalid_argument("id "s + std::to_string(document_id) + " already exists in the search server"s);
    } else if (!IsValidWord(document)) {
        throw std::invalid_argument("Invalid characters in the text of the added document"s);
    }
    
    const auto words = SplitIntoWordsNoStop(document);
    const double TF = 1.0 / words.size();
    auto& word_freqs = document_to_word_freqs_[document_id];
    
    for (std::string_view word : words) {
        word_to_document_freqs_[std::string(word)][document_id] += TF;
        word_freqs[word_to_document_freqs_.find(std::string(word))->first] += TF;
    }
    documents_.emplace(document_id, Properties{ ComputeAverageRating(ratings), status });
    document_ids_.insert(document_id);
}

std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
        return document_status == status;
    });
}

std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

matching_result SearchServer::MatchDocument(std::string_view raw_query, int document_id) const {
    if (!document_ids_.count(document_id)) {
        throw std::out_of_range("Requested id "s + std::to_string(document_id) + " is incorrect or doesn't exist"s);
    }
    
    const auto query = ParseQuery(raw_query);
    
    if (std::any_of(
        query.minus_words.begin(), query.minus_words.end(),
        [&](std::string_view word) {
            auto it = word_to_document_freqs_.find(word);
            return it != word_to_document_freqs_.end() && it->second.count(document_id);
        })) {
        return { {}, documents_.at(document_id).status };
    }
    
    std::vector<std::string_view> matched_words(query.plus_words.size());
    
    auto resize_iterator = std::copy_if(
        query.plus_words.begin(), query.plus_words.end(),
        matched_words.begin(),
        [&](std::string_view word) {
            auto it = word_to_document_freqs_.find(word);
            return it != word_to_document_freqs_.end() && it->second.count(document_id);
        }
    );
    matched_words.resize(resize_iterator - matched_words.begin());
    return { matched_words, documents_.at(document_id).status };
}

void SearchServer::RemoveDocument(int document_id) {
    if (auto it = document_to_word_freqs_.find(document_id); it != document_to_word_freqs_.end()) {
        document_to_word_freqs_.erase(it);
    }
    for (auto& word_to_id_freq : word_to_document_freqs_) {
        if (auto it = word_to_id_freq.second.find(document_id); it != word_to_id_freq.second.end()) {
            word_to_id_freq.second.erase(it);
        }
    }
    auto it_to_erase = documents_.find(document_id);
    documents_.erase(it_to_erase);
    document_ids_.erase(document_id);
}

const std::map<std::string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
    if (!document_to_word_freqs_.at(document_id).empty()) {
        return document_to_word_freqs_.at(document_id);
    }
    static const std::map<std::string_view, double> no_result_found;
    return no_result_found;
}

int SearchServer::GetDocumentCount() const { return documents_.size(); }

std::vector<std::string_view> SearchServer::SplitIntoWordsNoStop(std::string_view text) const {
    std::vector<std::string_view> words;
    for (std::string_view word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw std::invalid_argument("Invalid word: "s + std::string(word));
        }
        if (!IsStopWord(std::string(word))) {
            words.push_back(word);
        }
    }
    return words;
}

SearchServer::QueryWord SearchServer::ParseQueryWord(std::string_view word) const {
    if (word.empty()) { throw std::invalid_argument("The search server received an empty search request"s); }
    
    bool is_minus = false;
    if (word[0] == '-') {
        is_minus = true;
        word.remove_prefix(1);
    }
    if (word.empty() || word[0] == '-' || !IsValidWord(word)) {
        throw std::invalid_argument("Invalid query word: "s + std::string(word));
    }
    return { word, is_minus, IsStopWord(word) };
}

SearchServer::Query SearchServer::ParseQuery(std::string_view text, bool removing_doubles) const {
    Query query;
    for (std::string_view word : SplitIntoWords(text)) {
        const auto query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            query_word.is_minus
                ? query.minus_words.push_back(query_word.word)
                : query.plus_words.push_back(query_word.word);
        }
    }
    if (removing_doubles) {
        std::sort(query.minus_words.begin(), query.minus_words.end());
        query.minus_words.resize(std::unique(query.minus_words.begin(), query.minus_words.end()) - query.minus_words.begin());
        std::sort(query.plus_words.begin(), query.plus_words.end());
        query.plus_words.resize(std::unique(query.plus_words.begin(), query.plus_words.end()) - query.plus_words.begin());
    }
    return query;
}

bool SearchServer::IsStopWord(std::string_view word) const {
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(std::string_view word) {
    return std::none_of(word.begin(), word.end(), [](char c) { return c >= '\0' && c < ' '; });
}

int SearchServer::ComputeAverageRating(const std::vector<int>& ratings) {
    if (ratings.empty()) { return 0; }
    return std::accumulate(ratings.begin(), ratings.end(), 0) / static_cast<int>(ratings.size());
}

double SearchServer::GetIDF(std::string_view word) const {
    return std::log(GetDocumentCount() * 1.0 / word_to_document_freqs_.find(word)->second.size());
}