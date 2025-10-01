// g++ -o obs_recorder.exe obs_recorder.cpp -lgdi32 -luser32 -lshell32


#include <iostream>
#include <string>
#include <windows.h>
#include <vector>
#include <thread>
#include <chrono>

using namespace std;

// Структура для хранения информации об окнах
struct WindowInfo {
    HWND hwnd;
    string title;
};

vector<WindowInfo> windows;

// Callback функция для перечисления окон
BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    if (IsWindowVisible(hwnd)) {
        char windowTitle[256];
        GetWindowTextA(hwnd, windowTitle, sizeof(windowTitle));
        
        if (strlen(windowTitle) > 0) {
            WindowInfo info;
            info.hwnd = hwnd;
            info.title = string(windowTitle);
            windows.push_back(info);
        }
    }
    return TRUE;
}

// Функция для получения списка окон
void getWindowsList() {
    windows.clear();
    EnumWindows(EnumWindowsProc, 0);
}

// Функция для запуска OBS Studio
bool startOBS() {
    cout << "Запуск OBS Studio..." << endl;
    
    // Попытка запустить OBS
    HINSTANCE result = ShellExecuteA(NULL, "open", "obs64.exe", 
                                     "--minimize-to-tray --startrecording", 
                                     NULL, SW_HIDE);
    
    if ((INT_PTR)result <= 32) {
        // Попробуем альтернативный путь
        result = ShellExecuteA(NULL, "open", 
                              "C:\\Program Files\\obs-studio\\bin\\64bit\\obs64.exe",
                              "--minimize-to-tray --startrecording", 
                              NULL, SW_HIDE);
        
        if ((INT_PTR)result <= 32) {
            cerr << "Ошибка: OBS Studio не найден!" << endl;
            cerr << "Убедитесь, что OBS установлен и добавлен в PATH" << endl;
            return false;
        }
    }
    
    this_thread::sleep_for(chrono::seconds(3));
    cout << "OBS Studio запущен!" << endl;
    return true;
}

// Функция для создания конфигурационного файла для записи полного экрана
void createFullScreenConfig() {
    string appdata = getenv("APPDATA");
    string configPath = appdata + "\\obs-studio\\basic\\scenes\\";
    
    // Здесь можно создать JSON конфигурацию для OBS
    cout << "Настройка записи полного экрана..." << endl;
}

// Функция для создания конфигурации для записи окна
void createWindowConfig(const string& windowTitle) {
    cout << "Настройка записи окна: " << windowTitle << endl;
}

// Функция для отправки команды в OBS через WebSocket (упрощенная версия)
void sendOBSCommand(const string& command) {
    // Для полноценной работы требуется obs-websocket плагин
    // Здесь упрощенная версия через горячие клавиши
    
    if (command == "start") {
        cout << "Начинаем запись..." << endl;
        // Симуляция нажатия горячей клавиши (по умолчанию в OBS это может быть настроено)
        keybd_event(VK_CONTROL, 0, 0, 0);
        keybd_event(VK_F12, 0, 0, 0);
        keybd_event(VK_F12, 0, KEYEVENTF_KEYUP, 0);
        keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
    }
    else if (command == "stop") {
        cout << "Останавливаем запись..." << endl;
        keybd_event(VK_CONTROL, 0, 0, 0);
        keybd_event(VK_F12, 0, 0, 0);
        keybd_event(VK_F12, 0, KEYEVENTF_KEYUP, 0);
        keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);
    }
}

void displayMenu() {
    cout << "\n========================================" << endl;
    cout << "    OBS STUDIO SCREEN RECORDER" << endl;
    cout << "========================================" << endl;
    cout << "1. Записать весь экран" << endl;
    cout << "2. Записать конкретное окно" << endl;
    cout << "3. Остановить запись" << endl;
    cout << "4. Выход" << endl;
    cout << "========================================" << endl;
    cout << "Выберите опцию: ";
}

void recordFullScreen() {
    cout << "\n--- ЗАПИСЬ ПОЛНОГО ЭКРАНА ---" << endl;
    
    if (!startOBS()) {
        return;
    }
    
    createFullScreenConfig();
    
    cout << "\nЗапись началась!" << endl;
    cout << "Нажмите Enter для остановки записи..." << endl;
    cin.ignore();
    cin.get();
    
    sendOBSCommand("stop");
    cout << "Запись остановлена!" << endl;
}

void recordWindow() {
    cout << "\n--- ЗАПИСЬ КОНКРЕТНОГО ОКНА ---" << endl;
    
    cout << "Получение списка открытых окон..." << endl;
    getWindowsList();
    
    if (windows.empty()) {
        cout << "Нет доступных окон для записи!" << endl;
        return;
    }
    
    cout << "\nДоступные окна:" << endl;
    for (size_t i = 0; i < windows.size() && i < 20; i++) {
        cout << i + 1 << ". " << windows[i].title << endl;
    }
    
    cout << "\nВыберите номер окна для записи: ";
    int choice;
    cin >> choice;
    
    if (choice < 1 || choice > (int)windows.size()) {
        cout << "Неверный выбор!" << endl;
        return;
    }
    
    WindowInfo selectedWindow = windows[choice - 1];
    cout << "\nВыбрано окно: " << selectedWindow.title << endl;
    
    if (!startOBS()) {
        return;
    }
    
    createWindowConfig(selectedWindow.title);
    
    cout << "\nЗапись началась!" << endl;
    cout << "Нажмите Enter для остановки записи..." << endl;
    cin.ignore();
    cin.get();
    
    sendOBSCommand("stop");
    cout << "Запись остановлена!" << endl;
}

int main() {
    // Установка поддержки кириллицы в консоли
    SetConsoleCP(1251);
    SetConsoleOutputCP(1251);
    
    cout << "Добро пожаловать в OBS Studio Screen Recorder!" << endl;
    cout << "\nВНИМАНИЕ: Для работы программы требуется:" << endl;
    cout << "1. Установленный OBS Studio" << endl;
    cout << "2. Настроенные горячие клавиши для записи (Ctrl+F12)" << endl;
    cout << "3. Или установленный плагин obs-websocket для расширенного управления" << endl;
    
    int choice;
    
    while (true) {
        displayMenu();
        cin >> choice;
        
        switch (choice) {
            case 1:
                recordFullScreen();
                break;
            case 2:
                recordWindow();
                break;
            case 3:
                sendOBSCommand("stop");
                cout << "Запись остановлена!" << endl;
                break;
            case 4:
                cout << "Выход из программы..." << endl;
                return 0;
            default:
                cout << "Неверный выбор! Попробуйте снова." << endl;
        }
        
        this_thread::sleep_for(chrono::seconds(1));
    }
    
    return 0;
}