// return: 0

// Exercises operators, char/int conversion, assignment, short-circuiting, and
// loops. Returns 0 if every check passes, otherwise the number of the first
// failing check. See everything_functions.c for the rest.

int bump(int *counter) {
    *counter = *counter + 1;
    return 1;
}

int main() {
    int a;
    int b;
    int i;
    int calls;
    char c;

    // arithmetic, precedence, and parentheses
    if (2 + 3 * 4 - 10 / 2 != 9) {
        return 1;
    }
    if ((2 + 3) * (4 - 10) != -30) {
        return 2;
    }
    if (100 - 30 - 20 != 50 || 100 / 10 / 5 != 2) {
        return 3;
    }

    // signed division and modulus
    a = -17;
    if (a / 5 != -3 || a % 5 != -2 || 17 % -5 != 2) {
        return 4;
    }

    // shifts
    if (1 << 10 != 1024 || 1024 >> 3 != 128) {
        return 5;
    }

    // signed comparisons
    a = -300;
    b = 200;
    if (!(a < b) || a > b || !(a <= a) || !(b >= a) || b <= a) {
        return 6;
    }

    // unary operators
    if (-a != 300 || +b != 200 || !0 != 1 || !b != 0) {
        return 7;
    }

    // postfix ++ and -- return the old value
    a = 255;
    b = a++;
    if (b != 255 || a != 256) {
        return 8;
    }
    b = a--;
    if (b != 256 || a != 255) {
        return 9;
    }

    // chained assignment
    a = b = 1000;
    if (a != 1000 || b != 1000) {
        return 10;
    }

    // && and || short-circuit
    calls = 0;
    if (0 && bump(&calls)) {
        return 11;
    }
    if (!(1 || bump(&calls)) || calls != 0) {
        return 12;
    }
    if (!(1 && bump(&calls)) || calls != 1) {
        return 13;
    }

    // char arithmetic wraps at 8 bits and promotes to int
    c = 250;
    c = c + 10;
    if (c != 4) {
        return 14;
    }
    c = 200;
    if (c + 100 != 300) {
        return 15;
    }

    // while counting down with a compound condition
    i = 10;
    a = 0;
    while (i > 0 && a < 100) {
        a = a + i;
        i--;
    }
    if (a != 55 || i != 0) {
        return 16;
    }

    return 0;
}
