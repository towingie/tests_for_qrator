// Дан массив различных целых чисел.
// Надо найти индекс локального минимума в этом массиве.
// Необходимо реализовать алгоритм с минимальной асимптотической сложностью.
// Локальный минимум - это такой элемент массива a_i, что a_{i-1} > a_i < a_{i+1}.
// Для начала и конца массива условие локального минимума, соответственно: a0 < a1 и a_n < a_{n-1}.

// build:
// gcc min.c -o min

#include <stdio.h>

int search(int arr[], int start, int end, int num_elems){

  int idx_middle_elem = start + (end - start)/2;

  //проверяем
  if (arr[idx_middle_elem] < arr[idx_middle_elem+1] && arr[idx_middle_elem] < arr[idx_middle_elem-1]){
        printf(" elem(%d) < next-elem(%d) &&", arr[idx_middle_elem], arr[idx_middle_elem+1]);
        printf(" elem(%d) < prev-elem(%d)\n",  arr[idx_middle_elem], arr[idx_middle_elem-1]);
    return idx_middle_elem;
  } else if (idx_middle_elem > 0 && arr[idx_middle_elem] > arr[idx_middle_elem-1] ){
    // идем налево
    printf(" to left: middle > 0 && %d > %d\n", arr[idx_middle_elem], arr[idx_middle_elem-1]);
    return search(arr, start, (idx_middle_elem-1), num_elems);
  } else { // идем направо
    printf(" to right: %d > %d, idx_middle_elem=%d\n", arr[idx_middle_elem], arr[idx_middle_elem-1], idx_middle_elem);
    return search(arr, (idx_middle_elem+1), end, num_elems);
  }
  printf("something terribly wrong\n");
}

int main(){
  int arr[] = { 9, 3, 30, 4, 5, 7, 38, 9, 22, 11, 23, 6, 2};
  int num_elems = sizeof(arr) / sizeof(arr[0]);
  printf("array is [ ");
  for(int i=0;i<num_elems;++i) {
    printf("%.d ", arr[i]);
  }
  printf("]\nelements in arr: %d\n", num_elems);
  int lmin = search(arr, 0, num_elems-1, num_elems);
  printf("localmin idx is %d, num at idx %d\n", lmin, arr[lmin]);

  return 0;
}
