#include <iostream>
#include <curl/curl.h>
#include <fstream>
#include <vector>
#include <algorithm>
#include <set>
#include <thread>
#include <chrono>
#include <zip.h>

const std::string URL = "http://algisothal.ru:8888/cat";
const int NUM_CATS = 12;

// Функция для обработки получения изображения
size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    std::ofstream* out = static_cast<std::ofstream*>(userp);
    out->write(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}

// Функция для загрузки изображения
bool DownloadCatImage(const std::string& filename) {
    CURL* curl;
    CURLcode res;
    std::ofstream outFile(filename, std::ios::binary);
    
    if (!outFile.is_open()) {
        std::cerr << "Не удалось открыть файл для записи: " << filename << std::endl;
        return false;
    }

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, URL.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &outFile);
        res = curl_easy_perform(curl);
        
        curl_easy_cleanup(curl);
    }
    
    outFile.close();
    return true;
}

// Функция для проверки на уникальность изображений
bool IsUniqueImage(const std::set<std::string>& imageSet, const std::string& filename) {
    return imageSet.find(filename) == imageSet.end();
}

// Функция для создания ZIP-архива
void CreateZipArchive(const std::vector<std::string>& images) {
    zip_t* zip = zip_open("cats.zip", ZIP_CREATE | ZIP_TRUNCATE, nullptr);
    if (zip == nullptr) {
        std::cerr << "Не удалось создать ZIP-архив!" << std::endl;
        return;
    }

    for (const auto& image : images) {
        zip_source_t* source = zip_source_file(zip, image.c_str(), 0, 0);
        if (source == nullptr || zip_file_add(zip, image.c_str(), source, ZIP_FL_OVERWRITE) < 0) {
            std::cerr << "Ошибка добавления файла в архив: " << image << std::endl;
            zip_source_free(source);
        }
    }
    
    zip_close(zip);
}

// Основная функция сервиса
int main() {
    std::vector<std::string> catImages;
    std::set<std::string> uniqueImages;

    while (catImages.size() < NUM_CATS) {
        // Задержка от 1 до 15 секунд
        int delay = rand() % 15 + 1;
        std::this_thread::sleep_for(std::chrono::seconds(delay));

        std::string filename = "cat_" + std::to_string(catImages.size()) + ".jpeg";
        if (DownloadCatImage(filename) && IsUniqueImage(uniqueImages, filename)) {
            catImages.push_back(filename);
            uniqueImages.insert(filename);
        } else {
            std::cout << "Не удалось получить уникальное изображение." << std::endl;
        }
    }

    CreateZipArchive(catImages);
    std::cout << "Создан ZIP-архив с котиками." << std::endl;

    return 0;
}