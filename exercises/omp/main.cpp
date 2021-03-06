#include "omp.h"
#include <iostream>
#include <cmath>

int main (void)
{
    const int iterations = 10;
#pragma omp parallel for
    for (int k = 0; k < iterations; k++) {
        const int num_steps = 100000;
        double x, sum = 0.0;
        const double step = 1.0 / double(num_steps);
#pragma omp parallel private (x)
#pragma omp parallel for reduction(+:sum)
        for (int i = 1; i <= num_steps; i++) {
            x = (i - 0.5) * step;
            sum += 4.0 / (1.0 + x * x);
        }
        const double pi = step * sum;
        std::cout << ">Pi is " << pi << std::endl;
    }
}
