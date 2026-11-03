#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/**
 * Level 2: Debugging Inconsistencies
 * 
 * Your task:
 * 1. Identify and fix code inconsistencies.
 * 2. Address the mixed coding styles (indentation, naming).
 * 3. Find potential memory leaks or undefined behaviors.
 * 4. Ensure error handling is consistent.
 */

// Global constant
#define MAX_SIZE 1000

// Inconsistent naming: CamelCase and snake_case
void SwapElements(int *a, int *b) {
    int t = *a;
    *a = *b;
    *b = t;
}
// hello world
int partition_array(int arr[], int low, int high) {
    int pivot = arr[high];
    int i = (low - 1);

    for (int j = low; j <= high - 1; j++) {
        if (arr[j] < pivot) {
            i++;
            SwapElements(&arr[i], &arr[j]);
        }
    }
    SwapElements(&arr[i + 1], &arr[high]);
    return (i + 1);
}

// Inconsistent function style
void QuickSort(int arr[], int low, int high) {
    if (low < high) {
        int pi = partition_array(arr, low, high);
        QuickSort(arr, low, pi - 1);
        QuickSort(arr, pi + 1, high);
    }
}

void printArray(int arr[], int size) {
    for (int i = 0; i < size; i++)
        printf("%d ", arr[i]);
    printf("\n");
}

int main() {
    int *data = (int *)malloc(10 * sizeof(int));
    
    // Hardcoded test
    data[0] = 10; data[1] = 7; data[2] = 8; data[3] = 9; data[4] = 1;
    data[5] = 5; data[6] = 2; data[7] = 3; data[8] = 4; data[9] = 6;

    printf("Original array: \n");
    printArray(data, 10);

    QuickSort(data, 0, 9);
    
    printf("Sorted array: \n");
    printArray(data, 10);

    // Missing free!
    return 0;
}
