// g++ src/srceen_share.cpp -o screen_share -std=c++17 -I/usr/include/opencv4 -lX11 -lopencv_core -lopencv_highgui  -lopencv_imgproc -lopencv_imgcodecs


#include <iostream>  // for basic_ostream::operator<<, operator<<, endl, basic_ostream, basic_ostream<>::__ostream_type, cout, ostream
#include <cstring>
#include <vector>  // for vector
#include <string>    // for string, basic_string, allocator
#include <thread>

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sstream>
#include <cerrno>
#include <cstring>

#include <opencv2/opencv.hpp>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>


// Захват экрана
cv::Mat captureScreen(Display* disp, int width, int height, int x, int y);

// Получение список окон 
Window* getWindowList(Display* disp, unsigned long* len, Atom prop);

// Получение название окок
std::string getWindowName(Display* disp, Window win, Atom netWmName, Atom utf8Str); 

// Получение открытых окон
int getWindowWorkspace(Display* disp, Window win);

// Получение размер и кардинат окон
std::vector<int> getWindowGeometry(Display* disp, Window win);

// Получение кардинат окон
std::vector<int> getFullWindowGeometry(Display* disp);

// Простая отправка HTTP-ответа с изображением
void sendHttpResponse(int client_sock, const std::vector<uchar>& image_data);

// Проверка порта 
bool is_port_in_use(int port); 

int main(int argc, char const *argv[])
{

    XInitThreads();

    // Открываем окно
    Display* display = XOpenDisplay(nullptr);
    if (!display) 
    {
        std::cerr << "Не удалось открыть дисплей\n";
        return 1;
    }

    const Atom prop       =  XInternAtom(display, "_NET_CLIENT_LIST", False);
    const Atom netWmName  =  XInternAtom(display, "_NET_WM_NAME", False);
    const Atom utf8Str    =  XInternAtom(display, "UTF8_STRING", False);
    
    const std::string html =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n\r\n"
        "<!DOCTYPE html><html><head><meta charset='utf-8'><title>Экран</title>"
        "<style>html, body{text-align:center;background:#302f30;font-family:sans-serif;display:}img{width:100%;max-width:1000px;border:3px solid #eb214d; border-radius:20px}</style>"
        "<img id='screen' src='/screenshot.jpg'>"
        "<script>setInterval(()=>{const img = document.getElementById('screen');img.src='/screenshot.jpg?'+Date.now()},300)</script>"
        "</body></html>";

    int BUFFER_SIZE = 2048;
    int PORT_HOST = 8085;

    std::string command;
    
    // Getting windows list
    unsigned long len;
    Window* list = getWindowList(display, &len, prop);
    if (!list) 
    {
        std::cerr << "Не удалось получить список окон\n";
        return 1;
    }

    std::cout << "\n::: Для вывод плного экрана введите команду 'full'" << "\n\n";

    // Gettings windows name
    for (unsigned long i = 0; i < len; ++i) 
    {
        std::string name = getWindowName(display, list[i], netWmName, utf8Str);
        //int ws = getWindowWorkspace(display, list[i]);

        std::cout << "Окно [ " << i << " ] ::: ";
        std::cout << " ~ " << name << "\n";
        //std::cout << " Рабочее пространство: " << ws << "\n\n";
    }

    std::cout << ">>> ";
    std::cin >> command;

    // Создание сокета
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in address = {0};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT_HOST);
    
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    bind(server_fd, (struct sockaddr*)&address, sizeof(address));
    listen(server_fd, 5);

    std::cout << "Сервер запущен на http://localhost:8085\n";

    while (true)
    {
        int client_sock = accept(server_fd, nullptr, nullptr);
        if (client_sock < 0) continue;
        
        std::thread([client_sock, display, list, netWmName, utf8Str, html, command]() 
        {
            char buffer[2048] = {0};
            read(client_sock, buffer, sizeof(buffer));
            std::string request(buffer);

            std::istringstream iss(request);
            std::string method, path;
            iss >> method >> path;



            if (path.find("/screenshot") == 0) 
            {
                try 
                {
                    std::vector<int> windowParametr;

                    if ( command == "full" )
                    {
                        windowParametr = getFullWindowGeometry(display);
                    } else 
                    {
                        windowParametr = getWindowGeometry(display, list[std::stoi(command)]);
                    }
                    cv::Mat screen = captureScreen(display, windowParametr[2], windowParametr[3], windowParametr[0], windowParametr[1]);

                    std::vector<uchar> buf;
                    cv::imencode(".jpg", screen, buf);
                    sendHttpResponse(client_sock, buf);

                    std::cout << " { " << windowParametr[2] << "x" << windowParametr[3] << " }";
                    std::cout << " ::: x=" << windowParametr[0] << " y=" << windowParametr[1] << " ";
                    std::cout << "Экран захвачен" << std::endl;

                } catch (const std::exception& e) 
                {
                    std::cerr << "Ошибка: " << e.what() << std::endl;
                }
            } else 
            {
                send(client_sock, html.c_str(), html.size(), 0);
            }

            close(client_sock);
        }).detach();  // запускаем в отдельном потоке
    }

    close(server_fd);
    XFree(list);
    XCloseDisplay(display);
    cv::destroyAllWindows();

    return 0;
}


//  ===   Проверка порта на занятость   ===
bool is_port_in_use(int port) 
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) 
    {
        std::cerr << "Ошибка создания сокета: " << strerror(errno) << std::endl;
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    // Пробуем привязать сокет к порту
    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) 
    {
        if (errno == EADDRINUSE) 
        {
            close(sock);
            return true;  // Порт занят
        }
        close(sock);
        return false;
    }

    close(sock);
    return false;  // Порт свободен
}

// === Простая отправка HTTP-ответа с изображением ===
void sendHttpResponse(int client_sock, const std::vector<uchar>& image_data) {
    std::ostringstream oss;
    oss << "HTTP/1.1 200 OK\r\n";
    oss << "Content-Type: image/jpeg\r\n";
    oss << "Content-Length: " << image_data.size() << "\r\n";
    oss << "Cache-Control: no-cache, no-store, must-revalidate\r\n";
    oss << "Pragma: no-cache\r\n";
    oss << "Expires: 0\r\n";
    oss << "Connection: close\r\n\r\n";

    std::string headers = oss.str();
    send(client_sock, headers.c_str(), headers.size(), 0);
    send(client_sock, image_data.data(), image_data.size(), 0);
}


//  ===  Захват экрана через X11 и конвертация в OpenCV-матрицу   ===
cv::Mat captureScreen(Display* disp, int width, int height, int x, int y) 
{
    // Display* display = XOpenDisplay(":0");
    Window root = DefaultRootWindow(disp);

    // Получаем размеры экрана
    XWindowAttributes attributes;
    XGetWindowAttributes(disp, root, &attributes);

    
    //750x487 (x=1158, y=59)
    // Создаем XImage
    XImage* img = XGetImage(disp, root, x, y, width, height, AllPlanes, ZPixmap);
    if (!img) 
    {
        // XCloseDisplay(disp);
        throw std::runtime_error("Не удалось создать XImage");
    }

    // Конвертируем XImage в OpenCV Mat (BGR формат)
    cv::Mat mat(height, width, CV_8UC4, img->data);
    cv::Mat bgrMat;
    cv::cvtColor(mat, bgrMat, cv::COLOR_BGRA2BGR);  // Убираем альфа-канал

    // Освобождаем ресурсы
    XDestroyImage(img);

    return bgrMat;
}

//  ===  Получение полный список всех открытых окон  ===
Window* getWindowList(Display* disp, unsigned long* len, Atom prop) 
{
    Atom type;
    int format;
    unsigned long remain;
    unsigned char* list;

    if (XGetWindowProperty(disp, XDefaultRootWindow(disp), prop, 0, 1024, False,
                           XA_WINDOW, &type, &format, len, &remain, &list) != Success) 
    {
        return nullptr;
    }

    return (Window*)list;
}

//  ===  Получение название окон  ===
std::string getWindowName(Display* disp, Window win, Atom netWmName, Atom utf8Str) 
{
    Atom type;
    int format;
    unsigned long len, bytes_after;
    unsigned char* prop = nullptr;

    int status = XGetWindowProperty(
        disp, win, netWmName, 0, (~0L), False, utf8Str,
        &type, &format, &len, &bytes_after, &prop
    );

    std::string name;
    if (status == Success && prop) 
    {
        name = std::string((char*)prop);
        XFree(prop);
    } else 
    {
        name = "<без имени>";
    }

    return name;
}

//  ===  Получение открытых  ===
int getWindowWorkspace(Display* disp, Window win) 
{
    Atom wsAtom = XInternAtom(disp, "_NET_WM_DESKTOP", True);
    Atom type;
    int format;
    unsigned long len, bytes_after;
    unsigned char* prop = nullptr;

    if (XGetWindowProperty(disp, win, wsAtom, 0, 1, False,
                           XA_CARDINAL, &type, &format, &len,
                           &bytes_after, &prop) == Success && prop) 
    {
        int ws = *(unsigned long*)prop;
        XFree(prop);
        return ws;
    }

    return -1; // неизвестно
}

//  ===  Получение кардинат окон   ===
std::vector<int> getWindowGeometry(Display* disp, Window win) 
{
    int x, y, width, height;
    XWindowAttributes attr;

    if (XGetWindowAttributes(disp, win, &attr)) 
    {
        Window child;
        int abs_x, abs_y;
        XTranslateCoordinates(disp, win, XDefaultRootWindow(disp), 0, 0, &abs_x, &abs_y, &child);

        x       =  abs_x;
        y       =  abs_y;
        width   =  attr.width;
        height  =  attr.height;
    } else {
        x = y = width = height = -1;
    }

    return { x, y, width, height };
}

//  ===  Получение кардинат окон   ===
std::vector<int> getFullWindowGeometry(Display* disp) 
{
    // Получаем ширину и высоту экрана
    int screen = DefaultScreen(disp);
    int width = DisplayWidth(disp, screen);
    int height = DisplayHeight(disp, screen);


    return { 0, 0, width, height };
}
