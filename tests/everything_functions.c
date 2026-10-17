// return: 0
// screen: kcc -42 720 ok

// Exercises functions, recursion, arrays, pointers, strings, and output.
// Returns 0 if every check passes, otherwise the number of the first failing
// check. See everything_operators.c for the rest.

int square(int x) {
    return x * x;
}

int fact(int n) {
    if (n <= 1) {
        return 1;
    }
    return n * fact(n - 1);
}

int gcd(int a, int b) {
    int t;

    while (b != 0) {
        t = a % b;
        a = b;
        b = t;
    }
    return a;
}

int sign(int x) {
    if (x < 0) {
        return -1;
    } else if (x == 0) {
        return 0;
    } else {
        return +1;
    }
}

int sum_digits(int n) {
    int sum;

    if (n < 0) {
        n = -n;
    }
    sum = 0;
    while (n > 0) {
        sum = sum + n % 10;
        n = n / 10;
    }
    return sum;
}

void swap(int *a, int *b) {
    int t;

    t = *a;
    *a = *b;
    *b = t;
}

void selection_sort(int *arr, int n) {
    int i;
    int j;
    int min;

    for (i = 0; i < n - 1; i++) {
        min = i;
        for (j = i + 1; j < n; j++) {
            if (arr[j] < arr[min]) {
                min = j;
            }
        }
        swap(&arr[i], &arr[min]);
    }
}

int is_sorted(int *arr, int n) {
    int i;

    for (i = 1; i < n; i++) {
        if (arr[i - 1] > arr[i]) {
            return 0;
        }
    }
    return 1;
}

int count_primes(int limit) {
    char composite[50];
    int i;
    int j;
    int count;

    for (i = 0; i < limit; i++) {
        composite[i] = 0;
    }

    count = 0;
    for (i = 2; i < limit; i++) {
        if (!composite[i]) {
            count++;
            for (j = i * i; j < limit; j = j + i) {
                composite[j] = 1;
            }
        }
    }
    return count;
}

int str_len(char *s) {
    int n;

    n = 0;
    while (s[n] != '\0') {
        n++;
    }
    return n;
}

void print_int(int n) {
    if (n < 0) {
        putchar('-');
        n = -n;
    }
    if (n >= 10) {
        print_int(n / 10);
    }
    putchar('0' + n % 10);
}

int main() {
    int arr[8];
    int a;
    int i;
    int *p;
    char *s;

    // function calls and recursion
    if (square(-12) != 144 || fact(6) != 720 || gcd(84, 36) != 12) {
        return 1;
    }
    if (sign(-5) != -1 || sign(0) != 0 || sign(7) != 1) {
        return 2;
    }
    if (sum_digits(-9876) != 30) {
        return 3;
    }

    // arrays, pointers, and passing arrays to functions
    for (i = 0; i < 8; i++) {
        arr[i] = (i * 5) % 8 - 4;
    }
    selection_sort(arr, 8);
    if (!is_sorted(arr, 8) || arr[0] != -4 || arr[7] != 3) {
        return 4;
    }

    p = &a;
    *p = 42;
    if (a != 42) {
        return 5;
    }
    p = arr;
    if (p[2] != -2) {
        return 6;
    }

    // char arrays
    if (count_primes(50) != 15) {
        return 7;
    }

    // strings and char literals
    s = "hello";
    if (str_len(s) != 5 || s[1] != 'e' || s[4] - 'a' != 14) {
        return 8;
    }

    // output
    puts("kcc ");
    print_int(-42);
    putchar(' ');
    print_int(fact(6));
    putchar(' ');
    puts("ok");

    return 0;
}
