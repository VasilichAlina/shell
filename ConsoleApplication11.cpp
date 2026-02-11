// ConsoleApplication11.cpp : This file contains the 'main' function. Program execution begins and ends there.
//


#include <iostream>
#include <cmath>
#include <iomanip>

using namespace std;
const double pi = 3.14159265;

int fact(int x)
{    int p = 1;

    if (x == 0) return 1;
    else
        for (int i = 1; i < x; i++)
        {
            p *= i;
        }
    return p;

}

double func(double x)
{
    double sqrt_pi = sqrt(pi);
    double k = 2 / sqrt_pi;

    int n = 0;
    double term = x;
    double sum=0.0;

    while (true)
    {
  

        double term = (pow(-1, n) * pow(x, 2 * n + 1)) / (fact(n) * (2 * n + 1));
        double p_sum = sum;
        sum += term;

        if (p_sum == sum) break;
      n++;
    }
    return k * sum;
}


int main()
{
    setlocale(LC_ALL,"rus");

    double test_x[] = {0.5, 1.0, 5.0, 10.0};
    int n = 4; 
    for (int i = 0; i < n; i++)
    {
        double myfunc = func(test_x[i]);
        double realfunc = erf(test_x[i]);

        cout << "Ряд вычислен с помощью функции, х = " << test_x[i] << ", = " << myfunc << endl;
        cout << "Ряд вычислен с помощью встроенной функции, х = " << test_x[i] << ", = " << realfunc << endl;
        cout << "Погрешность =  " << myfunc - realfunc << endl;

    }

}
