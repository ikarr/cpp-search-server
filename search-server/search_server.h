#pragma once
#include "document.h"

#include <algorithm>
#include <numeric>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <tuple>
#include <cmath>
#include <stdexcept>

using namespace std::string_literals;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

class SearchServer {
public:
    // параметризованный конструктор, принимающий стоп-слова в виде произвольной коллекции строк
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);
    // параметризованный конструктор, принимающий стоп-слова в виде строки
    explicit SearchServer(const std::string& stop_words);
    
    void AddDocument(int document_id, const std::string& document, DocumentStatus status,
                     const std::vector<int>& ratings);

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::string& raw_query,
                                           DocumentPredicate document_predicate) const;

    std::vector<Document> FindTopDocuments(const std::string& raw_query,
                                           DocumentStatus status) const;
    
    std::vector<Document> FindTopDocuments(const std::string& raw_query) const;
    
    int GetDocumentCount() const;
    
    int GetDocumentId(int index) const;
    
    std::tuple<std::vector<std::string>, DocumentStatus>
        MatchDocument(const std::string& raw_query, int document_id) const;

private:
    struct Query {
        std::set<std::string> plus_words;
        std::set<std::string> minus_words;
    };

    struct Attributes {
        int rating;
        DocumentStatus status;
    };

    struct QueryWord {
        std::string word;
        bool is_minus;
        bool is_stop;
        bool is_valid;
    };

    std::map<std::string, std::map<int, double>> word_to_document_freqs_; // ключ – слово, а значение – id документа и вычисленная TF
    std::map<int, Attributes> documents_; // ключ - id документа, значением — его рейтинг и статус
    std::set<std::string> stop_words_;
    std::map<size_t, int> index_to_id_; // ключ - порядковый индекс документа, значение - id документа

    bool IsMinusWord(const std::string& word) const;
    
    bool IsStopWord(const std::string& word) const;
    
    static bool IsValidWord(const std::string& word);

    QueryWord ParseQueryWord(const std::string& word) const;

    std::vector<std::string> SplitIntoWordsNoStop(const std::string& text) const;

    Query ParseQuery(const std::string& text) const;

    static int ComputeAverageRating(const std::vector<int>& ratings);
    
    double GetIDF(const std::string& plus_word) const;

    template <typename DocumentPredicate>
    void FindAllDocuments(const Query& query, std::vector<Document>& result,
                          DocumentPredicate document_predicate) const;
};

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words) {
    for (const std::string& word : stop_words) {
        if (word == ""s || stop_words_.count(word)) {
            continue;
        } else if (none_of(word.begin(), word.end(), [](char c) {
            return c >= '\0' && c < ' ';
        })) {
            stop_words_.insert(word);
        } else {
            throw std::invalid_argument("Недопустимые символы в стоп-слове: "s + word);
        }
    }
}
    
template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query,
                                                     DocumentPredicate document_predicate) const {
    const Query query = ParseQuery(raw_query);
    std::vector<Document> matched_documents;
    FindAllDocuments(query, matched_documents, document_predicate);
    const double EPSILON = 1e-6;
    sort(matched_documents.begin(), matched_documents.end(),
         [&EPSILON](const Document& lhs, const Document& rhs) {
             return lhs.relevance > rhs.relevance ||
                 ((std::abs(lhs.relevance - rhs.relevance) < EPSILON) && lhs.rating > rhs.rating);
         });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return matched_documents;
}

template <typename DocumentPredicate>
void SearchServer::FindAllDocuments(const Query& query, std::vector<Document>& result,
                                    DocumentPredicate document_predicate) const {
    std::map<int, double> document_to_relevance;
    for (const auto& plus_word : query.plus_words) {
        if (!word_to_document_freqs_.count(plus_word)) { continue; }
        double idf = GetIDF(plus_word);
        for (const auto& [id, tf] : word_to_document_freqs_.at(plus_word)) {
            const auto& document_data = documents_.at(id);
            if (document_predicate(id, document_data.status, document_data.rating)) {
                document_to_relevance[id] += tf * idf;
            }
        }
    }
    for (const auto& minus_word : query.minus_words) {
        if (!word_to_document_freqs_.count(minus_word)) { continue; }
        for (const auto& [id, TF] : word_to_document_freqs_.at(minus_word)) {
            document_to_relevance.erase(id);
        }
    }
    for (const auto& [id, relevance] : document_to_relevance) {
        result.push_back({ id, relevance, documents_.at(id).rating });
    }
}