#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "01_quick_sort_cases.h"
#define Max(a, b) ((a) >= (b) ? (a) : (b));

int ed(const int arr[]){
    return  (arr == qs1) ? sizeof(qs1) / sizeof(qs1[0]) - 1 :
            (arr == qs2) ? sizeof(qs2) / sizeof(qs2[0]) - 1 :
            (arr == qs3) ? sizeof(qs3) / sizeof(qs3[0]) - 1 :
            (arr == qs4) ? sizeof(qs4) / sizeof(qs4[0]) - 1 :
                                                        - 1;

}

void basicSort(int arr[], const int st, const int mid, const int ed){
    if (arr[st] >= arr[mid]){
        int tmp = arr[st];
        arr[st] = arr[mid];
        arr[mid] = tmp;
    } if (arr[mid] >= arr[ed]){
        int tmp = arr[mid];
        arr[mid] = arr[ed];
        arr[ed] = tmp;
    } if (arr[st] >= arr[ed]){
        int tmp = arr[st];
        arr[st] = arr[ed];
        arr[ed] = tmp;
    }
}

void qSort(int arr[], int st, int ed){

    int mid = st + ed / 2;
    int pivot = arr[mid];
    while (st <= ed){
        st++;
        ed--;
        while (arr[st] < pivot){
            st++;
        }
        while (arr[ed] > pivot){
            ed--;
        }
        int tmp = arr[st];
        arr[st] = arr[ed];
        arr[ed] = tmp;
        if ((mid - st) >= (ed - mid)){
            qSort(arr, mid+1, end);
        }
        else {
            qsort(arr, st, mid);
        }
    }
}


int main(void){

    int st = 0;
    int ed1 = ed(qs1);
    int mid1 = ed(qs1) / 2;

    basicSort(qs1, st, mid1, ed1);
    qSort(qs1, st, ed1);


// Case 1 (before)
    printf("\n=== 테스트 케이스 1 ===\n");
    printf("정렬 전: [");
    for (int i = 0; i < ed(qs1); i++){
        printf("%d, ", qs1[i]);
    }   printf("%d", qs1[ed(qs1)]);
    printf("]\n");
// Case 1 (after)
    int copy1[ed(qs1) + 1];
    memcpy(copy1, qs1, ed(qs1) + 1);
    qSort(copy1, ed(copy1) + 1);
    printf("정렬 후: [");
    for (int i = 0; i < ed(copy1); i++){
        printf("%d, ", copy1[i]);
    }   printf("%d", copy1[ed(copy1)]);
    printf("]\n");
    return 0;
}
