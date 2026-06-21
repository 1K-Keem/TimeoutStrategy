#include <stdio.h>
#include "../include/DeadlockDetector.h"

static void printDeadlock(
    DeadlockDetector *detector,
    const char *title)
{
    printf("\n=== %s ===\n", title);

    if (DeadlockDetector_detectDeadlock(detector))
    {
        printf("Deadlock detected!\n");
    }
    else
    {
        printf("No deadlock.\n");
    }
}

int main(void)
{
    DeadlockDetector detector;

    DeadlockDetector_init(&detector);

    printf("Create graph...\n");

    /*
        P1 -> P2
    */
    DeadlockDetector_addWaitRelation(
        &detector,
        "P1",
        "P2");

    printDeadlock(&detector, "After P1 waits P2");

    /*
        P1 -> P2 -> P3
    */
    DeadlockDetector_addWaitRelation(
        &detector,
        "P2",
        "P3");

    printDeadlock(&detector, "After P2 waits P3");

    /*
        P1 -> P2 -> P3 -> P1

        cycle
    */
    DeadlockDetector_addWaitRelation(
        &detector,
        "P3",
        "P1");

    printDeadlock(&detector, "After P3 waits P1");

    printf("\n");

    printf("P1 in deadlock? %s\n",
           DeadlockDetector_isInDeadlock(
               &detector,
               "P1")
               ? "YES"
               : "NO");

    printf("P2 in deadlock? %s\n",
           DeadlockDetector_isInDeadlock(
               &detector,
               "P2")
               ? "YES"
               : "NO");

    printf("P3 in deadlock? %s\n",
           DeadlockDetector_isInDeadlock(
               &detector,
               "P3")
               ? "YES"
               : "NO");

    /*
        Remove edge

        P3 -> P1
    */
    DeadlockDetector_removeWaitRelation(
        &detector,
        "P3",
        "P1");

    printDeadlock(
        &detector,
        "After removing P3 -> P1");

    DeadlockDetector_destroy(&detector);

    return 0;
}
