// screen: 0 2 4 1 3 
// screen: 0 1 2 3 4 

void print_arr(int *arr, int n) {
    int i;

    for (i = 0; i < n; i++) {
        putchar('0' + arr[i]);
        putchar(' ');
    }
    putchar('\n');
}

void insertion_sort(int *arr, int n) {
    int tmp;
    int j;
    int i;

    for (i = 1; i < n; i++) {
        j = i;
        while (j > 0 && arr[j-1] > arr[j]) {
            tmp = arr[j];
            arr[j] = arr[j-1];
            arr[j-1] = tmp;
            j--;
        }
    }
}

int main() {
    int arr[7];
    int n;
    int i;

    n = 5;

    for (i = 0; i < n; i++) {
        arr[i] = (7 * i) % 5;
    }

    print_arr(arr, n);
    insertion_sort(arr, n);
    print_arr(arr, n);
}
