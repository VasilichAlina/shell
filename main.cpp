#include <iostream>
#include <string>
#include <fstream>
#include <unistd.h>
#include <sys/wait.h>
#include <vector>
#include <sstream>
#include <signal.h>
#include <cstring>

// ========== ПУНКТ 9: Сигналы ==========
volatile sig_atomic_t g_sighup_received = 0;

void sighup_handler(int signal_number)
{
    if (signal_number == SIGHUP)
    {
        g_sighup_received = 1;  // ТОЛЬКО флаг, НЕ выводим здесь!
    }
}

void execute_external(const std::vector<std::string>& tokens)
{
    if (tokens.empty()) return;

    pid_t pid = fork();

    if (pid == 0)
    {
        // ДОЧЕРНИЙ ПРОЦЕСС: ГАРАНТИРУЕМ что PATH есть
        setenv("PATH", "/bin:/usr/bin:/usr/local/bin:/sbin:/usr/sbin", 1);

        // Подготавливаем аргументы для execvp
        std::vector<char*> args;
        for (const auto& t : tokens)
        {
            args.push_back(const_cast<char*>(t.c_str()));
        }
        args.push_back(nullptr);  // Конец массива

        execvp(args[0], args.data());

        std::string explicit_paths[] = { "/bin/", "/usr/bin/", "/usr/local/bin/" };
        for (const auto& path_prefix : explicit_paths)
        {
            std::string full_path = path_prefix + tokens[0];
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
        std::cerr << "Failed to create process (fork error)\n";
    }
}

void print_env_variable(const std::string& var_name)
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

void show_command_history(const std::string& history_path)
{
    std::ifstream history_file(history_path);
    if (!history_file.is_open())
    {
        return;
    }

    std::string line;
    int line_number = 1;
    while (std::getline(history_file, line))
    {
        std::cout << line_number << "  " << line << "\n";
        line_number++;
    }
    history_file.close();
}

int main()
{
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    signal(SIGHUP, sighup_handler);

    setenv("PATH", "/bin:/usr/bin:/usr/local/bin:/sbin:/usr/sbin", 0);

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
        if (g_sighup_received)
        {
            std::cout << "\nConfiguration reloaded\n";
            g_sighup_received = 0;
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
            show_command_history(history_path);
            continue;
        }

        if (input.find("echo ") == 0)
        {
            std::string text = input.substr(5);
            std::string result = "";

            for (size_t i = 0; i < text.size(); i++)
            {
                if (text[i] == '$' && i + 1 < text.size())
                {
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
            continue;
        }
        if (input.find("\\e $") == 0)
        {
            std::string var_name = input.substr(4);
            print_env_variable(var_name);
            continue;
        }

        if (input.size() >= 9 && input.substr(0, 7) == "debug '" && input.back() == '\'')
        {
            std::string text = input.substr(7, input.size() - 8);
            std::cout << text << "\n";
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