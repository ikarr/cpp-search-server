#pragma once
#include "search_server.h"

#include <iostream>
#include <string>
#include <vector>

using namespace std::string_literals;

void AssertImpl(bool value, const std::string& str, const std::string& file, const std::string& func, unsigned line, const std::string& hint);

#define ASSERT(expr) AssertImpl(expr, #expr, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_HINT(expr, hint) AssertImpl(expr, #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const std::string& t_str, const std::string& u_str, const std::string& file,
    const std::string& func, unsigned line, const std::string& hint) {
    if (t != u) {
        std::cout << std::boolalpha;
        std::cout << file << "("s << line << "): "s << func << ": "s;
        std::cout << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        std::cout << t << " != "s << u << "."s;
        if (!hint.empty()) {
            std::cout << " Hint: "s << hint;
        }
        std::cout << std::endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

template <typename TestFunc>
void RunTestImpl(const TestFunc& test_func, const std::string& func_name) {
    test_func();
    std::cerr << func_name << " OK"s << std::endl;
}

#define RUN_TEST(func) RunTestImpl((func), #func)

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

void TestSearchServer();