#include "document.h"

Document::Document() = default;

Document::Document(int doc_id, double doc_relevance, int doc_rating) {
    id = doc_id;
    relevance = doc_relevance;
    rating = doc_rating;
}

std::ostream& operator<<(std::ostream& out, const Document& document) {
    out << "{ "s << "document_id = "s << document.id
        << ", "s << "relevance = "s << document.relevance
        << ", "s << "rating = "s << document.rating << " }"s;
    return out;
}