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

// Реализация класса SearchServer

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
        }
        else {
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
            matched_documents.push_back({ id, relevance, documents_.at(id).rating });
        }
        return matched_documents;
    }
};

// Реализация макросов ASSERT, ASSERT_EQUAL, ASSERT_EQUAL_HINT, ASSERT_HINT и RUN_TEST

void AssertImpl(bool value, const string& str, const string& file, const string& func, unsigned line, const string& hint) {
    if (!value) {
        cout << file << "("s << line << "): "s << func << ": "s;
        if (!hint.empty()) {
            cout << "ASSERT("s << str << ") failed. Hint: "s << hint;
        }
        else {
            cout << "ASSERT("s << str << ") failed."s;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT(expr) AssertImpl(expr, #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl(expr, #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
    const string& func, unsigned line, const string& hint) {
    if (t != u) {
        cout << boolalpha;
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cout << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

template <typename TestFunc>
void RunTestImpl(const TestFunc& test_func, const string& func_name) {
    test_func();
    cerr << func_name << " OK"s << endl;
}

#define RUN_TEST(func) RunTestImpl((func), #func)

// -------- Начало модульных тестов поисковой системы ----------

// Тест №1 проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(),
            "Stop words must be excluded from documents"s);
    }
}

// Тест №2 проверяет, что документы с минус-словами исключаются из поисковой выдачи
void TestExcludeDocumentsWithMinusWords() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        // Тест запроса с одним минус-словом
        const auto found_docs1 = server.FindTopDocuments("cat in the -city"s);
        // Тест запроса, в котором одно и то же слово является плюс- и минус-словом
        const auto found_docs2 = server.FindTopDocuments("cat -cat in the city"s);
        // Тест запроса, в котором все слова запроса являются минус-словами
        const auto found_docs3 = server.FindTopDocuments("-cat -in -the -city"s);
        ASSERT_EQUAL_HINT(found_docs1.size(), 0u,
            "This document has a word, which marked as a minus-word in the test query"s);
        ASSERT_EQUAL_HINT(found_docs2.size(), 0u,
            "This document has a word, which marked as a minus-word in the test query"s);
        ASSERT_EQUAL_HINT(found_docs3.size(), 0u,
            "No matching words in this document for the test query"s);
    }
}

// Тест №3 проверяет матчинг документов - соответствие документов запросу
void TestMatchingDocuments() {
    SearchServer server;
    server.AddDocument(0, "white cat and long tail"s, DocumentStatus::ACTUAL, { 8, -3 });
    server.AddDocument(1, "fluffy cat fluffy tail"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    server.SetStopWords("and"s);
    // Тест матчинга документа одновременно с обработкой стоп-слов
    const auto [words1, status1] = server.MatchDocument("cat and tail"s, 0);
    ASSERT_EQUAL_HINT(words1.size(), 2u,
        "Invalid number of matched words. Please check the logic of the class methods MatchDocument and SetStopWords"s);
    // Тест матчинга документа с минус-словом в поисковом запросе
    const auto [words2, status2] = server.MatchDocument("-fluffy cat"s, 1);
    ASSERT_EQUAL_HINT(words2.size(), 0u,
        "This document has a word, which marked as a minus-word in the test query"s);
}

// Тест №4 проверяет корректность сортировки поисковой выдачи по убыванию релевантности
// Тест №4 также проверяет корректное вычисление релевантности найденных документов
void TestCalcAndSortInDescOrder() {
    SearchServer server;
    server.SetStopWords("and"s);
    server.AddDocument(0, "white cat and long tail"s, DocumentStatus::ACTUAL, { 8, -3 });
    server.AddDocument(1, "fluffy cat fluffy tail"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    server.AddDocument(2, "well-groomed dog talking eyes"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    const auto found_docs = server.FindTopDocuments("well-groomed cat"s);
    // убеждаемся, что найдено три документа
    ASSERT_EQUAL_HINT(found_docs.size(), 3u,
        "Invalid number of documents found"s);
    // убеждаемся, что документы размещены в порядке убывания их релевантности: документ №2 - №1 - №0,
    // и что если релевантность одинакова - то сортировка происходит в порядке убывания рейтинга
    const vector<int>& docs_ids = { found_docs[0].id, found_docs[1].id, found_docs[2].id };
    const vector<int> expected_ids = { 2, 1, 0 };
    for (int i = 0; i < docs_ids.size(); i++) {
        ASSERT_EQUAL_HINT(docs_ids[i], expected_ids[i],
        "Invalid order of search results. Documents must been placed in descending order of relevance or average rating"s);
    }
    // убеждаемся, что релевантность подсчитана корректно
    const vector<double>& docs_relevance = { found_docs[0].relevance, found_docs[1].relevance, found_docs[2].relevance };
    const vector<double> expected_relevance = { 0.274653, 0.101366, 0.101366 };
    const double EPSILON = 1e-6;
    for (int i = 0; i < docs_relevance.size(); i++) {
        ASSERT_HINT(abs(docs_relevance[i] - expected_relevance[i]) < EPSILON,
        "Wrong calculation of document relevance. Make sure that TF and IDF are calculated correctly"s);
    }
}

// Тест №5 проверяет вычисление рейтинга документов
void TestCalculateRatingOfAddedDocumentContent() {
    SearchServer server;
    server.SetStopWords("and"s);
    server.AddDocument(0, "white cat and long tail"s, DocumentStatus::ACTUAL, { 8, -3 });
    server.AddDocument(1, "fluffy cat fluffy tail"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    server.AddDocument(2, "well-groomed dog talking eyes"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    const auto found_docs = server.FindTopDocuments("fluffy well-groomed cat"s);
    // убеждаемся, что найдено три документа
    ASSERT_EQUAL_HINT(found_docs.size(), 3u,
        "Invalid number of documents found"s);
    // убеждаемся, что релевантность подсчитана корректно
    const vector<int>& docs_rating = { found_docs[0].rating, found_docs[1].rating, found_docs[2].rating };
    const vector<int> expected_rating = { 5, -1, 2};
    for (size_t i = 0; i < docs_rating.size(); i++) {
        ASSERT_EQUAL_HINT(docs_rating[i], expected_rating[i],
        "Wrong calculation of document rating"s);
    }
}

// Тест №6 проверяет фильтрацию результатов поиска при помощи предиката, задаваемого пользователем
// Тест №6 также проверяет нахождение документов в соответствии с заданным статусом
void TestFilteringResultsByUserDefinedPredicate() {
    SearchServer server;
    server.SetStopWords("and"s);
    server.AddDocument(0, "white cat and long tail"s, DocumentStatus::ACTUAL, { 8, -3 });
    server.AddDocument(1, "fluffy cat fluffy tail"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    server.AddDocument(2, "well-groomed dog talking eyes"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    server.AddDocument(3, "well-groomed starling Eugene"s, DocumentStatus::BANNED, { 9 });

    // убеждаемся, что по умолчанию найдено три документа со статусом ACTUAL
    const auto found_docs_default = server.FindTopDocuments("fluffy well-groomed cat"s);
    ASSERT_EQUAL_HINT(found_docs_default.size(), 3u,
        "Invalid number of documents found"s);

    // убеждаемся, что найден один документ со статусом BANNED
    const auto found_docs_banned = server.FindTopDocuments("fluffy well-groomed cat"s,
        [](int document_id, DocumentStatus status, int rating) { return status == DocumentStatus::BANNED; });
    ASSERT_EQUAL_HINT(found_docs_banned.size(), 1u,
        "Invalid number of found documents with this status"s);

    // убеждаемся, что найдено два документа с чётными id
    const auto found_docs_even_ids = server.FindTopDocuments("fluffy well-groomed cat"s,
        [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; });
    ASSERT_EQUAL_HINT(found_docs_even_ids.size(), 2u,
        "Invalid number of found documents filteted using a user-defined predicate"s);
}

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestExcludeDocumentsWithMinusWords);
    RUN_TEST(TestMatchingDocuments);
    RUN_TEST(TestCalcAndSortInDescOrder);
    RUN_TEST(TestCalculateRatingOfAddedDocumentContent);
    RUN_TEST(TestFilteringResultsByUserDefinedPredicate);
}

// --------- Окончание модульных тестов поисковой системы -----------

int main() {
    TestSearchServer();
    // Если вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;
}