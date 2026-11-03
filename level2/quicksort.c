#include <stddef.h>
#include <stdio.h>

static void swap_elements(int *left, int *right) {
    int temporary = *left;
    *left = *right;
    *right = temporary;
}

/* Partition arr[low, high) and return the pivot's final index. */
static size_t partition_array(int arr[], size_t low, size_t high) {
    int pivot = arr[high - 1];
    size_t smaller_end = low;

    for (size_t current = low; current + 1 < high; current++) {
        if (arr[current] < pivot) {
            swap_elements(&arr[smaller_end], &arr[current]);
            smaller_end++;
        }
    }

    swap_elements(&arr[smaller_end], &arr[high - 1]);
    return smaller_end;
}

static void quick_sort_range(int arr[], size_t low, size_t high) {
    if (high - low < 2) {
        return;
    }

    size_t pivot_index = partition_array(arr, low, high);
    quick_sort_range(arr, low, pivot_index);
    quick_sort_range(arr, pivot_index + 1, high);
}

static void quick_sort(int arr[], size_t size) {
    if (arr == NULL || size < 2) {
        return;
    }

    quick_sort_range(arr, 0, size);
}

static void print_array(const int arr[], size_t size) {
    for (size_t i = 0; i < size; i++) {
        printf("%d%s", arr[i], i + 1 == size ? "" : " ");
    }
    putchar('\n');
}

int main(void) {
    int data[] = {10, 7, 8, 9, 1, 5, 2, 3, 4, 6};
    size_t size = sizeof data / sizeof data[0];

    printf("Original array:\n");
    print_array(data, size);

    quick_sort(data, size);

    printf("Sorted array:\n");
    print_array(data, size);
    return 0;
}

