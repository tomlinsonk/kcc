// return: 111

int main() {
    int n;
    int i;

    n = 27;
    i = 0;

    while (n != 1) {
        i++;
        if (n % 2 == 0) {       
            n = n / 2;  
        } else {
            n = 3 * n + 1;
        }
    }

    return i;
}