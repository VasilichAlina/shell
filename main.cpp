#include <iostream>
#include <string>
#include <fstream>
#include <unistd.h>//unitbuf
#include <sys/wait.h>//Для waitpid
#include <vector>
#include<sstream>//Для iss
#include<signal.h>//Работа с сигналами
#include <cstdint>//Для uint32_t 
#include <sys/stat.h>
#include "vfs.hpp"//Подключение VFS, в частности функция fuse_start

using namespace std;
void sighup_handler(int signal_number) {
    if (signal_number == SIGHUP) {
        cout << "\nConfiguration reloaded\n";
        cout << "$ ";
    }
}

//  \l /dev/sda
void check_disk_partitions(const std::string& device_path)
{
    //Создание потока для чтения файла, device_path - путь к устройству (в нашем случае /dev/sda)
    std::ifstream device(device_path, std::ios::binary);

    //Если ошибка открытия файла
    if (!device)
    {
        std::cout << "Error: Cannot open device " << device_path << "\n";
        return;
    }


    //Буфер на 512 байт
    char sector[512];
    //Читаем из нашего файла с диском первые 512 байт в буфер
    device.read(sector, 512);


    //Если прочитали не 512 байт(например меньше) - ошибка чтения
    if (device.gcount() != 512)
    {
        std::cout << "Error: Cannot read disk\n";
        return;
    }

    // Проверяем сигнатуру - 510 и 511 байты должны быть 0x55 и 0xAA соответственно, иначе это не MBR/GPT
    if ((unsigned char)sector[510] != 0x55 || (unsigned char)sector[511] != 0xAA)
    {
        std::cout << "Error: Invalid disk signature\n";
        return;
    }

    // Определяем тип диска
    bool is_gpt = false;
    //Идем начиная с 446 байта(начало таблицы разделов), всего 4 записи и каждая по 16 байт
    //Нас интересует тип записи(4 байт в записи)
    for (int i = 0; i < 4; i++)
    {
        //Если находим байт 0xEE это значит что он GPT Protective => это GPT
        if ((unsigned char)sector[446 + i * 16 + 4] == 0xEE)
        {
            is_gpt = true;
            break;
        }
    }

    if (!is_gpt)
    {
        // MBR - простой вывод
        for (int i = 0; i < 4; i++)
        {
            //Начало каждого из 4 разделов считаем для каждого прохода цикла
            int offset = 446 + i * 16;

            //Опять смотрим тип как в проверке на GPT
            unsigned char type = sector[offset + 4];

            //Раздел не существует если тип равен нулю
            if (type != 0) {
                //uint32_t - это беззнаковое 32-битное целое число
                //Читаем 12-15 байт раздела, отвечающий за количество секторов
                //Берем их(4 байта) как 32 битное число с помощью    *(uint32_t*)&sector
                uint32_t num_sectors = *(uint32_t*)&sector[offset + 12];
                //1 сектор - 512 байт, в 1 MB 1024*1024 байт => 2048 секторов
                uint32_t size_mb = num_sectors / 2048;
                //Если первый байт равен 0x80 то bootable
                bool bootable = ((unsigned char)sector[offset] == 0x80);

                std::cout << "Partition " << (i + 1) << ": Size=" << size_mb << "MB, Bootable: ";
                if (bootable)
                    std::cout << "Yes\n";
                else std::cout << "No\n";
            }
        }
    }

    else
    {
        //  GPT - просто количество разделов
        //Читаем из диска вторые 512 байт, в них хранится информация о GPT диске
        device.read(sector, 512);
        //Если прочли 512 (проверка как и раньше) и при этом байты 0-7 == "EFI PART"
        if (device.gcount() == 512 && sector[0] == 'E' && sector[1] == 'F' && sector[2] == 'I' && sector[3] == ' ' && sector[4] == 'P' && sector[5] == 'A' &&
            sector[6] == 'R' && sector[7] == 'T')
        {
            //Также переходим к чтению 4 байтов начиная с 80, тут количество записей в таблице разделов
            uint32_t num_partitions = *(uint32_t*)&sector[80];
            std::cout << "GPT partitions: " << num_partitions << "\n";
        }

        else
        {
            std::cout << "GPT partitions: unknown\n";
        }
    }
}

int main()
{

    cout << unitbuf;
    cerr << unitbuf;

    const char* home = getenv("HOME");
    if (!home) {
        cerr << "ERROR: HOME environment variable not set\n";
        return 1;
    }

    string users_dir = string(home) + "/users";
    cout << "[INFO] Creating users VFS at: " << users_dir << "\n";

    // Запускаем VFS
    start_users_vfs(users_dir);

    // ПРОВЕРКА: если VFS не создал директорию, создаем вручную
    struct stat st;
    if (stat(users_dir.c_str(), &st) == -1) {
        cerr << "[WARNING] VFS failed, creating directory manually...\n";
        mkdir(users_dir.c_str(), 0755);

        // Создаем тестового пользователя
        string test_dir = users_dir + "/testuser";
        mkdir(test_dir.c_str(), 0755);

        ofstream id_file(test_dir + "/id");
        id_file << "1000\n";
        id_file.close();

        ofstream home_file(test_dir + "/home");
        home_file << "/home/testuser\n";
        home_file.close();

        ofstream shell_file(test_dir + "/shell");
        shell_file << "/bin/bash\n";
        shell_file.close();
    }

    struct sigaction sa;
    sa.sa_handler = sighup_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGHUP, &sa, NULL) == -1) {
        cerr << "Warning: Failed to set SIGHUP handler\n";
    }

    cout << "$ ";

    string historyPath = string(home) + "/.kubsh_history";
    string input;

    // ИЗМЕНЕНИЕ: Проверка на Ctrl+D (EOF)
    while (getline(cin, input)) {
        // Запись в историю
        if (!input.empty()) {
            ofstream history(historyPath, ios::app);
            if (history.is_open()) {
                history << input << "\n";
                history.close();
            }
        }

        // history
        if (input == "history") {
            ifstream historyOutput(historyPath);
            string line;
            while (getline(historyOutput, line)) {
                cout << line << "\n";
            }
            cout << "$ ";
            continue;
        }

        // \q
        else if (input == "\\q") {
            break;
        }

        // \l /dev/sda
        else if (input.substr(0, 3) == "\\l ") {
            string device_path = input.substr(3);
            device_path.erase(0, device_path.find_first_not_of(" \t"));
            device_path.erase(device_path.find_last_not_of(" \t") + 1);

            if (device_path.empty()) {
                cout << "Usage: \\l /dev/device_name (e.g., \\l /dev/sda)\n";
            }
            else {
                check_disk_partitions(device_path);
            }
            cout << "$ ";
            continue;
        }

        // echo (debug)
        else if (input.substr(0, 7) == "debug '" && input[input.length() - 1] == '\'') {
            cout << input.substr(7, input.length() - 8) << endl;
            cout << "$ ";
            continue;
        }

        // \e $
        else if (input.substr(0, 4) == "\\e $") {
            string varName = input.substr(4);
            const char* value = getenv(varName.c_str());

            if (value != nullptr) {
                string valueStr = value;
                bool has_colon = false;

                for (char c : valueStr) {
                    if (c == ':') {
                        has_colon = true;
                        break;
                    }
                }

                if (has_colon) {
                    string current_part = "";
                    for (char c : valueStr) {
                        if (c == ':') {
                            cout << current_part << "\n";
                            current_part = "";
                        }
                        else {
                            current_part += c;
                        }
                    }
                    cout << current_part << "\n";
                }
                else {
                    cout << valueStr << "\n";
                }
            }
            else {
                cout << varName << ": не найдено\n";
            }
            cout << "$ ";
            continue;
        }

        // Выполнение внешних команд
        else {
            pid_t pid = fork();

            if (pid == 0) {
                // Устанавливаем PATH
                setenv("PATH", "/bin:/usr/bin:/usr/local/bin:/sbin:/usr/sbin", 1);

                vector<string> tokens;
                vector<char*> args;
                string token;
                istringstream iss(input);

                while (iss >> token) {
                    tokens.push_back(token);
                }

                for (auto& t : tokens) {
                    args.push_back(const_cast<char*>(t.c_str()));
                }
                args.push_back(nullptr);

                // Пробуем выполнить
                execvp(args[0], args.data());

                // Если не получилось, пробуем явные пути
                string explicit_paths[] = { "/bin/", "/usr/bin/", "/usr/local/bin/" };
                for (const auto& path_prefix : explicit_paths) {
                    string full_path = path_prefix + tokens[0];
                    execv(full_path.c_str(), args.data());
                }

                cout << args[0] << ": command not found\n";
                exit(1);
            }
            else if (pid > 0) {
                int status;
                waitpid(pid, &status, 0);
            }
            else {
                cerr << "Failed to create process\n";
            }
        }
        cout << "$ ";
    }

    // Выводим новую строку при выходе по Ctrl+D
    cout << "\n";

    stop_users_vfs();
    return 0;
}