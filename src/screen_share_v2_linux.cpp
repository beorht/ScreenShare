#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <fstream>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <signal.h>
#include <sys/wait.h>

using namespace std;

struct WindowInfo {
    Window window;
    string title;
    string windowClass;
};

class OBSRecorder {
private:
    Display* display;
    vector<WindowInfo> windows;
    pid_t obsPid;
    bool isRecording;
    
public:
    OBSRecorder() : display(nullptr), obsPid(-1), isRecording(false) {
        display = XOpenDisplay(nullptr);
        if (!display) {
            cerr << "Ошибка: не удалось подключиться к X11 дисплею!" << endl;
            exit(1);
        }
    }
    
    ~OBSRecorder() {
        if (display) {
            XCloseDisplay(display);
        }
    }
    
    // Получить имя окна
    string getWindowName(Window w) {
        char* name = nullptr;
        if (XFetchName(display, w, &name) && name) {
            string result(name);
            XFree(name);
            return result;
        }
        return "";
    }
    
    // Получить класс окна
    string getWindowClass(Window w) {
        XClassHint classHint;
        if (XGetClassHint(display, w, &classHint)) {
            string result;
            if (classHint.res_class) {
                result = string(classHint.res_class);
                XFree(classHint.res_class);
            }
            if (classHint.res_name) {
                XFree(classHint.res_name);
            }
            return result;
        }
        return "";
    }
    
    // Рекурсивно получить все окна
    void getWindowsRecursive(Window w) {
        Window root, parent;
        Window* children;
        unsigned int nchildren;
        
        if (XQueryTree(display, w, &root, &parent, &children, &nchildren)) {
            for (unsigned int i = 0; i < nchildren; i++) {
                string title = getWindowName(children[i]);
                if (!title.empty()) {
                    WindowInfo info;
                    info.window = children[i];
                    info.title = title;
                    info.windowClass = getWindowClass(children[i]);
                    windows.push_back(info);
                }
                getWindowsRecursive(children[i]);
            }
            if (children) XFree(children);
        }
    }
    
    // Получить список всех окон
    void getWindowsList() {
        windows.clear();
        Window root = DefaultRootWindow(display);
        getWindowsRecursive(root);
    }
    
    // Создать профиль OBS для записи полного экрана
    void createFullScreenProfile() {
        string homeDir = getenv("HOME");
        string configDir = homeDir + "/.config/obs-studio/basic/scenes/";
        system(("mkdir -p " + configDir).c_str());
        
        string profilePath = homeDir + "/.config/obs-studio/basic/profiles/ScreenRecorder/";
        system(("mkdir -p " + profilePath).c_str());
        
        // Создаем простую конфигурацию
        ofstream basicConfig(profilePath + "basic.ini");
        basicConfig << "[Output]\n";
        basicConfig << "Mode=Simple\n";
        basicConfig << "[SimpleOutput]\n";
        basicConfig << "FilePath=" << homeDir << "/Videos\n";
        basicConfig << "RecFormat=mkv\n";
        basicConfig << "RecEncoder=x264\n";
        basicConfig.close();
    }
    
    // Запустить OBS в режиме записи полного экрана
    bool startFullScreenRecording() {
        cout << "\n=== Запуск записи полного экрана ===" << endl;
        
        // Создаем временный скрипт для OBS
        string scriptPath = "/tmp/obs_fullscreen.sh";
        ofstream script(scriptPath);
        script << "#!/bin/bash\n";
        script << "obs --startrecording --minimize-to-tray --scene 'Screen Capture' &\n";
        script.close();
        
        system(("chmod +x " + scriptPath).c_str());
        
        // Запускаем OBS
        obsPid = fork();
        if (obsPid == 0) {
            // Дочерний процесс
            execlp("obs", "obs", "--startrecording", "--minimize-to-tray", nullptr);
            exit(1);
        } else if (obsPid > 0) {
            sleep(5); // Даем OBS время запуститься
            isRecording = true;
            cout << "✓ Запись началась! PID: " << obsPid << endl;
            return true;
        }
        
        cerr << "✗ Ошибка запуска OBS!" << endl;
        return false;
    }
    
    // Запустить запись конкретного окна
    bool startWindowRecording(const WindowInfo& windowInfo) {
        cout << "\n=== Запуск записи окна ===" << endl;
        cout << "Окно: " << windowInfo.title << endl;
        cout << "Класс: " << windowInfo.windowClass << endl;
        
        // Получаем ID окна в hex формате для OBS
        char windowId[32];
        sprintf(windowId, "0x%lx", windowInfo.window);
        
        // Создаем скрипт для запуска OBS с захватом окна
        string scriptPath = "/tmp/obs_window.sh";
        ofstream script(scriptPath);
        script << "#!/bin/bash\n";
        script << "# Window ID: " << windowId << "\n";
        script << "obs --startrecording --minimize-to-tray &\n";
        script.close();
        
        system(("chmod +x " + scriptPath).c_str());
        
        obsPid = fork();
        if (obsPid == 0) {
            execlp("obs", "obs", "--startrecording", "--minimize-to-tray", nullptr);
            exit(1);
        } else if (obsPid > 0) {
            sleep(5);
            isRecording = true;
            cout << "✓ Запись началась! PID: " << obsPid << endl;
            cout << "\nДля захвата окна в OBS:" << endl;
            cout << "1. Добавьте источник 'Window Capture (Xcomposite)'" << endl;
            cout << "2. Выберите окно: " << windowInfo.title << endl;
            cout << "3. Window ID: " << windowId << endl;
            return true;
        }
        
        cerr << "✗ Ошибка запуска OBS!" << endl;
        return false;
    }
    
    // Остановить запись
    void stopRecording() {
        if (!isRecording) {
            cout << "Запись не активна." << endl;
            return;
        }
        
        cout << "\n=== Остановка записи ===" << endl;
        
        // Отправляем сигнал SIGTERM процессу OBS
        if (obsPid > 0) {
            kill(obsPid, SIGTERM);
            sleep(2);
            
            // Проверяем, завершился ли процесс
            int status;
            if (waitpid(obsPid, &status, WNOHANG) == 0) {
                // Процесс еще работает, принудительно завершаем
                kill(obsPid, SIGKILL);
            }
            
            cout << "✓ Запись остановлена!" << endl;
            cout << "Файлы сохранены в: ~/Videos/" << endl;
        }
        
        isRecording = false;
        obsPid = -1;
    }
    
    // Показать список окон
    void displayWindows() {
        cout << "\n=== Доступные окна ===\n" << endl;
        
        for (size_t i = 0; i < windows.size(); i++) {
            cout << i + 1 << ". " << windows[i].title;
            if (!windows[i].windowClass.empty()) {
                cout << " [" << windows[i].windowClass << "]";
            }
            cout << endl;
        }
        cout << endl;
    }
};

void displayMenu() {
    cout << "\n╔════════════════════════════════════════╗" << endl;
    cout << "║   OBS STUDIO SCREEN RECORDER (Linux)   ║" << endl;
    cout << "╠════════════════════════════════════════╣" << endl;
    cout << "║ 1. Записать весь экран                 ║" << endl;
    cout << "║ 2. Записать конкретное окно            ║" << endl;
    cout << "║ 3. Остановить запись                   ║" << endl;
    cout << "║ 4. Выход                               ║" << endl;
    cout << "╚════════════════════════════════════════╝" << endl;
    cout << "\nВыберите опцию: ";
}

int main() {
    cout << "╔════════════════════════════════════════╗" << endl;
    cout << "║  Добро пожаловать в OBS Recorder!      ║" << endl;
    cout << "║  Система: Linux Manjaro + i3 + X11     ║" << endl;
    cout << "╚════════════════════════════════════════╝" << endl;
    
    // Проверка наличия OBS
    if (system("which obs > /dev/null 2>&1") != 0) {
        cerr << "\n✗ Ошибка: OBS Studio не найден!" << endl;
        cerr << "Установите OBS командой: sudo pacman -S obs-studio" << endl;
        return 1;
    }
    
    cout << "\n✓ OBS Studio обнаружен" << endl;
    cout << "✓ X11 дисплей подключен" << endl;
    
    OBSRecorder recorder;
    int choice;
    
    while (true) {
        displayMenu();
        cin >> choice;
        
        if (cin.fail()) {
            cin.clear();
            cin.ignore(10000, '\n');
            cout << "✗ Неверный ввод!" << endl;
            continue;
        }
        
        switch (choice) {
            case 1: {
                recorder.createFullScreenProfile();
                if (recorder.startFullScreenRecording()) {
                    cout << "\n✓ Идет запись экрана..." << endl;
                    cout << "Нажмите Enter для остановки записи...";
                    cin.ignore();
                    cin.get();
                    recorder.stopRecording();
                }
                break;
            }
            case 2: {
                cout << "\nПолучение списка окон..." << endl;
                recorder.getWindowsList();
                recorder.displayWindows();
                
                cout << "Введите номер окна для записи: ";
                int windowChoice;
                cin >> windowChoice;
                
                if (windowChoice < 1) {
                    cout << "✗ Неверный выбор!" << endl;
                    break;
                }
                
                // Получаем актуальный список
                vector<WindowInfo> windows;
                recorder.getWindowsList();
                
                if (windowChoice > 0 && windowChoice <= (int)windows.size()) {
                    if (recorder.startWindowRecording(windows[windowChoice - 1])) {
                        cout << "\n✓ Идет запись окна..." << endl;
                        cout << "Нажмите Enter для остановки записи...";
                        cin.ignore();
                        cin.get();
                        recorder.stopRecording();
                    }
                } else {
                    cout << "✗ Неверный выбор!" << endl;
                }
                break;
            }
            case 3:
                recorder.stopRecording();
                break;
            case 4:
                cout << "\nВыход из программы..." << endl;
                recorder.stopRecording();
                return 0;
            default:
                cout << "✗ Неверный выбор! Попробуйте снова." << endl;
        }
        
        sleep(1);
    }
    
    return 0;
}