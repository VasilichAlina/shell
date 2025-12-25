#include <iostream>
#include <string>
#include <fstream>
#include <unistd.h>
#include <sys/wait.h>
#include <vector>
#include <sstream>
#include <signal.h>
#include <cstring>
#include <sys/stat.h>

// Глобальный флаг для сигнала SIGHUP
volatile sig_atomic_t g_sighup_received = 0;

// Обработчик сигнала SIGHUP
void sighup_handler(int signal_number)
{
    if (signal_number == SIGHUP)
    {
        g_sighup_received = 1;
    }
}

// Функция для поиска команды в PATH
std::string find_in_path(const std::string& command)
{
    char* path_env = getenv("PATH");
    if (!path_env) return "";

    std::string path_str = path_env;
    std::stringstream path_stream(path_str);
    std::string dir;

    while (std::getline(path_stream, dir, ':'))
    {
        std::string full_path = dir + "/" + command;
        if (access(full_path.c_str(), X_OK) == 0)
        {
            return full_path;
        }
    }

    return "";
}

void execute_external(const std::vector<std::string>& tokens)
{
    if (tokens.empty()) return;

    pid_t pid = fork();

    if (pid == 0)
    {
        std::vector<char*> args;
        for (const auto& t : tokens)
        {
            args.push_back(const_cast<char*>(t.c_str()));
        }
        args.push_back(nullptr);

        // Пробуем выполнить команду через execvp
        execvp(args[0], args.data());

        // Если execvp не сработал, ищем команду в PATH
        std::string full_path = find_in_path(tokens[0]);
        if (!full_path.empty())
        {
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

// Функция для вывода переменной окружения
void print_env_var(const std::string& var_name)
{
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
}

// Функция для вывода истории команд
void show_history(const std::string& history_path)
{
    std::ifstream history_file(history_path);
    if (!history_file.is_open())
    {
        return;
    }

    std::string line;
    int line_num = 1;
    while (std::getline(history_file, line))
    {
        std::cout << line_num << "  " << line << "\n";
        line_num++;
    }
    history_file.close();
}

int main()
{
    
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;
    signal(SIGHUP, sighup_handler);

    
    const char* home = std::getenv("HOME");
    if (!home)
    {
        std::cerr << "ERROR: HOME environment variable not set\n";
        return 1;
    }

    std::string history_path = std::string(home) + "/.kubsh_history";
    std::string input;

    while (true)
    {
        // Проверяем, был ли получен сигнал SIGHUP
        if (g_sighup_received)
        {
            std::cout << "\nConfiguration reloaded\n";
            g_sighup_received = 0;
            // Перерисовываем приглашение
            std::cout << "$ ";
            fflush(stdout);
        }

       
        std::cout << "$ ";

        
        if (!std::getline(std::cin, input))
        {
            
            std::cout << "\n";
            break;
        }

        
        if (input.empty())
        {
            continue;
        }

        std::ofstream history_file(history_path, std::ios::app);
        if (history_file.is_open())
        {
            history_file << input << "\n";
            history_file.close();
        }

        if (input == "\\q")
        {
            break;
        }

        if (input == "history")
        {
            show_history(history_path);
            continue;
        }

 
        if (input.find("echo ") == 0)
        {
            std::string text = input.substr(5);
            std::cout << text << "\n";
            continue;
        }

        
        if (input.size() >= 9 && input.substr(0, 7) == "debug '" && input.back() == '\'')
        {
            std::string text = input.substr(7, input.size() - 8);
            std::cout << text << "\n";
            continue;
        }

        if (input.find("\\e $") == 0)
        {
            std::string var_name = input.substr(4);
            print_env_var(var_name);
            continue;
        }

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
    }

    return 0;
}