#pragma once
#include <iostream>
#include <stack>
#include <vector>

// шаблонная структура для работы с парами итераторов
template <typename Iterator>
struct IteratorRange {
    Iterator range_begin;
    Iterator range_end;
    
    Iterator GetRangeBegin() const {
        return range_begin;
    }
    Iterator GetRangeEnd() const {
        return range_end;
    }
    size_t GetRangeSize() {
        return (range_end - range_begin);
    }
};

template <typename Iterator>
class Paginator {
public:
    Paginator(Iterator content_begin, Iterator content_end, size_t page_size) {
        auto it = content_begin;
        do {
            // добавляем последнюю страницу и выходим из цикла
            if (distance(it, content_end) <= page_size) {
                documents_per_page_.push_back({it, content_end});
                break;
            }
            // цикл разбивки на страницы
            else {
                auto it_prev = it;
                advance(it, page_size);
                documents_per_page_.push_back({it_prev, it});
            }
            // постусловие «пока остался хотя бы один документ»
        } while (distance(it, content_end) >= 1);
    }
    
    auto begin() const {
        return documents_per_page_.begin();
    }
    auto end() const {
        return documents_per_page_.end();
    }   
    size_t size() {
        return documents_per_page_.size();
    }

private:
    std::vector<IteratorRange<Iterator>> documents_per_page_;
};

// оператор вывода для типа IteratorRange
template <typename Iterator>
std::ostream& operator<<(std::ostream& out, const IteratorRange<Iterator>& range) {
    for (auto it = range.GetRangeBegin(); it != range.GetRangeEnd(); ++it) {
        out << *it;
    }
    return out;
}

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}