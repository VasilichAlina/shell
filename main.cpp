#include <iostream>
#include <string>
#include <fstream>
#include <unistd.h>
#include <sys/wait.h>
#include <vector>
#include <sstream>
#include <signal.h>
#include <cstring>
#include <cstdint>
#include <sys/stat.h>
#include <cctype>

// Подключаем твой VFS
#include "vfs.hpp"

// ========== ПУНКТ 9: Сигналы ==========
volatile sig_atomic_t g_sighup_received = 0;

void sighup_handler(int signal_number)
{
    if (signal_number == SIGHUP)
    {
        g_sighup_received = 1;
    }
}

// ========== ПУНКТ 10: Анализ диска ==========
void check_disk_partitions(const std::string& device_path)
{
    std::ifstream device(device_path, std::ios::binary);

    if (!device)
    {
        std::cout << "Error: Cannot open device " << device_path << "\n";
        return;
    }

    char sector[512];
    device.read(sector, 512);

    if (device.gcount() != 512)
    {
        std::cout << "Error: Cannot read disk\n";
        return;
    }

    // Проверяем сигнатуру MBR
    if ((unsigned char)sector[510] != 0x55 || (unsigned char)sector[511] != 0xAA)
    {
        std::cout << "Error: Invalid disk signature\n";
        return;
    }

    // Определяем тип диска
    bool is_gpt = false;
    for (int i = 0; i < 4; i++)
    {
        if ((unsigned char)sector[446 + i * 16 + 4] == 0xEE)
        {
            is_gpt = true;
            break;
        }
    }

    if (!is_gpt)
    {
        // MBR
        for (int i = 0; i < 4; i++)
        {
            int offset = 446 + i * 16;
            unsigned char type = sector[offset + 4];

            if (type != 0)
            {
                uint32_t num_sectors = *(uint32_t*)&sector[offset + 12];
                uint32_t size_mb = num_sectors / 2048;
                bool bootable = ((unsigned char)sector[offset] == 0x80);

                std::cout << "Partition " << (i + 1) << ": Size=" << size_mb << "MB, Bootable: ";
                std::cout << (bootable ? "Yes" : "No") << "\n";
            }
        }
    }
    else
    {
        // GPT
        device.read(sector, 512);
        if (device.gcount() == 512 &&
            sector[0] == 'E' && sector[1] == 'F' && sector[2] == 'I' &&
            sector[3] == ' ' && sector[4] == 'P' && sector[5] == 'A' &&
            sector[6] == 'R' && sector[7] == 'T')
        {
            uint32_t num_partitions = *(uint32_t*)&sector[80];
            std::cout << "GPT partitions: " << num_partitions << "\n";
        }
        else
        {
            std::cout << "GPT partitions: unknown\n";
        }
    }
}

// ========== ПУНКТ 8: Внешние команды ==========
void execute_external(const std::vector<std::string>& tokens)
{
    if (tokens.empty()) return;

    pid_t pid = fork();

    if (pid == 0)
    {
        // ГАРАНТИРУЕМ PATH в дочернем процессе
        setenv("PATH", "/bin:/usr/bin:/usr/local/bin:/sbin:/usr/sbin", 1);

        std::vector<char*> args;
        for (const auto& t : tokens)
        {
            args.push_back(const_cast<char*>(t.c_str()));
        }
        args.push_back(nullptr);

        // Пробуем выполнить
        execvp(args[0], args.data());

        // Если не получилось - пробуем явные пути
        std::string paths[] = { "/bin/", "/usr/bin/", "/usr/local/bin/" };
        for (const auto& prefix : paths)
        {
            std::string full_path = prefix + tokens[0];
            execv(full_path.c_str(), args.data());
        }

        std::cerr << tokens[0] << ": command not found\n";
        exit(1);
    }
    else if (pid > 0)
    {
        int status;
        waitpid(pid, &status, 0);
    }
    else
    {
        std::cerr << "Failed to create process\n";
    }
}

// ========== ПУНКТ 5: Echo с раскрытием переменных ==========
void execute_echo(const std::string& input)
{
    std::string text = input.substr(5); // Убираем "echo "
    std::string result = "";

    for (size_t i = 0; i < text.size(); i++)
    {
        if (text[i] == '$' && i + 1 < text.size())
        {
            // Извлекаем имя переменной
            size_t start = i + 1;
            size_t end = start;

            while (end < text.size() &&
                (isalnum(text[end]) || text[end] == '_'))
            {
                end++;
            }

            std::string var_name = text.substr(start, end - start);
            const char* var_value = getenv(var_name.c_str());

            if (var_value)
            {
                result += var_value;
                i = end - 1;
            }
            else
            {
                result += text[i];
            }
        }
        else
        {
            result += text[i];
        }
    }

    std::cout << result << "\n";
}

// ========== ОСНОВНАЯ ФУНКЦИЯ ==========
int main()
{
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    // ПУНКТ 9: Обработчик сигнала
    struct sigaction sa;
    sa.sa_handler = sighup_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGHUP, &sa, NULL);

    // ГАРАНТИРУЕМ PATH
    setenv("PATH", "/bin:/usr/bin:/usr/local/bin:/sbin:/usr/sbin", 0);

    // ПУНКТ 11: Запускаем VFS в отдельном потоке
    std::cout << "[INFO] Starting Users VFS...\n";
    fuse_start();
    sleep(1); // Даем время FUSE запуститься

    const char* home = std::getenv("HOME");
    if (!home)
    {
        std::cerr << "ERROR: HOME environment variable not set\n";
        return 1;
    }

    // Создаем символическую ссылку на /opt/users если нужно
    std::string users_dir = "/opt/users";
    struct stat st;
    if (stat(users_dir.c_str(), &st) == -1) {
        std::cout << "[INFO] VFS mounted at /opt/users\n";
        std::cout << "[INFO] Access with: ls /opt/users/\n";
    }

    std::string history_path = std::string(home) + "/.kubsh_history";
    std::string input;

    while (true)
    {
        // ПУНКТ 9: Проверка сигнала
        if (g_sighup_received)
        {
            std::cout << "\nConfiguration reloaded\n";
            g_sighup_received = 0;
            std::cout << "$ ";
            fflush(stdout);
        }

        // Приглашение
        std::cout << "$ ";

        // Чтение ввода (пункт 2: выход по Ctrl+D)
        if (!std::getline(std::cin, input))
        {
            std::cout << "\n";
            break;
        }

        // Пропускаем пустые строки
        if (input.empty())
        {
            continue;
        }

        // ПУНКТ 4: Сохраняем в историю
        std::ofstream history_file(history_path, std::ios::app);
        if (history_file.is_open())
        {
            history_file << input << "\n";
            history_file.close();
        }

        // ========== ОБРАБОТКА КОМАНД ==========

        // ПУНКТ 3: Выход
        if (input == "\\q")
        {
            break;
        }

        // ПУНКТ 4: История
        if (input == "history")
        {
            std::ifstream history_output(history_path);
            std::string line;
            while (std::getline(history_output, line))
            {
                std::cout << line << "\n";
            }
            continue;
        }

        // ПУНКТ 5: Echo
        if (input.find("echo ") == 0)
        {
            execute_echo(input);
            continue;
        }

        // ПУНКТ 7: Переменные окружения
        if (input.find("\\e $") == 0)
        {
            std::string var_name = input.substr(4);
            const char* value = std::getenv(var_name.c_str());

            if (value != nullptr)
            {
                std::string value_str = value;

                if (value_str.find(':') != std::string::npos)
                {
                    std::stringstream ss(value_str);
                    std::string part;
                    while (std::getline(ss, part, ':'))
                    {
                        if (!part.empty())
                        {
                            std::cout << part << "\n";
                        }
                    }
                }
                else
                {
                    std::cout << value_str << "\n";
                }
            }
            else
            {
                std::cout << var_name << ": не найдено\n";
            }
            continue;
        }

        // ПУНКТ 10: Анализ диска
        if (input.substr(0, 3) == "\\l ")
        {
            std::string device_path = input.substr(3);
            device_path.erase(0, device_path.find_first_not_of(" \t"));
            device_path.erase(device_path.find_last_not_of(" \t") + 1);

            if (device_path.empty())
            {
                std::cout << "Usage: \\l /dev/device_name (e.g., \\l /dev/sda)\n";
            }
            else
            {
                check_disk_partitions(device_path);
            }
            continue;
        }

        // Команда debug (из твоего кода)
        if (input.size() >= 9 && input.substr(0, 7) == "debug '" && input.back() == '\'')
        {
            std::string text = input.substr(7, input.size() - 8);
            std::cout << text << "\n";
            continue;
        }

        // ПУНКТ 8: Внешние команды
        std::vector<std::string> tokens;
        std::stringstream iss(input);
        std::string token;

        while (iss >> token)
        {
            tokens.push_back(token);
        }

        if (!tokens.empty())
        {
            execute_external(tokens);
        }
        else
        {
            // ПУНКТ 6: Команда не найдена
            std::cout << input << ": command not found\n";
        }
    }

    return 0;
}