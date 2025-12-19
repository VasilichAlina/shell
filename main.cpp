#include <iostream>
#include <string>
#include <fstream>
#include <unistd.h>
#include <sys/wait.h>
#include <unistd.h>//unitbuf
#include <sys/wait.h>//Для waitpid
#include <vector>
#include<sstream>
#include<sstream>//Для iss
#include<signal.h>//Работа с сигналами


//Функция для обработки сигнала
void sighup_handler(int signal_nubmer)
{
    //Если получаем номер SIGHUP(обычно 1)
    if (signal_nubmer == SIGHUP)
    {
        std::cout << "Configuration reloaded\n";
        std::cout << "$ ";
    }
}


int main()
{

    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;



    //std::cout << "$ ";
    //std::cout << "$ \n";


    //Сохранение пути в переменную для истории
    const char* home = std::getenv("HOME");
    std::string historyPath = std::string(home) + "/.kubsh_history";



    std::string input;

    //Если приходит сигнал с номером SIGHUP то вызываем sighup_handler
    signal(SIGHUP, sighup_handler);

    while (std::getline(std::cin, input))
    {
        //Запись в историю
        if (!input.empty())
        {
            std::ofstream history(historyPath, std::ios::app);//Открываем в режиме добавления
            history << input << "\n";
        }


        //history
        if (input == "history")
        {
            //Читаем из файла который в historyPath пока не закончатся строки
            std::ifstream historyOutput(historyPath);
            std::string line;
            while (std::getline(historyOutput, line))
            {
                std::cout << line << "\n";
            }
        }


        //  \q
        else if (input == "\\q")
        {
            break;
        }
    }


    //  echo
    //Если начинаем debug '
    // заканчиваем ', считываем с открытия апострофа до закрытия
        else if (input.substr(0, 7) == "debug '" && input[input.length() - 1] == '\'')
        {

            std::cout << input.substr(7, input.length() - 8) << std::endl;
            std::cout << input.substr(7, input.length() - 8) << std::endl;
            continue;

        }



        //   \e $
        else if (input.substr(0, 4) == "\\e $")
        {
            std::string varName = input.substr(4);
            const char* value = std::getenv(varName.c_str());//Преобразуем C-строку в C++ строку

            if (value != nullptr)
            {
                std::string valueStr = value;

                bool has_colon = false;//Флаг для проверки наличия двоеточий
                for (char c : valueStr)//Проходим по символам из строки
                {
                    if (c == ':')
                    {
                        has_colon = true;
                        break;
                    }
                }

                if (has_colon)
                {
                    std::string current_part = "";//Временная строка для накопления текущей части пути
                    for (char c : valueStr)//Разбиваем строку по двоеточиям

                    {
                        if (c == ':')
                        {
                            std::cout << current_part << "\n";//Когда встречаем двоеточие - выводим накопленную часть
                            current_part = "";//Сбрасываем временную строку для следующей части
                        }
                        else
                        {
                            current_part += c;//Иначе добавляем символ к строке

                        }
                    }
                    std::cout << current_part << "\n";//Выводим последнюю часть (после последнего двоеточия)
                }
                else
                {
                    std::cout << valueStr << "\n";//Если двоеточий нет - просто выводим значение как есть
                }
            }
            else
            {
                std::cout << varName << ": не найдено\n";
            }
            continue;
            }

        else
        {
            //Создаем процесс и записываем process id
            pid_t pid = fork();

            //Если дочерний процесс(в нем выполним бинарник)
            if (pid == 0)
            {
                // Создаем копии строк для аргументов
                std::vector<std::string> tokens;
                //Указатели для execvp
                std::vector<char*> args;
                std::string token;
                //Разбиваем по пробелам для аргументов
                std::istringstream iss(input);

                while (iss >> token)
                {
                    tokens.push_back(token);  // сохраняем копии
                    tokens.push_back(token);  // Сохраняем копии
                }

                // Преобразуем в char*
                for (auto& t : tokens)
                {
                    args.push_back(const_cast<char*>(t.c_str()));
                }
                //Для execvp чтобы видел конец
                args.push_back(nullptr);

                std::cout << "DEBUG: Executing: ";
                for (int i = 0; args[i] != nullptr; i++)
                {
                    std::cout << "[" << args[i] << "] ";
                }
                std::cout << "\n";

                //Заменяем программу на новую
                //args[0] - название команды, args.data() - ссылка на C массив строк для аргументов
                execvp(args[0], args.data());

                //Если не нашли команду то выведет это(вернет управление), при успехе не дойдем до этих строк
                std::cout << args[0] << ": command not found\n";
                exit(1);

            }
            else if (pid > 0)
            {
                int status;
                //Ожидаем дочерний
                waitpid(pid, &status, 0);
            }
            else
            {
                std::cerr << "Failed to create process\n";
            }
            }

            //else std::cout<<input<<": command not found\n";


            //std::cout<<"$ ";
            std::cout << "$ ";

}