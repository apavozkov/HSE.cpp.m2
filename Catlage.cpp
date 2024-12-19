#include <iostream>
#include <curl/curl.h> // Либа для хттп
#include <opencv2/opencv.hpp> // Либа для обработки картинок
#include <zip.h>
#include <vector>
#include <set>
#include <thread>
#include <chrono>
#include <random>

const std::string URL = "http://algisothal.ru:8888/cat"; // Источник котиков
const int NUM_CATS = 12; // Кол-во котиков за раз

// Обрабатываем входящие данные
size_t in_data_pro(void* contents, size_t size, size_t nmemb, std::vector<unsigned char>& buffer) { // Определяем размер 1 элемента; кол-во элементов; 
    size_t total_size = size * nmemb;  // Вычисляем общий размер передачи
    buffer.insert(buffer.end(), (unsigned char*)contents, (unsigned char*)contents + total_size); // Данные добавляются в вектор
    return total_size; 
}

// Получаем картинку котика с сервера
std::vector<unsigned char> fetch_cat() { 
    CURL* curl; // Создаём переменные
    CURLcode res;
    std::vector<unsigned char> buffer;

    curl_global_init(CURL_GLOBAL_DEFAULT); // Инициализируем либу
    curl = curl_easy_init(); // Создаём новый экземпляр curl
    if(curl) { // Настраиваем параметры
        curl_easy_setopt(curl, CURLOPT_URL, URL.c_str()); // Куда запрос
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, in_data_pro); // Запись данных через функцию
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer); // Куда сохранять
        res = curl_easy_perform(curl); // Получаем данные
        curl_easy_cleanup(curl); // Чистим ресурсы
    }
    curl_global_cleanup(); // Грохаем либу

    return buffer; // Возвращаем вектор с данными картинки
}

// Сохраняем полученного кота 
void save_cat(const std::string& filename, const std::vector<unsigned char>& image_data) { // Передаём имя файла и вектор с данными картинки для декодировки
    cv::Mat img = cv::imdecode(image_data, cv::IMREAD_COLOR); // Пытается декодировть изображение 
    if (img.empty()) { // Обработка ошибки, если не произошло декодирование
        std::cerr << "Не удалось декодировать изображение" << std::endl;
        return;
    }
    cv::imwrite(filename, img); // Сохраняем декодированную картинку в файл
}

// Проверяем на повторы
bool duplicate(const std::set<std::string>& cat_hashes, const std::string& hash) { // Сравниваем хэш текущего котика с остальными
    return cat_hashes.find(hash) != cat_hashes.end(); // Проверяем ответ от find
}

// Делаем коллаж
void make_collage(const std::vector<std::string>& cat_filenames) { // Передаём вектор с именами файлов с котиками
    const int collage_width = 1200; // Устанавливаем размеры коллажа
    const int collage_height = 800;
    cv::Mat collage(collage_height, collage_width, CV_8UC3, cv::Scalar(255, 255, 255)); // Передаём параметры для двумерного массива и цветовые параметры
    
    int index = 0; // Ставим счётчик для перебора картинок 
    int cat_width = collage_width / 4; // Задаём параметры каждой картинки в коллаже
    int cat_height = collage_height / 3;

    for (int i = 0; i < 3; ++i) { // Перебираем
        for (int j = 0; j < 4; ++j) {
            if (index < cat_filenames.size()) {
                cv::Mat img = cv::imread(cat_filenames[index]);
                cv::resize(img, img, cv::Size(cat_width, cat_height));
                img.copyTo(collage(cv::Rect(j * cat_width, i * cat_height, cat_width, cat_height)));
                index++;
            }
        }
    }

    cv::imwrite("collage.jpg", collage);
}

void zip_collage(const std::vector<std::string>& cat_filenames) {
    int err = 0;
    zip_t* zip = zip_open("cats.zip", ZIP_CREATE | ZIP_TRUNCATE, &err);

    for (const auto& filename : cat_filenames) {
        zip_file_t* zf = zip_fopen(zip, filename.c_str(), ZIP_FL_UNCHANGED);
        zip_fclose(zf);
    }

    zip_close(zip);
}

int main() {
    std::set<std::string> unique_cats;
    std::vector<std::string> cat_filenames;

    while (cat_filenames.size() < NUM_CATS) {
        std::this_thread::sleep_for(std::chrono::seconds(1 + rand() % 15));

        auto image_data = fetch_cat();
        std::string hash = std::to_string(std::hash<std::vector<unsigned char>>{}(image_data));

        if (!duplicate(unique_cats, hash)) {
            unique_cats.insert(hash);
            std::string filename = "cat_" + std::to_string(cat_filenames.size()) + ".jpg";
            save_cat(filename, image_data);
            cat_filenames.push_back(filename);
        }
    }

    make_collage(cat_filenames);

    std::this_thread::sleep_for(std::chrono::seconds(1 + rand() % 15));
    zip_collage(cat_filenames);

    std::cout << "Коллаж из " << NUM_CATS << " котиков успешно создан и сохранён в cats.zip." << std::endl;

    return 0;
}
