#include "process_queries.h"

std::vector<std::vector<Document>> ProcessQueries(const SearchServer& search_server,
                                                  const std::vector<std::string>& queries) {
    
    std::vector<std::vector<Document>> matched_documents(queries.size());
    std::transform(
        std::execution::par,
        queries.begin(), queries.end(),
        matched_documents.begin(),
        [&search_server](auto& query) {
            return search_server.FindTopDocuments(query);
        }
        );
    
    return matched_documents;
}

std::list<Document> ProcessQueriesJoined(const SearchServer& search_server,
                                         const std::vector<std::string>& queries) {
    
    std::list<Document> matched_documents;
    for (const auto& documents : ProcessQueries(search_server, queries)) {
        for (const Document& document : documents) {
            matched_documents.insert(matched_documents.end(), document);
        }
    }
    
    return matched_documents;
}