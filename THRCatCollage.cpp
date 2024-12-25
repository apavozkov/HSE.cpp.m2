#include <iostream>
#include <curl/curl.h>
#include <fstream>
#include <vector>
#include <set>
#include <thread>
#include <chrono>
#include <zip.h>
#include <sys/stat.h>
#include <mutex>
#include <ctime>

// Константы
const std::string URL = "http://algisothal.ru:8888/cat";
const std::string UPLOAD_URL = "http://algisothal.ru:8888/cat";
const int NUM_CATS = 12;
const int MAX_THREADS = 12;

// Объявляем мьютекс для защиты общего ресурса
std::mutex mutex;

// Функция для получения размера файла
long GetFileSize(const std::string& filename) { // Передаём имя файла
    struct stat stat_buf;
    int rc = stat(filename.c_str(), &stat_buf); // Получаем информацию о файле
    return (rc == 0) ? stat_buf.st_size : -1; // Проверяем, удалось ли что-то получить
}

// Функция для проверки на уникальность изображений по размеру
bool IsUniqueImage(const std::set<long>& imageSizeSet, long size) { 
    return imageSizeSet.find(size) == imageSizeSet.end(); // Сравниваем с размерами других файлов из множества
}

// Функция обрабатывает данные, полученные от cURL
size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    std::ofstream* out = static_cast<std::ofstream*>(userp); 
    out->write(static_cast<char*>(contents), size * nmemb); // Записываем данные в файл
    return size * nmemb;
}

// Функция загружает изображение кота с сервера
bool DownloadCatImage(int index, std::vector<std::string>& catImages, std::set<long>& uniqueImageSizes) {
    std::string timestamp = std::to_string(std::time(nullptr));
    std::string filename = "cat_" + std::to_string(index) + "_" + timestamp + ".jpeg"; // Генерируем уникальное имя файла с индексом и временной метки

    CURL* curl = curl_easy_init(); // Инициализируем curl
    if (curl) {
        {
            std::ofstream outFile(filename, std::ios::binary);
            curl_easy_setopt(curl, CURLOPT_URL, (URL + "?index=" + std::to_string(index)).c_str()); // Настроиваем параметры запроса
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &outFile);

            CURLcode res = curl_easy_perform(curl); // Делаем запрос
            outFile.close(); // Закрываем файл после записи

            if (res != CURLE_OK) {
                std::cerr << "Ошибка при загрузке изображения: " << curl_easy_strerror(res) << std::endl;
                curl_easy_cleanup(curl);
                return false;
            }
        }

        long size = GetFileSize(filename); // Получаем размер файла

        std::lock_guard<std::mutex> guard(mutex);

        // Функция проверяет уникальность размера изображения до добавления
        if (IsUniqueImage(uniqueImageSizes, size)) {
            catImages.push_back(filename);
            uniqueImageSizes.insert(size); // Добавляем в множество уникальных размеров размер скачанного файла
            std::cout << "Получено уникальное изображение: " << filename << std::endl;
        } else {
            std::cout << "Изображение не уникально, удаляется: " << filename << std::endl;
            std::remove(filename.c_str());
        }

        curl_easy_cleanup(curl); // Чистим ресурсы 
    } else {
        std::cerr << "Ошибка инициализации CURL" << std::endl;
        return false;
    }
    return true;
}

// Функция в цикле запускает потоки для загрузки изображений, пока не будет загружено требуемое количество.
void LoadCatImages(std::vector<std::string>& catImages, std::set<long>& uniqueImageSizes) {
    std::vector<std::thread> threads;

    while (catImages.size() < NUM_CATS) {
        while (threads.size() < MAX_THREADS) {
            threads.emplace_back(DownloadCatImage, catImages.size(), std::ref(catImages), std::ref(uniqueImageSizes)); // Запускаем новый поток загрузки
        }

        for (auto& thread : threads) { // Ожидание завершения потоков, если их больше чем максимум
            if (thread.joinable()) {
                thread.join();
            }
        }
        threads.clear(); // Очищаем список потоков
    }
    
    for (auto& thread : threads) { // Ожидаем завершения всех потоков, которые могли остаться
        if (thread.joinable()) {
            thread.join();
        }
    }
}

// Функция для создания ZIP-архива
bool CreateZipArchive(const std::vector<std::string>& images) {
    zip_t* zip = zip_open("cats.zip", ZIP_CREATE | ZIP_TRUNCATE, nullptr); // Создаём или перезаписываем существующий архив
    if (zip == nullptr) { // Обрабатываем ошибку
        std::cerr << "Не удалось создать ZIP-архив!" << std::endl;
        return false;
    }

    for (const auto& image : images) { // В цикле все файлы закидываем в архив
        zip_source_t* source = zip_source_file(zip, image.c_str(), 0, 0);
        if (source == nullptr || zip_file_add(zip, image.c_str(), source, ZIP_FL_OVERWRITE) < 0) { // Если что-то идет не так, обрабатываем ошибку
            std::cerr << "Ошибка добавления файла в архив: " << image << std::endl;
            zip_source_free(source);
        }
    }

    zip_close(zip);
    return true;
}

// Функция для загрузки ZIP-архива по POST
bool postZipArchive(const std::string& zipFileName) {
    CURL* curl;
    CURLcode res;
    FILE* hd_src;
    struct stat file_info;

    if (stat(zipFileName.c_str(), &file_info) != 0) { // Проверяем, существует ли архив
        std::cerr << "Не удалось получить информацию об архиве " << zipFileName << std::endl;
        return false;
    }

    hd_src = fopen(zipFileName.c_str(), "rb"); // Проверяем, открывается ли архив на чтение + открываем архив
    if (!hd_src) {
        std::cerr << "Не удалось открыть архив " << zipFileName << std::endl;
        return false;
    }

    curl_global_init(CURL_GLOBAL_DEFAULT); // Передаём параметры и делаем POST запрос
    curl = curl_easy_init();
    if (curl) {
        struct curl_httppost* formpost = nullptr;
        struct curl_httppost* lastptr = nullptr;

        curl_formadd(&formpost, &lastptr,
            CURLFORM_COPYNAME, "file",
            CURLFORM_FILENAME, zipFileName.c_str(),
            CURLFORM_FILE, zipFileName.c_str(),
            CURLFORM_END);

        curl_easy_setopt(curl, CURLOPT_URL, UPLOAD_URL.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);

        res = curl_easy_perform(curl); // Отправляем POST запрос
        if (res != CURLE_OK) {
            std::cerr << "Ошибка при отправке архива: " << curl_easy_strerror(res) << std::endl;
        }
        curl_formfree(formpost);
        curl_easy_cleanup(curl);
    } else {
        std::cerr << "Ошибка инициализации CURL" << std::endl;
        fclose(hd_src);
        curl_global_cleanup();
        return false;
    }

    fclose(hd_src);
    curl_global_cleanup();  // Чистим curl и закрываем архив
    return true;
}

// Основная функция
int main() {
    std::vector<std::string> catImages;  // Инициализируем векторы
    std::set<long> uniqueImageSizes;

    LoadCatImages(catImages, uniqueImageSizes);

    std::lock_guard<std::mutex> guard(mutex);
    
    if (CreateZipArchive(catImages)) {
        std::cout << "Создан ZIP-архив с котиками." << std::endl;
    } else {
        std::cout << "Ошибка при создании ZIP-архива с котиками." << std::endl;
    }

    if (postZipArchive("cats.zip")) {
        std::cout << "Архив успешно загружен!" << std::endl;
    } else {
        std::cout << "Ошибка при загрузке архива." << std::endl;
    }

    return 0;
}
