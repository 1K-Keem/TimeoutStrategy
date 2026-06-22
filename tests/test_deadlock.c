#include <stdio.h>
#include "../include/DeadlockDetector.h"

/* Demo chơi tay Wait-For Graph: thêm/xóa cạnh rồi in trạng thái deadlock. */

static void printDeadlock(
    DeadlockDetector *detector,
    const char *title)
{
    printf("\n=== %s ===\n", title);

    if (DeadlockDetector_detectDeadlock(detector))
    {
        printf("Có deadlock!\n");
    }
    else
    {
        printf("Không có deadlock.\n");
    }
}

int main(void)
{
    DeadlockDetector detector;

    DeadlockDetector_init(&detector);

    printf("Tạo đồ thị...\n");

    /*
        P1 -> P2
    */
    DeadlockDetector_addWaitRelation(
        &detector,
        "P1",
        "P2");

    printDeadlock(&detector, "Sau khi P1 chờ P2");

    /*
        P1 -> P2 -> P3
    */
    DeadlockDetector_addWaitRelation(
        &detector,
        "P2",
        "P3");

    printDeadlock(&detector, "Sau khi P2 chờ P3");

    /*
        P1 -> P2 -> P3 -> P1
        Tạo thành chu trình -> deadlock.
    */
    DeadlockDetector_addWaitRelation(
        &detector,
        "P3",
        "P1");

    printDeadlock(&detector, "Sau khi P3 chờ P1");

    printf("\n");

    printf("P1 có trong deadlock? %s\n",
           DeadlockDetector_isInDeadlock(
               &detector,
               "P1")
               ? "CÓ"
               : "KHÔNG");

    printf("P2 có trong deadlock? %s\n",
           DeadlockDetector_isInDeadlock(
               &detector,
               "P2")
               ? "CÓ"
               : "KHÔNG");

    printf("P3 có trong deadlock? %s\n",
           DeadlockDetector_isInDeadlock(
               &detector,
               "P3")
               ? "CÓ"
               : "KHÔNG");

    /*
        Gỡ cạnh P3 -> P1 để phá chu trình.
    */
    DeadlockDetector_removeWaitRelation(
        &detector,
        "P3",
        "P1");

    printDeadlock(
        &detector,
        "Sau khi gỡ P3 -> P1");

    DeadlockDetector_destroy(&detector);

    return 0;
}
