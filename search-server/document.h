#pragma once
#include <iostream>

using namespace std::string_literals;

struct Document {
    // конструктор по умолчанию
    Document();
    // параметризованный конструктор
    Document(int doc_id, double doc_relevance, int doc_rating);
    
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

void PrintDocument(const Document& document);