#include <stdio.h>

int factorial(int n) {
    if (n <= 1)
        return 1;
    return n * factorial(n - 1);
}


int main() {
    int result = factorial(5);
    printf("Factorial of 5 is %d\n", result);
    
    if (result > 100) {
        printf("Result is large\n");
    } else {
        printf("Result is small\n");
    }
    
    return 0;
}



/*
int main() {
    int result = 0;

    for (int i = 1; i <= 5; i++) {
        result = factorial(i);

        if (result > 100) {
            printf("Factorial of %d is large: %d\n", i, result);
        } else {
            printf("Factorial of %d is small: %d\n", i, result);
        }
    }

    return 0;
}
*/
