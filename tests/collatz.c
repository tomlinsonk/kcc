// return: 111

int main() {
    int n;
    int i;

    n = 27;
    i = 0;

    while (n != 1) {
        i++;
        if (n << 15 == 0) {       // avoiding modulus
            n = n >> 1;           // avoiding division
        } else {
            n = n + n + n + 1;    // avoiding multiplication
        }
    }

    return i;
}