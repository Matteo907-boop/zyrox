//#include <stdio.h>
//#include <stdint.h>
//
//int __test_impl()
//{
//    for (int i = 0; i < 10; i++)
//    {
//       printf("i: %d\n", i + 2 - i * 2);
//    }
//    return 5 + 3;
//}
//
//int __test_fn(int x)
//{
//    if (x == 2) {
//        printf("x is 2\n");
//    } else {
//        printf("x is not 2!, x is: %d\n", x);
//    }
//    return x + 4 * x - 2 / 4;
//}
//
//int main()
//{
//    printf("/stack:hello, world!\n");
//    int i = 2;
//    __test_fn(i);
//    if (i == 2){
//        switch (i)
//        {
//        case 2:
//            __test_impl();
//            return 0;
//        case 1:
//            return 1;
//        }
//    }
//}

#include <stdio.h>

__attribute__((annotate("mba:2 bbs:1,2,5,100 sibr:1,100")))  int XOR(int a, int b) {
    return a ^ b;
}

int main() {
    if (XOR(5, 7) == 2) {
        printf("result is 2\n");
    } else {
        printf("result is not 2\n");
    }
    return 0;
}