// Решите загадку: Сколько чисел от 1 до 1000 содержат как минимум одну цифру 3?
// Напишите ответ здесь:
#include <iostream>
#include <string>
#include <vector>

using namespace std;

// создаём числовой ряд
vector<int> GetNumberLine(int lower_thold, int upper_thold) {
	vector<int> NumLine;
	for (int i = lower_thold; i <= upper_thold; ++i) {
		NumLine.push_back(i);
	}
	return NumLine;
}

// полученный вектор чисел преобразовываем в вектор строк
vector<string> ConvertToVString(const vector<int>& NumLine) {
	vector<string> StringNumLine;
	for (int number : NumLine) {
		StringNumLine.push_back(to_string(number));
	}
	return StringNumLine;
}

// ищем в векторе строк как минимум одну цифру 3; формируем счётчик
int SearchForThree(const vector<string>& StringNumLine) {
	int counter = 0;
	for (string number : StringNumLine) {
		for (char digit : number) {
			if (digit == '3') {
				++counter;
				break;
			}
		}
	}
	return counter;
}

// проверяем результат: согласно поисковой выдаче, должно получиться 271 :)
int main() {
	int result = SearchForThree(ConvertToVString(GetNumberLine(1, 1000)));
	cout << result << endl;
}
// Закомитьте изменения и отправьте их в свой репозиторий.
