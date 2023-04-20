#pragma once
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "log_duration.h"
#include "search_server.h"

using namespace std;

void AssertImpl(bool value, const string& str, const string& file, const string& func, unsigned line, const string& hint);

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

// ===START OF BENCHMARK CODE===

string GenerateWord(mt19937& generator, int max_length);

vector<string> GenerateDictionary(mt19937& generator, int word_count, int max_length);

string GenerateQuery(mt19937& generator, const vector<string>& dictionary, int word_count, double minus_prob);

vector<string> GenerateQueries(mt19937& generator, const vector<string>& dictionary, int query_count, int max_word_count);

template <typename ExecutionPolicy>
void Test(string_view mark, const SearchServer& search_server, const vector<string>& queries, ExecutionPolicy&& policy) {
    LOG_DURATION(mark);
    double total_relevance = 0;
    for (const string_view query : queries) {
        for (const auto& document : search_server.FindTopDocuments(policy, query)) {
            total_relevance += document.relevance;
        }
    }
    cout << total_relevance << endl;
}

#define TEST(policy) Test(#policy, search_server, queries, execution::policy)

// ===END OF BENCHMARK CODE===

// Тест №1 проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent();

// Тест №2 проверяет, что документы с минус-словами исключаются из поисковой выдачи
void TestExcludeDocumentsWithMinusWords();

// Тест №3 проверяет матчинг документов - соответствие документов запросу
void TestMatchingDocuments();

// Тест №4 проверяет корректность сортировки поисковой выдачи по убыванию релевантности
// Тест №4 также проверяет корректное вычисление релевантности найденных документов
void TestCalcAndSortInDescOrder();

// Тест №5 проверяет вычисление рейтинга документов
void TestCalculateRatingOfAddedDocumentContent();

// Тест №6 проверяет фильтрацию результатов поиска при помощи предиката, задаваемого пользователем
// Тест №6 также проверяет нахождение документов в соответствии с заданным статусом
void TestFilteringResultsByUserDefinedPredicate();

// Бенчмарк для измерения времени работы методов
void Benchmark();

void TestSearchServer();