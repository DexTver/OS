#define UNICODE
#define _UNICODE

#include <windows.h>
#include <iostream>
#include <string>
#include <locale>
#include <io.h>
#include <fcntl.h>
#include <conio.h>
#include <limits>

using namespace std;

// Настройка консоли на использование UTF-8
void SetConsoleToUTF8() {
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
    _setmode(_fileno(stdout), _O_U8TEXT);
    _setmode(_fileno(stdin), _O_U8TEXT);
}

// Ожидание нажатия клавиши
void WaitForKeyPress() {
    wcout << L"\nНажмите любую клавишу для возврата в меню...";
    _getch();
}

// Очистка экрана
void ClearScreen() {
    system("cls");
}

// Вывод списка логических дисков
void ListDrives() {
    DWORD bufferLength = GetLogicalDriveStringsW(0, nullptr);
    if (bufferLength == 0) {
        wcerr << L"Ошибка получения списка дисков." << endl;
        return;
    }
    auto *buffer = new wchar_t[bufferLength];
    DWORD result = GetLogicalDriveStringsW(bufferLength, buffer);
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

// Вывод информации о диске
void ShowDriveInfo() {
    wstring drive;
    wcout << L"Введите букву диска (например, C): ";
    wcin >> drive;
    if (drive.length() == 1)
        drive += L":\\";
    else if (drive.length() == 2 && drive[1] == L':')
        drive += L"\\";

    UINT driveType = GetDriveTypeW(drive.c_str());
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
    if (GetVolumeInformationW(drive.c_str(), volumeNameBuffer, MAX_PATH, &serialNumber, &maxComponentLen, &fileSystemFlags, fileSystemNameBuffer, MAX_PATH)) {
        wcout << L"Метка тома: " << volumeNameBuffer << endl;
        wcout << L"Файловая система: " << fileSystemNameBuffer << endl;
        wcout << L"Серийный номер: " << serialNumber << endl;
        wcout << L"Максимальная длина имени файла: " << maxComponentLen << endl;
        wcout << L"Числовое значение флагов (fileSystemFlags): " << fileSystemFlags << endl;
    } else {
        wcerr << L"Ошибка получения информации о томе." << endl;
    }

    DWORD sectorsPerCluster, bytesPerSector, numberOfFreeClusters, totalNumberOfClusters;
    if (GetDiskFreeSpaceW(drive.c_str(), &sectorsPerCluster, &bytesPerSector, &numberOfFreeClusters, &totalNumberOfClusters)) {
        ULONGLONG freeBytes = static_cast<ULONGLONG>(sectorsPerCluster) * bytesPerSector * numberOfFreeClusters;
        wcout << L"Свободное место: " << freeBytes / (1024 * 1024) << L" МБ" << endl;
    } else {
        wcerr << L"Ошибка получения информации о свободном месте." << endl;
    }
}

// Создание каталога
void CreateDirectoryFunc() {
    wstring dirPath;
    wcout << L"Введите путь для создания каталога: ";
    wcin.ignore();
    getline(wcin, dirPath);
    if (CreateDirectoryW(dirPath.c_str(), nullptr)) {
        wcout << L"Каталог создан успешно." << endl;
    } else {
        wcerr << L"Ошибка создания каталога. Код ошибки: " << GetLastError() << endl;
    }
}

// Удаление заданного каталога
void RemoveDirectoryFunc() {
    wstring dirPath;
    wcout << L"Введите путь для удаления каталога: ";
    wcin.ignore();
    getline(wcin, dirPath);

    wcout << L"Вы действительно хотите удалить папку? [Y/N]: ";
    wchar_t confirm;
    wcin >> confirm;

    if (confirm == L'Y' || confirm == L'y') {
        if (RemoveDirectoryW(dirPath.c_str())) {
            wcout << L"Каталог удален успешно." << endl;
        } else {
            DWORD err = GetLastError();
            if (err == ERROR_DIR_NOT_EMPTY) {
                wcerr << L"Ошибка: каталог не пуст!" << endl;
            } else {
                wcerr << L"Ошибка удаления каталога. Код ошибки: " << err << endl;
            }
        }
    } else {
        wcout << L"Операция удаления отменена пользователем." << endl;
    }
}

// Создание файла
void CreateFileFunc() {
    wstring filePath;
    wcout << L"Введите полный путь для создания файла: ";
    wcin.ignore();
    getline(wcin, filePath);
    HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        wcerr << L"Ошибка создания файла. Код ошибки: " << GetLastError() << endl;
    } else {
        wcout << L"Файл создан успешно." << endl;
        CloseHandle(hFile);
    }
}

// Копирование файла
void CopyFileFunc() {
    wstring srcPath, destPath;
    wcin.ignore();
    wcout << L"Введите путь исходного файла: ";
    getline(wcin, srcPath);
    wcout << L"Введите путь для копирования файла: ";
    getline(wcin, destPath);

    if (GetFileAttributesW(destPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        wcout << L"Файл с таким именем уже существует в целевом каталоге." << endl;
        return;
    }
    if (CopyFileW(srcPath.c_str(), destPath.c_str(), TRUE)) {
        wcout << L"Файл скопирован успешно." << endl;
    } else {
        wcerr << L"Ошибка копирования файла. Код ошибки: " << GetLastError() << endl;
    }
}

// Перемещение файла
void MoveFileFunc() {
    wstring srcPath, destPath;
    wcin.ignore();
    wcout << L"Введите путь исходного файла: ";
    getline(wcin, srcPath);
    wcout << L"Введите путь для перемещения файла: ";
    getline(wcin, destPath);

    if (GetFileAttributesW(destPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        wcout << L"Файл с таким именем уже существует в целевом каталоге." << endl;
        return;
    }
    if (MoveFileW(srcPath.c_str(), destPath.c_str())) {
        wcout << L"Файл перемещен успешно." << endl;
    } else {
        wcerr << L"Ошибка перемещения файла. Код ошибки: " << GetLastError() << endl;
    }
}

// ----------------------------------------------------------
//  ПОКАЗАТЬ/РАСШИФРОВАТЬ ТЕКУЩИЕ АТРИБУТЫ
// ----------------------------------------------------------
void PrintAttributesDecode(DWORD attributes) {
    wcout << L"\nРасшифровка атрибутов:\n";
    if (attributes & FILE_ATTRIBUTE_READONLY) {
        wcout << L"  - READONLY\n";
    }
    if (attributes & FILE_ATTRIBUTE_HIDDEN) {
        wcout << L"  - HIDDEN\n";
    }
    if (attributes & FILE_ATTRIBUTE_SYSTEM) {
        wcout << L"  - SYSTEM\n";
    }
    if (attributes & FILE_ATTRIBUTE_DIRECTORY) {
        wcout << L"  - DIRECTORY\n";
    }
    if (attributes & FILE_ATTRIBUTE_ARCHIVE) {
        wcout << L"  - ARCHIVE\n";
    }
    if (attributes & FILE_ATTRIBUTE_DEVICE) {
        wcout << L"  - DEVICE\n";
    }
    if (attributes & FILE_ATTRIBUTE_NORMAL) {
        wcout << L"  - NORMAL\n";
    }
    if (attributes & FILE_ATTRIBUTE_TEMPORARY) {
        wcout << L"  - TEMPORARY\n";
    }
    if (attributes & FILE_ATTRIBUTE_COMPRESSED) {
        wcout << L"  - COMPRESSED\n";
    }
    if (attributes & FILE_ATTRIBUTE_ENCRYPTED) {
        wcout << L"  - ENCRYPTED\n";
    }
    wcout << endl;
}

// ----------------------------------------------------------
//  АНАЛИЗ И ИЗМЕНЕНИЕ АТРИБУТОВ ФАЙЛА
// ----------------------------------------------------------
void FileAttributesFunc() {
    wstring filePath;
    wcin.ignore();
    wcout << L"Введите путь к файлу: ";
    getline(wcin, filePath);

    DWORD attributes = GetFileAttributesW(filePath.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        wcerr << L"Ошибка получения атрибутов файла. Код ошибки: " << GetLastError() << endl;
        return;
    }
    wcout << L"Текущие атрибуты (числом): " << attributes << endl;
    PrintAttributesDecode(attributes);

    wcout << L"1. Сделать файл скрытым (HIDDEN)\n"
            L"2. Убрать скрытый атрибут\n"
            L"3. Сделать файл только для чтения (READONLY)\n"
            L"4. Убрать атрибут только для чтения\n"
            L"0. Ничего не менять\n"
            L"Выберите действие: ";
    int choice;
    wcin >> choice;

    switch (choice) {
        case 1:
            attributes |= FILE_ATTRIBUTE_HIDDEN;
            break;
        case 2:
            attributes &= ~FILE_ATTRIBUTE_HIDDEN;
            break;
        case 3:
            attributes |= FILE_ATTRIBUTE_READONLY;
            break;
        case 4:
            attributes &= ~FILE_ATTRIBUTE_READONLY;
            break;
        case 0:
            wcout << L"Атрибуты не изменялись.\n";
            return;
        default:
            wcout << L"Неверный выбор.\n";
            return;
    }

    if (SetFileAttributesW(filePath.c_str(), attributes)) {
        wcout << L"Атрибут успешно изменен.\n";
    } else {
        wcerr << L"Ошибка изменения атрибутов. Код ошибки: " << GetLastError() << endl;
    }
}

// Изменение времени
void ChangeFileTimeFunc() {
    wstring filePath;
    wcin.ignore();
    wcout << L"Введите путь к файлу: ";
    getline(wcin, filePath);

    HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        wcerr << L"Ошибка открытия файла. Код: " << GetLastError() << endl;
        return;
    }

    FILETIME ftCreate, ftAccess, ftWrite;
    if (!GetFileTime(hFile, &ftCreate, &ftAccess, &ftWrite)) {
        wcerr << L"Ошибка получения времени файла. Код: " << GetLastError() << endl;
        CloseHandle(hFile);
        return;
    }

    SYSTEMTIME stCreateUTC, stAccessUTC, stWriteUTC;
    FileTimeToSystemTime(&ftCreate, &stCreateUTC);
    FileTimeToSystemTime(&ftAccess, &stAccessUTC);
    FileTimeToSystemTime(&ftWrite, &stWriteUTC);

    wcout << L"\nТекущее время создания (UTC): "
            << stCreateUTC.wDay << L"/" << stCreateUTC.wMonth << L"/" << stCreateUTC.wYear
            << L" " << stCreateUTC.wHour << L":" << stCreateUTC.wMinute << L":" << stCreateUTC.wSecond << endl;

    wcout << L"Текущее время последнего доступа (UTC): "
            << stAccessUTC.wDay << L"/" << stAccessUTC.wMonth << L"/" << stAccessUTC.wYear
            << L" " << stAccessUTC.wHour << L":" << stAccessUTC.wMinute << L":" << stAccessUTC.wSecond << endl;

    wcout << L"Текущее время последней записи (UTC): "
            << stWriteUTC.wDay << L"/" << stWriteUTC.wMonth << L"/" << stWriteUTC.wYear
            << L" " << stWriteUTC.wHour << L":" << stWriteUTC.wMinute << L":" << stWriteUTC.wSecond << endl;

    wcout << L"\nВыберите, какое время хотите изменить:\n"
            << L"1. Время создания (Creation Time)\n"
            << L"2. Время последнего доступа (Last Access Time)\n"
            << L"3. Время последней записи (Last Write Time)\n"
            << L"0. Отмена\n"
            << L"Ваш выбор: ";

    int choice;
    wcin >> choice;
    if (!wcin || choice < 0 || choice > 3) {
        wcin.clear();
        wcin.ignore(numeric_limits<streamsize>::max(), L'\n');
        wcout << L"Неверный выбор или ошибка ввода. Отмена операции." << endl;
        CloseHandle(hFile);
        return;
    }
    if (choice == 0) {
        wcout << L"Операция отменена." << endl;
        CloseHandle(hFile);
        return;
    }

    wcout << L"Хотите ли вы установить новое время для выбранного пункта? (Y/N): ";
    wchar_t confirm;
    wcin >> confirm;
    if (confirm != L'Y' && confirm != L'y') {
        wcout << L"Изменение времени отменено пользователем." << endl;
        CloseHandle(hFile);
        return;
    }

    SYSTEMTIME stNewUTC;
    ZeroMemory(&stNewUTC, sizeof(stNewUTC));
    wcout << L"Введите новую дату/время (UTC) в формате:\n"
            << L"ДД ММ ГГГГ ЧЧ ММ СС\n"
            << L"(например, \"28 03 2025 12 30 00\"): ";

    wcin >> stNewUTC.wDay >> stNewUTC.wMonth >> stNewUTC.wYear
            >> stNewUTC.wHour >> stNewUTC.wMinute >> stNewUTC.wSecond;

    FILETIME ftNew;
    if (!SystemTimeToFileTime(&stNewUTC, &ftNew)) {
        wcerr << L"Ошибка преобразования SystemTime в FileTime." << endl;
        CloseHandle(hFile);
        return;
    }

    switch (choice) {
        case 1:
            ftCreate = ftNew;
            break;
        case 2:
            ftAccess = ftNew;
            break;
        case 3:
            ftWrite = ftNew;
            break;
    }

    if (!SetFileTime(hFile, &ftCreate, &ftAccess, &ftWrite)) {
        wcerr << L"Ошибка изменения времени. Код: " << GetLastError() << endl;
    } else {
        wcout << L"Выбранное время успешно изменено." << endl;
    }
    CloseHandle(hFile);
}


int main() {
    SetConsoleToUTF8();
    setlocale(LC_ALL, "ru_RU.UTF-8");

    while (true) {
        wcout << L"\nМеню:\n"
                << L"1. Вывести список дисков\n"
                << L"2. Вывести информацию о диске\n"
                << L"3. Создать каталог\n"
                << L"4. Удалить каталог (рекурсивно)\n"
                << L"5. Создать файл\n"
                << L"6. Копировать файл\n"
                << L"7. Переместить файл\n"
                << L"8. Анализ и изменение атрибутов файла\n"
                << L"9. Изменение времени файла\n"
                << L"10. Выход\n"
                << L"Выберите пункт меню: ";

        int choice;
        if (!(wcin >> choice)) {
            wcin.clear();
            wcin.ignore(numeric_limits<streamsize>::max(), L'\n');
            ClearScreen();
            wcout << L"Ошибка ввода. Попробуйте еще раз." << endl;
            continue;
        }
        ClearScreen();

        switch (choice) {
            case 1:
                ListDrives();
                break;
            case 2:
                ShowDriveInfo();
                break;
            case 3:
                CreateDirectoryFunc();
                break;
            case 4:
                RemoveDirectoryFunc();
                break;
            case 5:
                CreateFileFunc();
                break;
            case 6:
                CopyFileFunc();
                break;
            case 7:
                MoveFileFunc();
                break;
            case 8:
                FileAttributesFunc();
                break;
            case 9:
                ChangeFileTimeFunc();
                break;
            case 10:
                wcout << L"Выход из программы." << endl;
                return 0;
            default:
                ClearScreen();
                wcout << L"Неверный выбор, попробуйте снова." << endl;
                break;
        }

        if (choice >= 1 && choice <= 9) {
            WaitForKeyPress();
            ClearScreen();
        }
    }
}
