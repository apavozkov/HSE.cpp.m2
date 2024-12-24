#include <iostream>
#include <curl/curl.h> // Либа для хттп
#include <fstream> // Либа для файловых потоков
#include <vector>
#include <set>
#include <thread> // Либа для потоков
#include <chrono> // Либа для задержек
#include <zip.h> // ЛИба для архивов
#include <sys/stat.h>

// Константы
const std::string URL = "http://algisothal.ru:8888/cat"; // Адрес для get
const std::string UPLOAD_URL = "http://algisothal.ru:8890/cat"; // Адрес для post
const int NUM_CATS = 12; // Количество котиков в наборе

// Функция для обработки получения изображения
size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) { // Принимаем указатель на данные, их размер и количество элементов
    std::ofstream* out = static_cast<std::ofstream*>(userp);
    out->write(static_cast<char*>(contents), size * nmemb); // Записываем данные в файл
    return size * nmemb;
}

// Функция для загрузки изображения
bool DownloadCatImage(const std::string& filename) { // Передаём имя файла для записи
    CURL* curl;
    CURLcode res;
    std::ofstream outFile(filename, std::ios::binary); // Открываем файл

    if (!outFile.is_open()) { // Проверяем, открылся ли файл
        std::cerr << "Не удалось открыть файл для записи: " << filename << std::endl;
        return false;
    }

    curl_global_init(CURL_GLOBAL_DEFAULT); // Инициализируем либу
    curl = curl_easy_init(); // Создаём новый экземпляр
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, URL.c_str()); // Передаём параметры
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback); 
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &outFile);
        res = curl_easy_perform(curl); // Делаем запрос
        curl_easy_cleanup(curl); // Чистим ресурсы
    }

    outFile.close(); // Закрываем файл
    return true; 
}

// Функция для получения размера файла
long GetFileSize(const std::string& filename) { // Передаём имя файла
    struct stat stat_buf; 
    int rc = stat(filename.c_str(), &stat_buf); // Получаем информацию о файле
    return rc == 0 ? stat_buf.st_size : -1; // Проверяем, удалось ли что-то получить
}

// Функция для проверки на уникальность изображений по размеру
bool IsUniqueImage(const std::set<long>& imageSizeSet, const std::string& filename) {
    long size = GetFileSize(filename); // Получаем размер файла
    return imageSizeSet.find(size) == imageSizeSet.end(); // Сравниваем с размерами других файлов из множества
}

// Функция для создания ZIP-архива
void CreateZipArchive(const std::vector<std::string>& images) {
    zip_t* zip = zip_open("cats.zip", ZIP_CREATE | ZIP_TRUNCATE, nullptr); // Создаём или перезаписываем существующий архив
    if (zip == nullptr) { // Обрабатываем ошибку
        std::cerr << "Не удалось создать ZIP-архив!" << std::endl;
        return;
    }

    for (const auto& image : images) { // В цикле все файлы закидываем в архив
        zip_source_t* source = zip_source_file(zip, image.c_str(), 0, 0);
        if (source == nullptr || zip_file_add(zip, image.c_str(), source, ZIP_FL_OVERWRITE) < 0) { // Если что-то идет не так, обрабатываем ошибку
            std::cerr << "Ошибка добавления файла в архив: " << image << std::endl;
            zip_source_free(source);
        }
    }

    zip_close(zip); // Закрываем архив
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

	res = curl_easy_perform(curl); // Отправляем POST запрос

        if(res != CURLE_OK) { // Обрабатываем ошибку
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

    fclose(hd_src); // Чистим curl и закрываем архив
    curl_global_cleanup();
    return true;
}

// Основная функция
int main() {
    std::vector<std::string> catImages; // Вектор изображений
    std::set<long> uniqueImageSizes; // Вектор с уникальными размерами

    while (catImages.size() < NUM_CATS) { // Перебираем в цикле, пока не получим нужное количество котеек
        // Задержка от 1 до 15 секунд
        int delay = rand() % 15 + 1;
        std::this_thread::sleep_for(std::chrono::seconds(delay));

        std::string filename = "cat_" + std::to_string(catImages.size()) + ".jpeg"; // Создаём имя файла для изображения
        if (DownloadCatImage(filename) && IsUniqueImage(uniqueImageSizes, filename)) { // Проверяем, удалось ли скачать файл и уникален ли он
            catImages.push_back(filename);
            uniqueImageSizes.insert(GetFileSize(filename)); // Добавляем в множество уникальных размеров размер скачанного файла
            std::cout << "Получено уникальное изображение." << std::endl;
        } else {
            std::cout << "Не удалось получить уникальное изображение." << std::endl;
        }
    }

    if (CreateZipArchive(catImages)) { // Вызываем фунекцию + статус
    	std::cout << "Создан ZIP-архив с котиками." << std::endl;
    } else {
	std::cout << "Ошибка при создании ZIP-архива с котиками." << std::endl;

    if (postZipArchive("cats.zip")) { // Вызываем фунекцию + статус
        std::cout << "Архив успешно загружен!" << std::endl;
    } else {
        std::cout << "Ошибка при загрузке архива." << std::endl;
    }

    return 0;
}
