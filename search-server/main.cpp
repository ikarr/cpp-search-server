#include <algorithm>
#include <iostream>
#include <set>
#include <map>
#include <string>
#include <utility>
#include <vector>
#include <cmath>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result = 0;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        } else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

struct Document {
    int id;
    double relevance;
};

struct Query {
    set<string> plus_words;
    set<string> minus_words;
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document) {
        if (document.empty()) { return; }
        const vector<string> words = SplitIntoWordsNoStop(document);
        // подсчёт term frequency - частота использования слова в конкретном документе
        double TF = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += TF;
        }
        ++document_count_;
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        const Query query_words = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query_words);
        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                 return lhs.relevance > rhs.relevance;
             });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

private:    
    map<string, map<int, double>> word_to_document_freqs_;
    set<string> stop_words_;
    int document_count_ = 0;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }
    
    bool IsMinusWord(const string& word) const {
        if (word[0] == '-') {
            return true;
        }
        return false;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (IsMinusWord(word) && !IsStopWord(word.substr(1))) {
                words.push_back(word);
            }
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    Query ParseQuery(const string& text) const {
        Query query_words;
        for (const string& word : SplitIntoWordsNoStop(text)) {
            // если word – минус-слово, то – отрезаем '-' и добавляем word в minus_words
            if (IsMinusWord(word)) {
                query_words.minus_words.insert(word.substr(1));
            // иначе если word – плюс-слово и оно отсутствует в minus_words, то добавляем word в plus_words
            } else if (!query_words.minus_words.count(word)) {
                query_words.plus_words.insert(word);
            }
        }
        return query_words;
    }

    vector<Document> FindAllDocuments(const Query& query_words) const {
        // ключ — id найденного документа, а значение — релевантность соответствующего документа
        map<int, double> document_to_relevance;
        vector<Document> matched_documents;
        double IDF;
        // итерирование по плюс-словам
        for (const string& plus_word : query_words.plus_words) {
            if (!word_to_document_freqs_.count(plus_word)) { continue; }
            IDF = log(document_count_ / static_cast<double>(word_to_document_freqs_.at(plus_word).size()));
            for (const auto& [id, tf] : word_to_document_freqs_.at(plus_word)) {
                document_to_relevance[id] += IDF * tf;
            }
        }
        // итерирование по минус-словам
        for (const string& minus_word : query_words.minus_words) {
            if (!word_to_document_freqs_.count(minus_word)) {
                continue;
            }
            // создаём контейнер для хранения ключей, помеченных на удаление
            vector<int> documents_to_delete;
            // помечаем на удаление id документов с минус-словами
            for (const auto& [id, TF] : word_to_document_freqs_.at(minus_word)) {
                documents_to_delete.push_back(id);
            }
            // удаляем из словаря релевантности помеченные id
            for (const auto& id : documents_to_delete) {
                document_to_relevance.erase(id);
            }
        }
        for (const auto& [id, relevance] : document_to_relevance) {
            matched_documents.push_back({id, relevance});
        }
        return matched_documents;
    }
};

SearchServer CreateSearchServer() {
    SearchServer search_server;
    search_server.SetStopWords(ReadLine());
    const int document_count = ReadLineWithNumber();
    for (int document_id = 0; document_id < document_count; ++document_id) {
        search_server.AddDocument(document_id, ReadLine());
    }
    return search_server;
}

int main() {
    const SearchServer search_server = CreateSearchServer();
    const string query = ReadLine();
    for (const auto& [document_id, relevance] : search_server.FindTopDocuments(query)) {
        cout << "{ document_id = "s << document_id << ", "
             << "relevance = "s << relevance << " }"s << endl;
    }
}