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
    // конструктор по умолчанию
    Document() = default;
    // параметризованный конструктор
    Document(int doc_id, double doc_relevance, int doc_rating) {
        id = doc_id;
        relevance = doc_relevance;
        rating = doc_rating;
    }
    int id = 0;
    double relevance = 0.0;
    int rating = 0;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED
};

class SearchServer {
public:
    // параметризованный конструктор, принимающий стоп-слова в виде произвольной коллекции строк
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words) {
        for (const string& word : stop_words) {
            if (word == ""s || stop_words_.count(word)) {
                continue;
            }
            else if (none_of(word.begin(), word.end(), [](char c) {
                return c >= '\0' && c < ' ';
            })) {
                stop_words_.insert(word);
            } else {
                throw invalid_argument("Недопустимые символы в стоп-слове: "s + word);
            }
        }
    }
    // параметризованный конструктор, принимающий стоп-слова в виде строки
    explicit SearchServer(const string& stop_words) : SearchServer(SplitIntoWords(stop_words)) {}
    
    void AddDocument(int document_id, const string& document, DocumentStatus status, const vector<int>& ratings) {
        if (document_id < 0) {
            throw invalid_argument("Попытка добавить документ с отрицательным id"s);
        } else if (documents_.count(document_id) > 0) {
            throw invalid_argument("Попытка добавить документ c id ранее добавленного документа"s);
        } else if (!IsValidWord(document)) {
            throw invalid_argument("Наличие недопустимых символов в тексте добавляемого документа"s);
        } else {
            const vector<string> words = SplitIntoWordsNoStop(document);
            double TF = 1.0 / words.size();
            for (const string& word : words) {
                word_to_document_freqs_[word][document_id] += TF;
            }
            documents_[document_id] = { ComputeAverageRating(ratings), status };
            index_to_id_.insert({ index_to_id_.size(), document_id });
        }
    }

    template <typename DocumentPredicate>
    vector<Document> FindTopDocuments(const string& raw_query, DocumentPredicate document_predicate) const {
        const Query query = ParseQuery(raw_query);
        vector<Document> matched_documents;
        FindAllDocuments(query, matched_documents, document_predicate);
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
    
    int GetDocumentId(int index) const {
        if ((index > index_to_id_.size()) || (index < 0)) {
            throw out_of_range("Индекс переданного документа выходит за пределы допустимого диапазона"s);
        }
        return index_to_id_.at(index);
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        const Query query = ParseQuery(raw_query);
        vector<string> matched_words;
        // если в запросе есть минус-слово, и оно найдено в документе по заданному id – возвращаем пустой вектор
        for (const auto& minus_word : query.minus_words) {
            if (word_to_document_freqs_.count(minus_word)) {
                if (word_to_document_freqs_.at(minus_word).count(document_id)) {
                    return tuple{matched_words, documents_.at(document_id).status};
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
        return tuple{matched_words, documents_.at(document_id).status};
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

    struct QueryWord {
        string word;
        bool is_minus;
        bool is_stop;
        bool is_valid;
    };

    map<string, map<int, double>> word_to_document_freqs_; // ключ – слово, а значение – id документа и вычисленная TF
    map<int, Attributes> documents_; // ключ - id документа, значением — его рейтинг и статус
    set<string> stop_words_;
    map<size_t, int> index_to_id_; // ключ - порядковый индекс документа, значение - id документа

    bool IsMinusWord(const string& word) const {
        return word[0] == '-';
    }

    bool IsStopWord(const string& word) const {
        if (word[0] == '-') {
            return stop_words_.count(word.substr(1)) > 0;
        }
        return stop_words_.count(word) > 0;
    }

    static bool IsValidWord(const string& word) {
        return (none_of(word.begin(), word.end(), [](char c) {
            return c >= '\0' && c < ' ';
            })
        && !word.empty() && !(word == "-"s) && !(word[0] == '-' && word[1] == '-'));
    }

    QueryWord ParseQueryWord(const string& word) const {
        return { word, IsMinusWord(word), IsStopWord(word), IsValidWord(word) };
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!ParseQueryWord(word).is_stop) {
                words.push_back(word);
            }
        }
        return words;
    }

    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWordsNoStop(text)) {
            if (ParseQueryWord(word).is_valid) {
                if (ParseQueryWord(word).is_minus) {
                    query.minus_words.insert(word.substr(1));
                } else if (!query.minus_words.count(word)) {
                    query.plus_words.insert(word);
                }
            } else if (!none_of(word.begin(), word.end(), [](char c) {
                return c >= '\0' && c < ' ';
            })) {
                throw invalid_argument("В словах поискового запроса есть недопустимые символы"s);
            } else if (word[0] == '-' && word[1] == '-') {
                throw invalid_argument("Наличие более чем одного минуса перед словами в запросе"s);
            } else if (word == "-"s) {
                throw invalid_argument("Отсутствие текста после символа «минус» в поисковом запросе"s);
            }
        }
        return query;
    }

    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) { return 0; }
        return accumulate(ratings.begin(), ratings.end(), 0)
            / static_cast<int>(ratings.size());
    }
    
    double GetIDF(const string& plus_word) const {
        return log(documents_.size() * 1.0 / static_cast<double>(word_to_document_freqs_.at(plus_word).size()));
    }

    template <typename DocumentPredicate>
    void FindAllDocuments(const Query& query, vector<Document>& result, DocumentPredicate document_predicate) const {
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
        for (const auto& [id, relevance] : document_to_relevance) {
            result.push_back({ id, relevance, documents_.at(id).rating });
        }
    }
};