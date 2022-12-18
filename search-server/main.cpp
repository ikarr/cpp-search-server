#include <algorithm>
#include <iostream>
#include <set>
#include <map>
#include <string>
#include <utility>
#include <vector>
#include <numeric>
#include <cmath>
#include <tuple>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
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
    int rating;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        double TF = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += TF;
        }
        documents_[document_id] = { ComputeAverageRating(ratings), status };
    }

    template <typename DocumentPredicate>
    vector<Document> FindTopDocuments(const string& raw_query, DocumentPredicate document_predicate) const {     
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, document_predicate);
        const double EPSILON = 1e-6;
        sort(matched_documents.begin(), matched_documents.end(),
             [&EPSILON](const Document& lhs, const Document& rhs) {
                 return lhs.relevance > rhs.relevance ||
                     ((abs(lhs.relevance - rhs.relevance) < EPSILON) && lhs.rating > rhs.rating);
             });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const {            
        return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
            return document_status == status;
        });
    }
    
    vector<Document> FindTopDocuments(const string& raw_query) const {            
        return FindTopDocuments(raw_query, [](int document_id, DocumentStatus status, int rating) {
            return status == DocumentStatus::ACTUAL;
        });
    }
    
    int GetDocumentCount() const {
        return documents_.size();
    }
    
    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        const Query query = ParseQuery(raw_query);
        const DocumentStatus document_status = documents_.at(document_id).status;
        vector<string> matched_words;
        // если в запросе есть минус-слово, и оно найдено в документе по заданному id – возвращаем пустой вектор
        for (const auto& minus_word : query.minus_words) {
            if (word_to_document_freqs_.count(minus_word)) {
                if (word_to_document_freqs_.at(minus_word).count(document_id)) {
                    return tuple(matched_words, document_status);
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
        return tuple(matched_words, document_status);
    }
    
private:    
    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    struct Attributes {
        int rating;
        DocumentStatus status;
    };
    
    struct FlaggedWord {
        string word;
        bool IsMinusWord;
        bool NoStopWord;
    };
    
    map<string, map<int, double>> word_to_document_freqs_; // ключ – слово, а значение – id документа и вычисленная TF
    map<int, Attributes> documents_; // ключом в словаре является id документа, а значением — его рейтинг и статус
    set<string> stop_words_;
    
    bool IsMinusWord(const string& word) const {
        if (word[0] == '-') {
            return true;
        }
        return false;
    }

    bool IsStopWord(const string& word) const {
        if (word[0] == '-') {
            return stop_words_.count(word.substr(1)) > 0;
        }
        return stop_words_.count(word) > 0;
    }
        
    FlaggedWord FlagTheWord(const string& word) const {
        return { word, IsMinusWord(word), !IsStopWord(word) };
    }
    
    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) { return 0; }
        int ratings_amount = ratings.size();
        return accumulate(ratings.begin(), ratings.end(), 0) / static_cast<int>(ratings_amount);
    }
    
    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (FlagTheWord(word).NoStopWord) {
                words.push_back(word);
            }
        }
        return words;
    }

    Query ParseQuery(const string& text) const {
        Query query_words;
        for (const string& word : SplitIntoWordsNoStop(text)) {
            // если word – минус-слово, то – отрезаем '-' и добавляем word в minus_words
            if (FlagTheWord(word).IsMinusWord) {
                query_words.minus_words.insert(word.substr(1));
            }
            // иначе если word – плюс-слово и оно отсутствует в minus_words, то добавляем word в plus_words
            else if (!query_words.minus_words.count(word)) {
                query_words.plus_words.insert(word);
            }
        }
        return query_words;
    }
        
    double GetIDF(const string& plus_word) const {
        return log(documents_.size() * 1.0 / static_cast<double>(word_to_document_freqs_.at(plus_word).size()));
    }
    
    template <typename DocumentPredicate>
    vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {
        map<int, double> document_to_relevance;
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
        vector<Document> matched_documents;
        for (const auto& [id, relevance] : document_to_relevance) {
            matched_documents.push_back({id, relevance, documents_.at(id).rating});
        }
        return matched_documents;
    }
};

void PrintDocument(const Document& document) {
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating
         << " }"s << endl;
}

int main() {
    SearchServer search_server;
    search_server.SetStopWords("и в на"s);
    search_server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    search_server.AddDocument(3, "ухоженный скворец евгений"s,         DocumentStatus::BANNED, {9});
    cout << "ACTUAL by default:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s)) {
        PrintDocument(document);
    }
    cout << "BANNED:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::BANNED)) {
        PrintDocument(document);
    }
    cout << "Even ids:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s,
    [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; })) {
        PrintDocument(document);
    }
    return 0;
}