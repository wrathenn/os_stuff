/* Определить нормальный вес человека и индекс массы его тела по формулам: h * t / 240 и m / h^2, 
где h - рост человека (измеряемый в сантиметрах в первой формуле и в метрах - во второй);
t - длина окружности грудной клетки (в сантиметрах); m - вес (в килограммах). 
Порядок ввода параметров: h, t, m (h в см, t в см, m в кг) */

#include <stdio.h>

int main(int argc, char* argv[])
{
    for (int i = 0; i < argc; ++i) {
        printf("Аргумент №%d = %s\n", i, argv[i]);
    }

    float h, t, m;

    printf("Введите через пробел рост в см, длину окр. грудной клетки в см и вес в кг:\n");
    scanf("%f %f %f", &h, &t, &m);

    float weight = h * t / 240;
    float index = m / (h / 100) / (h / 100);

    printf("%.5f %.5f\n", weight, index);

    return 0;
}