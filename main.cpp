#define UNICODE
#define _UNICODE

#include <windows.h>
#include <iostream>
#include <string>
#include <locale>
#include <io.h>
#include <fcntl.h>
#include <conio.h>

using namespace std;

// Функция для настройки консоли на использование UTF-8
void SetConsoleToUTF8() {
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
    _setmode(_fileno(stdout), _O_U8TEXT);
    _setmode(_fileno(stdin), _O_U8TEXT);
}

// Функция ожидания нажатия клавиши
void WaitForKeyPress() {
    wcout << L"\nНажмите любую клавишу для возврата в меню...";
    _getch();
}

// Функция очистки экрана
void ClearScreen() {
    system("cls");
}

// Вывод списка логических дисков с использованием GetLogicalDriveStrings
void ListDrives() {
    DWORD bufferLength = GetLogicalDriveStrings(0, nullptr);
    if (bufferLength == 0) {
        wcerr << L"Ошибка получения списка дисков." << endl;
        return;
    }
    auto *buffer = new wchar_t[bufferLength];
    DWORD result = GetLogicalDriveStrings(bufferLength, buffer);
    if (result == 0) {
        wcerr << L"Ошибка получения списка дисков." << endl;
        delete[] buffer;
        return;
    }
    wcout << L"Список логических дисков:" << endl;
    wchar_t *current = buffer;
    while (*current) {
        wcout << current << endl;
        current += wcslen(current) + 1;
    }
    delete[] buffer;
}

// Вывод информации о выбранном диске и размере свободного места
void ShowDriveInfo() {
    wstring drive;
    wcout << L"Введите букву диска (например, C): ";
    wcin >> drive;
    if (drive.length() == 1)
        drive = drive + L":\\";
    else if (drive.length() == 2 && drive[1] == L':')
        drive += L"\\";

    UINT driveType = GetDriveType(drive.c_str());
    wcout << L"Тип диска: ";
    switch (driveType) {
        case DRIVE_UNKNOWN: wcout << L"Неизвестно";
            break;
        case DRIVE_NO_ROOT_DIR: wcout << L"Отсутствует корневой каталог";
            break;
        case DRIVE_REMOVABLE: wcout << L"Съемный диск";
            break;
        case DRIVE_FIXED: wcout << L"Фиксированный диск";
            break;
        case DRIVE_REMOTE: wcout << L"Сетевой диск";
            break;
        case DRIVE_CDROM: wcout << L"CD/DVD диск";
            break;
        case DRIVE_RAMDISK: wcout << L"RAM диск";
            break;
        default: wcout << L"Неизвестно";
            break;
    }
    wcout << endl;

    wchar_t volumeNameBuffer[MAX_PATH + 1] = {0};
    wchar_t fileSystemNameBuffer[MAX_PATH + 1] = {0};
    DWORD serialNumber = 0, maxComponentLen = 0, fileSystemFlags = 0;
    if (GetVolumeInformation(drive.c_str(), volumeNameBuffer, MAX_PATH,
                             &serialNumber, &maxComponentLen, &fileSystemFlags,
                             fileSystemNameBuffer, MAX_PATH)) {
        wcout << L"Метка тома: " << volumeNameBuffer << endl;
        wcout << L"Файловая система: " << fileSystemNameBuffer << endl;
        wcout << L"Серийный номер: " << serialNumber << endl;
    } else {
        wcerr << L"Ошибка получения информации о томе." << endl;
    }

    DWORD sectorsPerCluster, bytesPerSector, numberOfFreeClusters, totalNumberOfClusters;
    if (GetDiskFreeSpace(drive.c_str(), &sectorsPerCluster, &bytesPerSector,
                         &numberOfFreeClusters, &totalNumberOfClusters)) {
        const ULONGLONG freeBytes = static_cast<ULONGLONG>(sectorsPerCluster) * bytesPerSector * numberOfFreeClusters;
        wcout << L"Свободное место: " << freeBytes / (1024 * 1024) << L" МБ" << endl;
    } else {
        wcerr << L"Ошибка получения информации о свободном месте." << endl;
    }
}

// Создание заданного каталога с использованием CreateDirectory
void CreateDirectoryFunc() {
    wstring dirPath;
    wcout << L"Введите путь для создания каталога: ";
    wcin.ignore();
    getline(wcin, dirPath);
    if (CreateDirectory(dirPath.c_str(), nullptr)) {
        wcout << L"Каталог создан успешно." << endl;
    } else {
        wcerr << L"Ошибка создания каталога. Код ошибки: " << GetLastError() << endl;
    }
}

// Удаление заданного каталога с использованием RemoveDirectory
void RemoveDirectoryFunc() {
    wstring dirPath;
    wcout << L"Введите путь для удаления каталога: ";
    wcin.ignore();
    getline(wcin, dirPath);
    if (RemoveDirectory(dirPath.c_str())) {
        wcout << L"Каталог удален успешно." << endl;
    } else {
        wcerr << L"Ошибка удаления каталога. Код ошибки: " << GetLastError() << endl;
    }
}

// Создание файла в новом каталоге с использованием CreateFile
void CreateFileFunc() {
    wstring filePath;
    wcout << L"Введите полный путь для создания файла: ";
    wcin.ignore();
    getline(wcin, filePath);
    HANDLE hFile = CreateFile(filePath.c_str(),
                              GENERIC_WRITE,
                              0,
                              nullptr,
                              CREATE_NEW,
                              FILE_ATTRIBUTE_NORMAL,
                              nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        wcerr << L"Ошибка создания файла. Код ошибки: " << GetLastError() << endl;
    } else {
        wcout << L"Файл создан успешно." << endl;
        CloseHandle(hFile);
    }
}

// Копирование файла с проверкой на совпадение имён (CopyFile)
void CopyFileFunc() {
    wstring srcPath, destPath;
    wcin.ignore();
    wcout << L"Введите путь исходного файла: ";
    getline(wcin, srcPath);
    wcout << L"Введите путь для копирования файла: ";
    getline(wcin, destPath);

    // Проверка существования файла в целевом каталоге
    if (GetFileAttributes(destPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        wcout << L"Файл с таким именем уже существует в целевом каталоге." << endl;
        return;
    }
    if (CopyFile(srcPath.c_str(), destPath.c_str(), TRUE)) {
        wcout << L"Файл скопирован успешно." << endl;
    } else {
        wcerr << L"Ошибка копирования файла. Код ошибки: " << GetLastError() << endl;
    }
}

// Перемещение файла с проверкой на совпадение имён (MoveFile)
void MoveFileFunc() {
    wstring srcPath, destPath;
    wcin.ignore();
    wcout << L"Введите путь исходного файла: ";
    getline(wcin, srcPath);
    wcout << L"Введите путь для перемещения файла: ";
    getline(wcin, destPath);

    // Проверка существования файла в целевом каталоге
    if (GetFileAttributes(destPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        wcout << L"Файл с таким именем уже существует в целевом каталоге." << endl;
        return;
    }
    if (MoveFile(srcPath.c_str(), destPath.c_str())) {
        wcout << L"Файл перемещен успешно." << endl;
    } else {
        wcerr << L"Ошибка перемещения файла. Код ошибки: " << GetLastError() << endl;
    }
}

// Анализ и изменение атрибутов файла (например, установка/снятие скрытого атрибута)
// Дополнительно выводится время создания файла с использованием GetFileTime
void FileAttributesFunc() {
    wstring filePath;
    wcin.ignore();
    wcout << L"Введите путь к файлу: ";
    getline(wcin, filePath);

    DWORD attributes = GetFileAttributes(filePath.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        wcerr << L"Ошибка получения атрибутов файла. Код ошибки: " << GetLastError() << endl;
        return;
    }
    wcout << L"Текущие атрибуты файла: " << attributes << endl;
    wcout << L"1. Сделать файл скрытым" << endl;
    wcout << L"2. Убрать скрытый атрибут" << endl;
    wcout << L"Выберите действие: ";
    int choice;
    wcin >> choice;
    if (choice == 1) {
        attributes |= FILE_ATTRIBUTE_HIDDEN;
        if (SetFileAttributes(filePath.c_str(), attributes))
            wcout << L"Атрибут изменен: файл скрыт." << endl;
        else
            wcerr << L"Ошибка установки атрибутов. Код ошибки: " << GetLastError() << endl;
    } else if (choice == 2) {
        attributes &= ~FILE_ATTRIBUTE_HIDDEN;
        if (SetFileAttributes(filePath.c_str(), attributes))
            wcout << L"Атрибут изменен: файл теперь не скрытый." << endl;
        else
            wcerr << L"Ошибка установки атрибутов. Код ошибки: " << GetLastError() << endl;
    } else {
        wcout << L"Неверный выбор." << endl;
    }

    // Получение времени создания файла
    HANDLE hFile = CreateFile(filePath.c_str(), GENERIC_READ,
                              FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        wcerr << L"Ошибка открытия файла для чтения времени. Код ошибки: " << GetLastError() << endl;
        return;
    }
    FILETIME creationTime, lastAccessTime, lastWriteTime;
    if (GetFileTime(hFile, &creationTime, &lastAccessTime, &lastWriteTime)) {
        SYSTEMTIME stUTC, stLocal;
        FileTimeToSystemTime(&creationTime, &stUTC);
        SystemTimeToTzSpecificLocalTime(nullptr, &stUTC, &stLocal);
        wcout << L"Время создания файла: "
                << stLocal.wDay << L"/" << stLocal.wMonth << L"/" << stLocal.wYear
                << L" " << stLocal.wHour << L":" << stLocal.wMinute << L":" << stLocal.wSecond
                << endl;
    } else {
        wcerr << L"Ошибка получения времени файла. Код ошибки: " << GetLastError() << endl;
    }
    CloseHandle(hFile);
}

int main() {
    SetConsoleToUTF8();
    setlocale(LC_ALL, "ru_RU.UTF-8");

    int choice;
    while (true) {
        wcout << L"\nМеню:" << endl;
        wcout << L"1. Вывести список дисков" << endl;
        wcout << L"2. Вывести информацию о диске и свободное место" << endl;
        wcout << L"3. Создать каталог" << endl;
        wcout << L"4. Удалить каталог" << endl;
        wcout << L"5. Создать файл" << endl;
        wcout << L"6. Копировать файл" << endl;
        wcout << L"7. Переместить файл" << endl;
        wcout << L"8. Анализ и изменение атрибутов файла" << endl;
        wcout << L"9. Выход" << endl;
        wcout << L"Выберите пункт меню: ";
        wcin >> choice;

        switch (choice) {
            case 1:
                ListDrives();
                getchar();
                break;
            case 2:
                ShowDriveInfo();
                getchar();
                break;
            case 3:
                CreateDirectoryFunc();
                getchar();
                break;
            case 4:
                RemoveDirectoryFunc();
                getchar();
                break;
            case 5:
                CreateFileFunc();
                getchar();
                break;
            case 6:
                CopyFileFunc();
                getchar();
                break;
            case 7:
                MoveFileFunc();
                getchar();
                break;
            case 8:
                FileAttributesFunc();
                getchar();
                break;
            case 9:
                wcout << L"Выход из программы." << endl;
                ClearScreen();
                return 0;
            default:
                ClearScreen();
                wcout << L"Неверный выбор, попробуйте снова." << endl;
        }
        if (choice > 0 && choice < 9) {
            WaitForKeyPress();
            ClearScreen();
        }
    }
}
