/* Test harness nhe, khong dung framework ngoai - thuan C */
#include "../include/CSVParser.h"
#include "../include/DeadlockDetector.h"
#include "../include/Models.h"
#include "../include/SimulationEngine.h"
#include "../include/TimeoutManager.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_failures = 0;
static int g_checks   = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        g_checks++;                                                        \
        if (!(cond)) {                                                     \
            g_failures++;                                                  \
            printf("  FAIL [%s:%d] %s\n", __FILE__, __LINE__, #cond);     \
        }                                                                  \
    } while (0)

/* Viet file CSV tam vao duong dan cho san */
static void writeTemp(const char *name, const char *content)
{
    FILE *fp = fopen(name, "w");
    if (!fp) { printf("Cannot open %s\n", name); exit(1); }
    fputs(content, fp);
    fclose(fp);
}

/* ------------------------------------------------------------------ */
/* testParser                                                           */
/* ------------------------------------------------------------------ */
static void testParser(void)
{
    printf("testParser\n"); fflush(stdout);

    writeTemp("tmp_parse_ok.csv",
        "time,process_id,action,resource_id,duration\n"
        "2,P2,request,R2,5\n"
        "0,P1,request,R1,3\n"
        "1,P1,release,R1,0\n");

    size_t count = 0;
    Event *events = CSVParser_parse("tmp_parse_ok.csv", &count);
    CHECK(count == 3);
    CHECK(events[0].time == 0);
    CHECK(events[1].time == 1);
    CHECK(events[2].time == 2);
    CHECK(strcmp(events[0].processId, "P1") == 0);
    CHECK(strcmp(events[0].action, "request") == 0);
    CHECK(events[0].duration == 3);
    free(events);
    remove("tmp_parse_ok.csv");

    /* Action khong hop le: CSVParser goi exit() -> khong the test trong process nay */
}

/* ------------------------------------------------------------------ */
/* testDetector                                                         */
/* ------------------------------------------------------------------ */
static void testDetector(void)
{
    printf("testDetector\n"); fflush(stdout);

    DeadlockDetector det;
    DeadlockDetector_init(&det);

    /* P1 -> P2 -> P3 -> P1: chu trinh */
    DeadlockDetector_addWaitRelation(&det, "P1", "P2");
    DeadlockDetector_addWaitRelation(&det, "P2", "P3");
    DeadlockDetector_addWaitRelation(&det, "P3", "P1");
    CHECK(DeadlockDetector_detectDeadlock(&det));
    CHECK(DeadlockDetector_isInDeadlock(&det, "P1"));
    CHECK(DeadlockDetector_isInDeadlock(&det, "P2"));
    CHECK(DeadlockDetector_isInDeadlock(&det, "P3"));

    /* P4 cho P1 nhung khong ai cho P4 -> P4 khong trong chu trinh */
    DeadlockDetector_addWaitRelation(&det, "P4", "P1");
    CHECK(DeadlockDetector_detectDeadlock(&det));
    CHECK(!DeadlockDetector_isInDeadlock(&det, "P4"));

    /* Pid chua biet */
    CHECK(!DeadlockDetector_isInDeadlock(&det, "P9"));

    /* Go P1 -> het chu trinh */
    DeadlockDetector_removeProcess(&det, "P1");
    CHECK(!DeadlockDetector_detectDeadlock(&det));

    DeadlockDetector_destroy(&det);

    /* Chain don thuan, khong deadlock */
    DeadlockDetector chain;
    DeadlockDetector_init(&chain);
    DeadlockDetector_addWaitRelation(&chain, "A", "B");
    DeadlockDetector_addWaitRelation(&chain, "B", "C");
    CHECK(!DeadlockDetector_detectDeadlock(&chain));
    CHECK(!DeadlockDetector_isInDeadlock(&chain, "A"));
    DeadlockDetector_destroy(&chain);
}

/* ------------------------------------------------------------------ */
/* Helper: tao ProcessEntry mang 1 phan tu                             */
/* ------------------------------------------------------------------ */
static ProcessEntry makeProcessEntry(const char *id, ProcessState state)
{
    ProcessEntry pe;
    pe.key = (char *)id;
    memset(&pe.value, 0, sizeof(Process));
    pe.value.id    = (char *)id;
    pe.value.state = state;
    return pe;
}

static ResourceEntry makeResourceEntry(const char *id, const char *owner)
{
    ResourceEntry re;
    re.key = (char *)id;
    memset(&re.value, 0, sizeof(Resource));
    re.value.id    = (char *)id;
    re.value.owner = owner ? strdup(owner) : NULL;
    return re;
}

/* ------------------------------------------------------------------ */
/* testTimeoutKill                                                      */
/* ------------------------------------------------------------------ */
static void testTimeoutKill(void)
{
    printf("testTimeoutKill\n"); fflush(stdout);

    TimeoutConfig cfg;
    cfg.timeout      = 3;
    cfg.strategy     = TIMEOUT_STRATEGY_KILL;
    cfg.retryDelay   = 1;
    cfg.maxRetries   = 3;
    cfg.maxRollbacks = 3;

    TimeoutManager mgr;
    TimeoutManager_init(&mgr, cfg);

    /* Process P1 bi block, giu R1, dang cho R2 */
    ProcessEntry pe = makeProcessEntry("P1", PROCESS_STATE_BLOCKED);
    pe.value.has_requestTime = true;
    pe.value.requestTime = 0;
    pe.value.waitingFor  = strdup("R2");
    /* Them R1 vao heldResources */
    pe.value.heldResources = malloc(sizeof(char *));
    pe.value.heldResources[0] = strdup("R1");
    pe.value.heldResourcesCount    = 1;
    pe.value.heldResourcesCapacity = 1;

    ResourceEntry re = makeResourceEntry("R1", "P1");

    PendingRequest pending[2];
    pending[0].processId  = strdup("P1");
    pending[0].resourceId = strdup("R2");
    pending[0].requestTime = 0;
    pending[0].duration    = 5;
    pending[0].retryCount  = 0;
    size_t pendingCount = 1;

    DeadlockDetector det;
    DeadlockDetector_init(&det);
    /* P1 khong trong chu trinh -> false positive */

    size_t recCount = 0;
    TimeoutRecord *records = TimeoutManager_checkTimeouts(
        &mgr, 5,
        &pe, 1,
        &re, 1,
        pending, &pendingCount,
        &det, &recCount);

    CHECK(recCount == 1);
    CHECK(records[0].waitingTime == 5);
    CHECK(records[0].killed);
    CHECK(records[0].falsePositive);
    CHECK(pe.value.state == PROCESS_STATE_TERMINATED);
    CHECK(Resource_isFree(&re.value));

    free(records);
    DeadlockDetector_destroy(&det);
    free(pe.value.heldResources);   /* da duoc giai phong trong kill, nhung mang con do */
}

/* ------------------------------------------------------------------ */
/* testTimeoutRetryEscalation                                           */
/* ------------------------------------------------------------------ */
static void testTimeoutRetryEscalation(void)
{
    printf("testTimeoutRetryEscalation\n"); fflush(stdout);

    TimeoutConfig cfg;
    cfg.timeout      = 2;
    cfg.strategy     = TIMEOUT_STRATEGY_RETRY;
    cfg.retryDelay   = 1;
    cfg.maxRetries   = 2;
    cfg.maxRollbacks = 3;

    TimeoutManager mgr;
    TimeoutManager_init(&mgr, cfg);

    ProcessEntry pe = makeProcessEntry("P1", PROCESS_STATE_BLOCKED);
    pe.value.has_requestTime = true;
    pe.value.requestTime = 0;
    pe.value.waitingFor  = strdup("R1");

    ResourceEntry re = makeResourceEntry("R1", "P2");

    /* Mang pending du lon cho retry push_back */
    PendingRequest pending[16];
    pending[0].processId  = strdup("P1");
    pending[0].resourceId = strdup("R1");
    pending[0].requestTime = 0;
    pending[0].duration    = 5;
    pending[0].retryCount  = 0;
    size_t pendingCount = 1;

    DeadlockDetector det;
    DeadlockDetector_init(&det);

    /* Retry 1 */
    size_t rc = 0;
    TimeoutRecord *recs = TimeoutManager_checkTimeouts(
        &mgr, 3, &pe, 1, &re, 1, pending, &pendingCount, &det, &rc);
    pendingCount = 1; /* retry them lai 1 pending */
    CHECK(rc == 1);
    CHECK(recs[0].retried);
    CHECK(!recs[0].killed);
    CHECK(pe.value.state == PROCESS_STATE_RUNNING);
    free(recs);

    /* Simulate block lai, retry 2 */
    pe.value.state = PROCESS_STATE_BLOCKED;
    rc = 0;
    recs = TimeoutManager_checkTimeouts(
        &mgr, 10, &pe, 1, &re, 1, pending, &pendingCount, &det, &rc);
    pendingCount = 1;
    CHECK(rc == 1);
    CHECK(recs[0].retried);
    free(recs);

    /* Retry 3 -> vuot maxRetries -> kill */
    pe.value.state = PROCESS_STATE_BLOCKED;
    pe.value.requestTime = pending[0].requestTime;
    rc = 0;
    recs = TimeoutManager_checkTimeouts(
        &mgr, 40, &pe, 1, &re, 1, pending, &pendingCount, &det, &rc);
    CHECK(rc == 1);
    CHECK(recs[0].killed);
    CHECK(pe.value.state == PROCESS_STATE_TERMINATED);
    free(recs);

    DeadlockDetector_destroy(&det);
}

/* ------------------------------------------------------------------ */
/* testTimeoutRollback                                                  */
/* ------------------------------------------------------------------ */
static void testTimeoutRollback(void)
{
    printf("testTimeoutRollback\n"); fflush(stdout);

    TimeoutConfig cfg;
    cfg.timeout      = 3;
    cfg.strategy     = TIMEOUT_STRATEGY_ROLLBACK;
    cfg.retryDelay   = 1;
    cfg.maxRetries   = 3;
    cfg.maxRollbacks = 2;

    TimeoutManager mgr;
    TimeoutManager_init(&mgr, cfg);

    ProcessEntry pe = makeProcessEntry("P1", PROCESS_STATE_BLOCKED);
    pe.value.has_requestTime = true;
    pe.value.requestTime = 0;
    pe.value.waitingFor  = strdup("R2");
    pe.value.heldResources = malloc(sizeof(char *));
    pe.value.heldResources[0] = strdup("R1");
    pe.value.heldResourcesCount    = 1;
    pe.value.heldResourcesCapacity = 1;

    ResourceEntry re = makeResourceEntry("R1", "P1");

    PendingRequest pending[4];
    pending[0].processId  = strdup("P1");
    pending[0].resourceId = strdup("R2");
    pending[0].requestTime = 0;
    pending[0].duration    = 5;
    pending[0].retryCount  = 0;
    size_t pendingCount = 1;

    DeadlockDetector det;
    DeadlockDetector_init(&det);

    /* Rollback 1 */
    size_t rc = 0;
    TimeoutRecord *recs = TimeoutManager_checkTimeouts(
        &mgr, 5, &pe, 1, &re, 1, pending, &pendingCount, &det, &rc);
    pendingCount = 0;
    CHECK(rc == 1);
    CHECK(recs[0].rolledBack);
    CHECK(!recs[0].killed);
    CHECK(pe.value.state == PROCESS_STATE_NEW);
    CHECK(pe.value.rollbackCount == 1);
    CHECK(Resource_isFree(&re.value));
    free(recs);
    free(pe.value.heldResources);

    /* Rollback 2 */
    pe.value.state = PROCESS_STATE_BLOCKED;
    pe.value.requestTime = 10;
    pe.value.heldResources = malloc(sizeof(char *));
    pe.value.heldResources[0] = strdup("R1");
    pe.value.heldResourcesCount    = 1;
    pe.value.heldResourcesCapacity = 1;
    re.value.owner = strdup("P1");

    pending[0].processId  = strdup("P1");
    pending[0].resourceId = strdup("R2");
    pending[0].requestTime = 10;
    pending[0].duration    = 5;
    pending[0].retryCount  = 0;
    pendingCount = 1;

    rc = 0;
    recs = TimeoutManager_checkTimeouts(
        &mgr, 20, &pe, 1, &re, 1, pending, &pendingCount, &det, &rc);
    pendingCount = 0;
    CHECK(rc == 1);
    CHECK(recs[0].rolledBack);
    CHECK(pe.value.rollbackCount == 2);
    free(recs);
    free(pe.value.heldResources);

    /* Rollback 3 -> vuot maxRollbacks -> kill */
    pe.value.state = PROCESS_STATE_BLOCKED;
    pe.value.requestTime = 30;
    pe.value.heldResources = malloc(sizeof(char *) * 1);
    pe.value.heldResourcesCount    = 0;
    pe.value.heldResourcesCapacity = 1;

    pending[0].processId  = strdup("P1");
    pending[0].resourceId = strdup("R2");
    pending[0].requestTime = 30;
    pending[0].duration    = 5;
    pending[0].retryCount  = 0;
    pendingCount = 1;

    rc = 0;
    recs = TimeoutManager_checkTimeouts(
        &mgr, 40, &pe, 1, &re, 1, pending, &pendingCount, &det, &rc);
    CHECK(rc == 1);
    CHECK(recs[0].killed);
    CHECK(!recs[0].rolledBack);
    CHECK(pe.value.state == PROCESS_STATE_TERMINATED);
    free(recs);
    free(pe.value.heldResources);

    DeadlockDetector_destroy(&det);
}

/* ------------------------------------------------------------------ */
/* testEngineThroughputBounded                                          */
/* ------------------------------------------------------------------ */
static void testEngineThroughputBounded(void)
{
    printf("testEngineThroughputBounded\n"); fflush(stdout);

    writeTemp("tmp_engine.csv",
        "time,process_id,action,resource_id,duration\n"
        "0,P1,request,R1,10\n"
        "0,P2,request,R2,10\n"
        "0,P3,request,R3,10\n"
        "1,P1,request,R2,0\n"
        "1,P2,request,R3,0\n"
        "1,P3,request,R1,0\n");

    size_t eventsCount = 0;
    Event *events = CSVParser_parse("tmp_engine.csv", &eventsCount);

    TimeoutConfig cfg;
    cfg.timeout      = 3;
    cfg.strategy     = TIMEOUT_STRATEGY_RETRY;
    cfg.retryDelay   = 1;
    cfg.maxRetries   = 3;
    cfg.maxRollbacks = 3;

    SimulationEngine *engine = SimulationEngine_create(&cfg, false);
    SimulationMetrics m = SimulationEngine_run(engine, events, eventsCount);
    SimulationEngine_destroy(engine);

    CHECK(m.totalProcesses == 3);
    CHECK(SimulationMetrics_throughput(&m) <= 1.0);
    CHECK(m.completedProcesses <= m.totalProcesses);
    CHECK(m.falsePositives <= m.timeoutEvents);

    /* Kill tren cung kich ban deadlock */
    TimeoutConfig cfgKill;
    cfgKill.timeout      = 3;
    cfgKill.strategy     = TIMEOUT_STRATEGY_KILL;
    cfgKill.retryDelay   = 1;
    cfgKill.maxRetries   = 3;
    cfgKill.maxRollbacks = 3;

    SimulationEngine *engineKill = SimulationEngine_create(&cfgKill, false);
    SimulationMetrics mk = SimulationEngine_run(engineKill, events, eventsCount);
    SimulationEngine_destroy(engineKill);

    CHECK(SimulationMetrics_throughput(&mk) <= 1.0);
    CHECK(mk.killedProcesses >= 1);

    free(events);
    remove("tmp_engine.csv");
}

/* ------------------------------------------------------------------ */
/* testEngineRollback                                                   */
/* ------------------------------------------------------------------ */
static void testEngineRollback(void)
{
    printf("testEngineRollback\n"); fflush(stdout);

    writeTemp("tmp_rollback.csv",
        "time,process_id,action,resource_id,duration\n"
        "0,P1,request,R1,10\n"
        "0,P2,request,R2,10\n"
        "0,P3,request,R3,10\n"
        "1,P1,request,R2,0\n"
        "1,P2,request,R3,0\n"
        "1,P3,request,R1,0\n");

    size_t eventsCount = 0;
    Event *events = CSVParser_parse("tmp_rollback.csv", &eventsCount);

    TimeoutConfig cfg;
    cfg.timeout      = 3;
    cfg.strategy     = TIMEOUT_STRATEGY_ROLLBACK;
    cfg.retryDelay   = 1;
    cfg.maxRetries   = 3;
    cfg.maxRollbacks = 3;

    SimulationEngine *engine = SimulationEngine_create(&cfg, false);
    SimulationMetrics m = SimulationEngine_run(engine, events, eventsCount);
    SimulationEngine_destroy(engine);

    CHECK(m.totalProcesses == 3);
    CHECK(SimulationMetrics_throughput(&m) <= 1.0);
    CHECK(m.completedProcesses <= m.totalProcesses);
    CHECK(m.rollbackEvents >= 1);

    free(events);
    remove("tmp_rollback.csv");
}

/* ------------------------------------------------------------------ */
/* testTc2RollbackThroughputUnique                                      */
/* ------------------------------------------------------------------ */
static void testTc2RollbackThroughputUnique(void)
{
    printf("testTc2RollbackThroughputUnique\n"); fflush(stdout);

    size_t eventsCount = 0;
    Event *events = CSVParser_parse("data/tc_2.csv", &eventsCount);
    if (!events) { printf("  SKIP: data/tc_2.csv not found\n"); return; }

    TimeoutConfig cfg;
    cfg.timeout      = 1;
    cfg.strategy     = TIMEOUT_STRATEGY_ROLLBACK;
    cfg.retryDelay   = 1;
    cfg.maxRetries   = 3;
    cfg.maxRollbacks = 3;

    SimulationEngine *engine = SimulationEngine_create(&cfg, false);
    SimulationMetrics m = SimulationEngine_run(engine, events, eventsCount);
    SimulationEngine_destroy(engine);

    CHECK(m.totalProcesses == 8);
    CHECK(m.completedProcesses == m.totalProcesses);
    CHECK(SimulationMetrics_throughput(&m) == 1.0);

    free(events);
}

/* ------------------------------------------------------------------ */
/* testTc2KillNoRevive                                                  */
/* ------------------------------------------------------------------ */
static void testTc2KillNoRevive(void)
{
    printf("testTc2KillNoRevive\n"); fflush(stdout);

    size_t eventsCount = 0;
    Event *events = CSVParser_parse("data/tc_2.csv", &eventsCount);
    if (!events) { printf("  SKIP: data/tc_2.csv not found\n"); return; }

    TimeoutConfig cfg;
    cfg.timeout      = 4;
    cfg.strategy     = TIMEOUT_STRATEGY_KILL;
    cfg.retryDelay   = 1;
    cfg.maxRetries   = 3;
    cfg.maxRollbacks = 3;

    SimulationEngine *engine = SimulationEngine_create(&cfg, false);
    SimulationMetrics m = SimulationEngine_run(engine, events, eventsCount);
    SimulationEngine_destroy(engine);

    CHECK(m.totalProcesses == 8);
    CHECK(m.completedProcesses == 7);
    CHECK(SimulationMetrics_throughput(&m) == 0.875);

    free(events);
}

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */
int main(void)
{
    testParser();
    testDetector();
    testTimeoutKill();
    testTimeoutRetryEscalation();
    testTimeoutRollback();
    testEngineThroughputBounded();
    testEngineRollback();
    testTc2RollbackThroughputUnique();
    testTc2KillNoRevive();

    printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}







