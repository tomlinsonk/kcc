// return: 0

// Shifting by 0 leaves the value unchanged; shifting by the bit width or more gives 0.
// Returns 0 if every check passes, otherwise the number of the first failing check.
int main() {
    int x;
    int n;
    char c;
    char m;

    x = 1234;
    n = 0;
    if (x << 0 != 1234 || x >> 0 != 1234 || x << n != 1234) {
        return 1;
    }

    c = 77;
    m = 0;
    if (c << m != 77 || c >> m != 77) {
        return 2;
    }

    n = 16;
    m = 8;
    if (x << n != 0 || x >> n != 0 || c << m != 0 || c >> m != 0) {
        return 3;
    }

    n = 3;
    if (x << n != 9872 || x >> n != 154 || (x + 1) << n != 9880) {
        return 4;
    }

    return 0;
}
