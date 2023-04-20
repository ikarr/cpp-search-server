#pragma once
#include <algorithm>
#include <cassert>
#include <cmath>
#include <execution>
#include <functional>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <type_traits>
#include <vector>
#include "concurrent_map.h"
#include "document.h"
#include "string_processing.h"

using namespace std::string_literals;
using matching_result = std::tuple<std::vector<std::string_view>, DocumentStatus>;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double EPSILON = 1e-6;

class SearchServer {
public:
    // конструктор, принимающий стоп-слова в виде контейнера строк
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);
    // конструкторы, принимающие стоп-слова единой строкой
    explicit SearchServer(std::string_view stop_words);
    explicit SearchServer(const std::string& stop_words);

    void AddDocument(int document_id, std::string_view document, DocumentStatus status, const std::vector<int>& ratings);

    // версии FindTopDocuments без политик распараллеливания
    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const;

    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentStatus status) const;

    std::vector<Document> FindTopDocuments(std::string_view raw_query) const;

    // вывод топ-5 документов, соответствующих поисковому запросу
    template<typename ExecutionPolicy, typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(ExecutionPolicy&&, std::string_view raw_query, DocumentPredicate document_predicate) const;

    template<typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(ExecutionPolicy&&, std::string_view raw_query, DocumentStatus status) const;

    template<typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(ExecutionPolicy&&, std::string_view raw_query) const;

    // матчинг документов
    matching_result MatchDocument(std::string_view raw_query, int document_id) const;

    template<typename ExecutionPolicy>
    matching_result MatchDocument(ExecutionPolicy&& policy, std::string_view raw_query, int document_id) const;
    
    // удаление документа с сервера
    void RemoveDocument(int document_id);

    template<typename ExecutionPolicy>
    void RemoveDocument(ExecutionPolicy&& policy, int document_id);
    
    // геттеры
    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;
    
    int GetDocumentCount() const;

    auto begin() const { return document_ids_.begin(); }

    auto end() const { return document_ids_.end(); }
    
private:
    struct Properties {
        int rating;
        DocumentStatus status;
    };
    
    struct QueryWord {
        std::string_view word;
        bool is_minus;
        bool is_stop;
    };
    
    struct Query {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
    };
    
    // поиск всех документов, соответствующих поисковому запросу и предикату
    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const;

    template<typename ExecutionPolicy, typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(ExecutionPolicy&&, const Query& query, DocumentPredicate document_predicate) const;
    
    std::vector<std::string_view> SplitIntoWordsNoStop(std::string_view text) const;
    
    QueryWord ParseQueryWord(std::string_view text) const;
    
    Query ParseQuery(std::string_view text, bool removing_doubles = true) const;

    bool IsStopWord(std::string_view word) const;

    static bool IsValidWord(std::string_view word);
    
    static int ComputeAverageRating(const std::vector<int>& ratings);
    
    double GetIDF(std::string_view word) const;

    std::map<std::string, std::map<int, double>, std::less<>> word_to_document_freqs_;
    std::map<int, std::map<std::string_view, double>> document_to_word_freqs_;
    std::set<std::string, std::less<>> stop_words_;
    std::map<int, Properties> documents_;
    std::set<int> document_ids_;
};

// шаблонный конструктор
template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
    : stop_words_(SplitIntoStrings(stop_words))
{
    if (!std::all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
        throw std::invalid_argument("Some of stop words are invalid"s);
    }
}
// шаблонный метод FindTopDocuments с передачей пользовательского предиката
template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const {
    return FindTopDocuments(std::execution::seq, std::move(raw_query), document_predicate);
}

template<typename ExecutionPolicy, typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, std::string_view raw_query, DocumentPredicate document_predicate) const {
    const auto query = ParseQuery(raw_query);
    auto matched_documents = FindAllDocuments(policy, query, document_predicate);

    std::sort(matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs) {
        return lhs.relevance > rhs.relevance ||
            ((std::abs(lhs.relevance - rhs.relevance) < EPSILON) && lhs.rating > rhs.rating);
        });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }

    return matched_documents;
}

template<typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(policy, raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
        return document_status == status;
    });
}

template<typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, std::string_view raw_query) const {
    return FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL);
}

// шаблонный метод FindAllDocuments с передачей пользовательского предиката
template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {
    return FindAllDocuments(std::execution::seq, query, document_predicate);
}

template<typename ExecutionPolicy, typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(ExecutionPolicy&& policy, const Query& query, DocumentPredicate document_predicate) const {
    ConcurrentMap<int, double> document_to_relevance(std::thread::hardware_concurrency());
    auto plus_words_processing = [&](std::string_view word) {
        if (word_to_document_freqs_.count(word)) {
            const double IDF = GetIDF(word);
            for (const auto [document_id, TF] : word_to_document_freqs_.find(word)->second) {
                const auto& document_data = documents_.at(document_id);
                if (document_predicate(document_id, document_data.status, document_data.rating)) {
                    document_to_relevance[document_id].ref_to_value += TF * IDF;
                }
            }
        }
    };
    std::for_each(policy, query.plus_words.begin(), query.plus_words.end(), plus_words_processing);
    
    auto minus_words_processing = [&](std::string_view word) {
        if (word_to_document_freqs_.count(word)) {
            for (const auto [document_id, freq] : word_to_document_freqs_.find(word)->second) {
                document_to_relevance.Erase(document_id);
            }
        }
    };
    std::for_each(policy, query.minus_words.begin(), query.minus_words.end(), minus_words_processing);
    
    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance.BuildOrdinaryMap()) {
        matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
    }
    return matched_documents;
}

template<typename ExecutionPolicy>
matching_result SearchServer::MatchDocument(ExecutionPolicy&& policy, std::string_view raw_query, int document_id) const {
    // если вызвана последовательная политика - запускается метод без политик
    if (std::is_same_v<std::remove_reference_t<ExecutionPolicy&&>, const std::execution::sequenced_policy>) {
        return MatchDocument(raw_query, document_id);
    }
    // защита от передачи несуществующего id
    if (!document_ids_.count(document_id)) {
        throw std::out_of_range("Requested id "s + std::to_string(document_id) + " is incorrect or doesn't exist"s);
    }
    const auto query = ParseQuery(raw_query, false);
    if (std::any_of(policy,
        query.minus_words.begin(), query.minus_words.end(),
        [&](std::string_view word) {
            auto it = word_to_document_freqs_.find(word);
            return it != word_to_document_freqs_.end() && it->second.count(document_id);
        })) {
        return { {}, documents_.at(document_id).status };
    }
    
    std::vector<std::string_view> matched_words(query.plus_words.size());
    
    auto resize_iterator = std::copy_if(policy,
        query.plus_words.begin(), query.plus_words.end(),
        matched_words.begin(),
        [&](std::string_view word) {
            auto it = word_to_document_freqs_.find(word);
            return it != word_to_document_freqs_.end() && it->second.count(document_id);
        }
    );

    matched_words.resize(resize_iterator - matched_words.begin());
    std::set<std::string_view> unique_words(matched_words.begin(), matched_words.end());
    return { std::vector<std::string_view>(unique_words.begin(), unique_words.end()), documents_.at(document_id).status };
}

template<typename ExecutionPolicy>
void SearchServer::RemoveDocument(ExecutionPolicy&& policy, int document_id) {
    // 1/3: удаление документа из словарей word_to_document_freqs_, document_to_word_freqs_
    // создаём контейнер с произвольным доступом и размером, равным количеству удаляемых слов,
    // и наполняем контейнер указателями на слова документа
    if (document_ids_.find(document_id) == document_ids_.end()) {
        throw std::invalid_argument("Requested id "s + std::to_string(document_id) + " is incorrect or doesn't exist"s);
    }
    auto& word_freqs = document_to_word_freqs_.at(document_id);
    std::vector<const std::string*> words_to_erase(word_freqs.size());
    std::transform(policy,
        word_freqs.begin(), word_freqs.end(),
        words_to_erase.begin(),
        [](const auto& word_freq) {
            return &(word_freq.first);
        }
    );
    // итерируясь по вектору, удаляем записи в словаре word_to_document_freqs_
    std::for_each(policy,
        words_to_erase.begin(), words_to_erase.end(),
        [&, document_id](const std::string* word_ptr) {
            if (word_to_document_freqs_.count(*word_ptr) != 0) {
                word_to_document_freqs_.at(*word_ptr).erase(document_id);
            }
        }
    );
    // 2/3: удаление документа из словаря documents_
    documents_.erase(document_id);
    // 3/3: удаление документа из контейнера id документов
    document_ids_.erase(document_id);
}