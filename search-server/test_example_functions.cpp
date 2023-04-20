#include "test_example_functions.h"

void AssertImpl(bool value, const std::string& str, const std::string& file, const std::string& func, unsigned line, const std::string& hint) {
    if (!value) {
        std::cout << file << "("s << line << "): "s << func << ": "s;
        if (!hint.empty()) {
            std::cout << "ASSERT("s << str << ") failed. Hint: "s << hint;
        }
        else {
            std::cout << "ASSERT("s << str << ") failed."s;
        }
        std::cout << std::endl;
        abort();
    }
}

// ===START OF BENCHMARK CODE===

string GenerateWord(mt19937& generator, int max_length) {
    const int length = uniform_int_distribution(1, max_length)(generator);
    string word;
    word.reserve(length);
    for (int i = 0; i < length; ++i) {
        word.push_back(uniform_int_distribution('a', 'z')(generator));
    }
    return word;
}

vector<string> GenerateDictionary(mt19937& generator, int word_count, int max_length) {
    vector<string> words;
    words.reserve(word_count);
    for (int i = 0; i < word_count; ++i) {
        words.push_back(GenerateWord(generator, max_length));
    }
    words.erase(unique(words.begin(), words.end()), words.end());
    return words;
}

string GenerateQuery(mt19937& generator, const vector<string>& dictionary, int word_count, double minus_prob = 0) {
    string query;
    for (int i = 0; i < word_count; ++i) {
        if (!query.empty()) {
            query.push_back(' ');
        }
        if (uniform_real_distribution<>(0, 1)(generator) < minus_prob) {
            query.push_back('-');
        }
        query += dictionary[uniform_int_distribution<int>(0, dictionary.size() - 1)(generator)];
    }
    return query;
}

vector<string> GenerateQueries(mt19937& generator, const vector<string>& dictionary, int query_count, int max_word_count) {
    vector<string> queries;
    queries.reserve(query_count);
    for (int i = 0; i < query_count; ++i) {
        queries.push_back(GenerateQuery(generator, dictionary, max_word_count));
    }
    return queries;
}

// ===END OF BENCHMARK CODE===

void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const std::string content = "cat in the city"s;
    const std::vector<int> ratings = { 1, 2, 3 };
    {
        SearchServer server("at"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(),
            "Stop words must be excluded from documents"s);
    }
}

void TestExcludeDocumentsWithMinusWords() {
    const int doc_id = 42;
    const std::string content = "cat in the city"s;
    const std::vector<int> ratings = { 1, 2, 3 };
    {
        SearchServer server("at"s);
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

void TestMatchingDocuments() {
    SearchServer server("and"s);
    server.AddDocument(0, "white cat and long tail"s, DocumentStatus::ACTUAL, { 8, -3 });
    server.AddDocument(1, "fluffy cat fluffy tail"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    // Тест матчинга документа одновременно с обработкой стоп-слов
    const auto [words1, status1] = server.MatchDocument("cat and tail"s, 0);
    ASSERT_EQUAL_HINT(words1.size(), 2u,
        "Invalid number of matched words. Please check the logic of the class methods MatchDocument and SetStopWords"s);
    // Тест матчинга документа с минус-словом в поисковом запросе
    const auto [words2, status2] = server.MatchDocument("-fluffy cat"s, 1);
    ASSERT_EQUAL_HINT(words2.size(), 0u,
        "This document has a word, which marked as a minus-word in the test query"s);
}

void TestCalcAndSortInDescOrder() {
    SearchServer server("and"s);
    server.AddDocument(0, "white cat and long tail"s, DocumentStatus::ACTUAL, { 8, -3 });
    server.AddDocument(1, "fluffy cat fluffy tail"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    server.AddDocument(2, "well-groomed dog talking eyes"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    const auto found_docs = server.FindTopDocuments("well-groomed cat"s);
    // убеждаемся, что найдено три документа
    ASSERT_EQUAL_HINT(found_docs.size(), 3u,
        "Invalid number of documents found"s);
    // убеждаемся, что документы размещены в порядке убывания их релевантности: документ №2 - №1 - №0,
    // и что если релевантность одинакова - то сортировка происходит в порядке убывания рейтинга
    const std::vector<int>& docs_ids = { found_docs[0].id, found_docs[1].id, found_docs[2].id };
    const std::vector<int> expected_ids = { 2, 1, 0 };
    for (int i = 0; i < docs_ids.size(); i++) {
        ASSERT_EQUAL_HINT(docs_ids[i], expected_ids[i],
        "Invalid order of search results. Documents must been placed in descending order of relevance or average rating"s);
    }
    // убеждаемся, что релевантность подсчитана корректно
    const std::vector<double>& docs_relevance = { found_docs[0].relevance, found_docs[1].relevance, found_docs[2].relevance };
    const std::vector<double> expected_relevance = { 0.274653, 0.101366, 0.101366 };
    const double EPSILON = 1e-6;
    for (int i = 0; i < docs_relevance.size(); i++) {
        ASSERT_HINT(std::abs(docs_relevance[i] - expected_relevance[i]) < EPSILON,
        "Wrong calculation of document relevance. Make sure that TF and IDF are calculated correctly"s);
    }
}

void TestCalculateRatingOfAddedDocumentContent() {
    SearchServer server("and"s);
    server.AddDocument(0, "white cat and long tail"s, DocumentStatus::ACTUAL, { 8, -3 });
    server.AddDocument(1, "fluffy cat fluffy tail"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    server.AddDocument(2, "well-groomed dog talking eyes"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    const auto found_docs = server.FindTopDocuments("fluffy well-groomed cat"s);
    // убеждаемся, что найдено три документа
    ASSERT_EQUAL_HINT(found_docs.size(), 3u,
        "Invalid number of documents found"s);
    // убеждаемся, что релевантность подсчитана корректно
    const std::vector<int>& docs_rating = { found_docs[0].rating, found_docs[1].rating, found_docs[2].rating };
    const std::vector<int> expected_rating = { 5, -1, 2};
    for (size_t i = 0; i < docs_rating.size(); i++) {
        ASSERT_EQUAL_HINT(docs_rating[i], expected_rating[i],
        "Wrong calculation of document rating"s);
    }
}

void TestFilteringResultsByUserDefinedPredicate() {
    SearchServer server("and"s);
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

void Benchmark() {
    mt19937 generator;
    const auto dictionary = GenerateDictionary(generator, 1000, 10);
    const auto documents = GenerateQueries(generator, dictionary, 10'000, 70);
    SearchServer search_server(dictionary[0]);
    for (size_t i = 0; i < documents.size(); ++i) {
        search_server.AddDocument(i, documents[i], DocumentStatus::ACTUAL, {1, 2, 3});
    }
    const auto queries = GenerateQueries(generator, dictionary, 100, 70);
    TEST(seq);
    TEST(par);
}

void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestExcludeDocumentsWithMinusWords);
    RUN_TEST(TestMatchingDocuments);
    RUN_TEST(TestCalcAndSortInDescOrder);
    RUN_TEST(TestCalculateRatingOfAddedDocumentContent);
    RUN_TEST(TestFilteringResultsByUserDefinedPredicate);
    RUN_TEST(Benchmark);
}