#include <iostream>
#include <curl/curl.h>
#include <fstream>
#include <vector>
#include <algorithm>
#include <set>
#include <thread>
#include <chrono>
#include <zip.h>
#include <sys/stat.h>

const std::string URL = "http://algisothal.ru:8888/cat";
const std::string UPLOAD_URL = "http://algisothal.ru:8890/cat";
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

// Функция для получения размера файла
long GetFileSize(const std::string& filename) {
    struct stat stat_buf;
    int rc = stat(filename.c_str(), &stat_buf);
    return rc == 0 ? stat_buf.st_size : -1;
}

// Функция для проверки на уникальность изображений по размеру
bool IsUniqueImage(const std::set<long>& imageSizeSet, const std::string& filename) {
    long size = GetFileSize(filename);
    return imageSizeSet.find(size) == imageSizeSet.end();
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

// Функция для загрузки ZIP-архива по POST
bool postZipArchive(const std::string& zipFileName) {
    CURL* curl;
    CURLcode res;
    FILE* hd_src;
    struct stat file_info;

    if (stat(zipFileName.c_str(), &file_info) != 0) {
	std::cerr << "Не удалось получить информацию об архиве " << zipFileName << std::endl;
	return false;
    }

    hd_src = fopen(zipFileName.c_str(), "rb");
    if (!hd_src) {
	std::cerr << "Не удалось открыть архив " << zipFileName << std::endl;
	return false;
    }
    
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    if(curl) {
	struct curl_httppost* formpost = nullptr;
	struct curl_httppost* lastptr = nullptr;

	curl_formadd(&formpost, &lastptr,
		CURLFORM_COPYNAME, "file",
		CURLFORM_FILENAME, zipFileName.c_str(),
		CURLFORM_FILE, zipFileName.c_str(),
		CURLFORM_END);

	curl_easy_setopt(curl, CURLOPT_URL, UPLOAD_URL.c_str());
	curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);

	res = curl_easy_perform(curl);

        if(res != CURLE_OK) {
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
    curl_global_cleanup();
    return true;
}

// Основная функция сервиса
int main() {
    std::vector<std::string> catImages;
    std::set<long> uniqueImageSizes;

    while (catImages.size() < NUM_CATS) {
        // Задержка от 1 до 15 секунд
        int delay = rand() % 15 + 1;
        std::this_thread::sleep_for(std::chrono::seconds(delay));

        std::string filename = "cat_" + std::to_string(catImages.size()) + ".jpeg";
        if (DownloadCatImage(filename) && IsUniqueImage(uniqueImageSizes, filename)) {
            catImages.push_back(filename);
            uniqueImageSizes.insert(GetFileSize(filename));
            std::cout << "Получено уникальное изображение." << std::endl;
        } else {
            std::cout << "Не удалось получить уникальное изображение." << std::endl;
        }
    }

    CreateZipArchive(catImages);
    std::cout << "Создан ZIP-архив с котиками." << std::endl;

    // Загружаем ZIP-архив
    if (postZipArchive("cats.zip")) {
        std::cout << "Архив успешно загружен!" << std::endl;
    } else {
        std::cout << "Ошибка при загрузке архива." << std::endl;
    }

    return 0;
}
