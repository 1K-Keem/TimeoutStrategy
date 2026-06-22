#include "../include/SimulationEngine.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Deep copy string
static char *my_strdup(const char *s)
{
  if (!s)
    return NULL;
  char *d = malloc(strlen(s) + 1);
  if (d)
    strcpy(d, s);
  return d;
}

// Các hàm getProcess, getRemainingCount, getEventVector, ensureRemainingCount, ensureEventVector, pushEvent, addHeldResource, removeHeldResource được định nghĩa ở đây để quản lý trạng thái của các process, resource, pending request và event vector trong SimulationEngine. Chúng giúp tìm kiếm, tạo mới và cập nhật thông tin liên quan đến các thực thể này một cách hiệu quả trong quá trình chạy mô phỏng.

// Tìm process theo ID, trả về NULL nếu không tìm thấy
static Process *getProcess(SimulationEngine *engine, const char *processId)
{
  size_t i;
  for (i = 0; i < engine->processesCount_; i++)
  {
    if (strcmp(engine->processes_[i].key, processId) == 0)
    {
      return &engine->processes_[i].value;
    }
  }
  return NULL;
}

// Tìm số lượng event còn lại của một process
static int *getRemainingCount(SimulationEngine *engine, const char *processId)
{
  size_t i;
  for (i = 0; i < engine->remainingEventCountCount_; i++)
  {
    if (strcmp(engine->remainingEventCount_[i].key, processId) == 0)
    {
      return &engine->remainingEventCount_[i].value;
    }
  }
  return NULL;
}

// Kiểm tra process đã tồn tại hay chưa, nếu chưa thì tạo mới với số lượng event còn lại là 0
static int *ensureRemainingCount(SimulationEngine *engine, const char *processId)
{
  int *ptr = getRemainingCount(engine, processId);
  if (ptr)
    return ptr;

  // Nếu chưa tồn tại, tạo mới entry với value mặc định là 0
  if (engine->remainingEventCountCount_ >= engine->remainingEventCountCapacity_)
  {
    engine->remainingEventCountCapacity_ = engine->remainingEventCountCapacity_ == 0 ? 10 : engine->remainingEventCountCapacity_ * 2;
    engine->remainingEventCount_ = realloc(engine->remainingEventCount_, engine->remainingEventCountCapacity_ * sizeof(IntEntry));
  }

  engine->remainingEventCount_[engine->remainingEventCountCount_].key = my_strdup(processId);
  engine->remainingEventCount_[engine->remainingEventCountCount_].value = 0;
  return &engine->remainingEventCount_[engine->remainingEventCountCount_++].value;
}

// Tìm vector event của một process, trả về NULL nếu không tìm thấy
static EventVector *getEventVector(SimulationEngine *engine, const char *processId)
{
  size_t i;
  for (i = 0; i < engine->processEventsCount_; i++)
  {
    if (strcmp(engine->processEvents_[i].key, processId) == 0)
    {
      return &engine->processEvents_[i].value;
    }
  }
  return NULL;
}

// Kiểm tra vector event của một process đã tồn tại hay chưa, nếu chưa thì tạo mới và trả về con trỏ đến nó
static EventVector *ensureEventVector(SimulationEngine *engine, const char *processId)
{
  EventVector *ptr = getEventVector(engine, processId);
  if (ptr)
    return ptr;

  if (engine->processEventsCount_ >= engine->processEventsCapacity_)
  {
    engine->processEventsCapacity_ = engine->processEventsCapacity_ == 0 ? 10 : engine->processEventsCapacity_ * 2;
    engine->processEvents_ = realloc(engine->processEvents_, engine->processEventsCapacity_ * sizeof(EventVectorEntry));
  }

  engine->processEvents_[engine->processEventsCount_].key = my_strdup(processId);
  engine->processEvents_[engine->processEventsCount_].value.events = NULL;
  engine->processEvents_[engine->processEventsCount_].value.count = 0;
  engine->processEvents_[engine->processEventsCount_].value.capacity = 0;
  return &engine->processEvents_[engine->processEventsCount_++].value;
}

// Thêm một event vào vector event của một process (push_back)
static void pushEvent(EventVector *vec, const Event *event)
{
  if (vec->count >= vec->capacity)
  {
    vec->capacity = vec->capacity == 0 ? 10 : vec->capacity * 2;
    vec->events = realloc(vec->events, vec->capacity * sizeof(Event));
  }

  vec->events[vec->count].time = event->time;
  vec->events[vec->count].processId = my_strdup(event->processId);
  vec->events[vec->count].action = my_strdup(event->action);
  vec->events[vec->count].resourceId = my_strdup(event->resourceId);
  vec->events[vec->count].duration = event->duration;
  vec->count++;
}

// 2 hàm addHeldResource và removeHeldResource mô phỏng std::set
// Quản lý danh sách tài nguyên mà một process đang giữ.

// Thêm một tài nguyên vào danh sách heldResources của process nếu nó chưa tồn tại
static void addHeldResource(Process *process, const char *resourceId)
{
  size_t i;
  for (i = 0; i < process->heldResourcesCount; i++)
  {
    if (strcmp(process->heldResources[i], resourceId) == 0)
      return;
  }

  if (process->heldResourcesCount >= process->heldResourcesCapacity)
  {
    process->heldResourcesCapacity = process->heldResourcesCapacity == 0 ? 10 : process->heldResourcesCapacity * 2;
    process->heldResources = realloc(process->heldResources, process->heldResourcesCapacity * sizeof(char *));
  }

  process->heldResources[process->heldResourcesCount++] = my_strdup(resourceId);
}

// loại bỏ một tài nguyên khỏi danh sách này khi process giải phóng nó
static void removeHeldResource(Process *process, const char *resourceId)
{
  size_t i, keep = 0;
  for (i = 0; i < process->heldResourcesCount; i++)
  {
    if (strcmp(process->heldResources[i], resourceId) == 0)
    {
      free(process->heldResources[i]);
    }
    else
    {
      // Dồn các phần tử phía sau về trước nếu có phần tử bị xóa
      if (keep != i)
      {
        process->heldResources[keep] = process->heldResources[i];
      }
      keep++;
    }
  }
  process->heldResourcesCount = keep;
}

// Khởi tạo SimulationEngine, cấp phát bộ nhớ và khởi tạo các trường dữ liệu = 0
SimulationEngine *SimulationEngine_create(const TimeoutConfig *config, bool verbose)
{
  SimulationEngine *engine = malloc(sizeof(SimulationEngine));
  memset(engine, 0, sizeof(SimulationEngine));
  engine->verbose_ = verbose;
  if (config) {
    TimeoutManager_init(&engine->timeoutManager_, *config);
  }
  DeadlockDetector_init(&engine->deadlockDetector_);
  return engine;
}

void SimulationEngine_log(const SimulationEngine *engine, int currentTime, const char *message)
{
  if (engine->verbose_)
  {
    printf("Time %d: %s\n", currentTime, message);
  }
}

// Giải phóng bộ nhớ của SimulationEngine và các trường dữ liệu liên quan
void SimulationEngine_resetState(SimulationEngine *engine)
{
  size_t i;
  for (i = 0; i < engine->processesCount_; i++)
  {
    free(engine->processes_[i].key);
    free(engine->processes_[i].value.id);
    size_t j;
    for (j = 0; j < engine->processes_[i].value.heldResourcesCount; j++)
    {
      free(engine->processes_[i].value.heldResources[j]);
    }
    free(engine->processes_[i].value.heldResources);
    free(engine->processes_[i].value.waitingFor);
  }
  engine->processesCount_ = 0;

  for (i = 0; i < engine->resourcesCount_; i++)
  {
    free(engine->resources_[i].key);
    free(engine->resources_[i].value.id);
    free(engine->resources_[i].value.owner);
  }
  engine->resourcesCount_ = 0;

  for (i = 0; i < engine->pendingRequestsCount_; i++)
  {
    free(engine->pendingRequests_[i].processId);
    free(engine->pendingRequests_[i].resourceId);
  }
  engine->pendingRequestsCount_ = 0;

  for (i = 0; i < engine->remainingEventCountCount_; i++)
  {
    free(engine->remainingEventCount_[i].key);
  }
  engine->remainingEventCountCount_ = 0;

  for (i = 0; i < engine->processEventsCount_; i++)
  {
    free(engine->processEvents_[i].key);
    size_t j;
    for (j = 0; j < engine->processEvents_[i].value.count; j++)
    {
      free(engine->processEvents_[i].value.events[j].processId);
      free(engine->processEvents_[i].value.events[j].action);
      free(engine->processEvents_[i].value.events[j].resourceId);
    }
    free(engine->processEvents_[i].value.events);
  }
  engine->processEventsCount_ = 0;

  memset(&engine->metrics_, 0, sizeof(SimulationMetrics));
  DeadlockDetector_clear(&engine->deadlockDetector_);
}

// Kiểm tra process đã tồn tại hay chưa, nếu chưa thì tạo mới và thêm vào mảng processes_
void SimulationEngine_ensureProcessExists(SimulationEngine *engine, const char *processId)
{
  if (getProcess(engine, processId) != NULL)
    return;

  if (engine->processesCount_ >= engine->processesCapacity_)
  {
    engine->processesCapacity_ = engine->processesCapacity_ == 0 ? 10 : engine->processesCapacity_ * 2;
    engine->processes_ = realloc(engine->processes_, engine->processesCapacity_ * sizeof(ProcessEntry));
  }

  ProcessEntry *entry = &engine->processes_[engine->processesCount_++];
  entry->key = my_strdup(processId);
  memset(&entry->value, 0, sizeof(Process));
  entry->value.id = my_strdup(processId);
  entry->value.state = PROCESS_STATE_NEW;

  engine->metrics_.totalProcesses++;
}

// Kiểm tra resource đã tồn tại hay chưa, nếu chưa thì tạo mới và thêm vào mảng resources_
Resource *SimulationEngine_ensureResourceExists(SimulationEngine *engine, const char *resourceId)
{
  size_t i;
  for (i = 0; i < engine->resourcesCount_; i++)
  {
    if (strcmp(engine->resources_[i].key, resourceId) == 0)
    {
      return &engine->resources_[i].value;
    }
  }

  if (engine->resourcesCount_ >= engine->resourcesCapacity_)
  {
    engine->resourcesCapacity_ = engine->resourcesCapacity_ == 0 ? 10 : engine->resourcesCapacity_ * 2;
    engine->resources_ = realloc(engine->resources_, engine->resourcesCapacity_ * sizeof(ResourceEntry));
  }

  ResourceEntry *entry = &engine->resources_[engine->resourcesCount_++];
  entry->key = my_strdup(resourceId);
  memset(&entry->value, 0, sizeof(Resource));
  entry->value.id = my_strdup(resourceId);

  return &entry->value;
}

// Hàm chạy khởi đầu, nạp event vào từng hàng đợi riêng của process
// Đồng thời, tăng biến remainingEventCount_ của process đó lên 1
void SimulationEngine_registerEventSources(SimulationEngine *engine, const Event *events, size_t eventsCount)
{
  size_t i;
  for (i = 0; i < eventsCount; i++)
  {
    const Event *event = &events[i];

    int *remaining = ensureRemainingCount(engine, event->processId);
    (*remaining)++;

    EventVector *vec = ensureEventVector(engine, event->processId);
    pushEvent(vec, event);

    SimulationEngine_ensureProcessExists(engine, event->processId);
  }
}

// Chạy luồng xử lí chính của engine (logic được mô tả trong report)
SimulationMetrics SimulationEngine_run(SimulationEngine *engine, const Event *sortedEvents, size_t eventsCount)
{
  SimulationEngine_resetState(engine);
  if (eventsCount == 0)
  {
    return engine->metrics_;
  }

  SimulationEngine_registerEventSources(engine, sortedEvents, eventsCount);

  int currentTime = sortedEvents[0].time;
  size_t nextEventIndex = 0;

  while (true)
  {
    SimulationEngine_releaseExpiredResources(engine, currentTime);
    SimulationEngine_grantPendingRequests(engine, currentTime);
    SimulationEngine_processEventsAt(engine, currentTime, sortedEvents, eventsCount, &nextEventIndex);
    SimulationEngine_applyTimeouts(engine, currentTime);
    SimulationEngine_grantPendingRequests(engine, currentTime);
    SimulationEngine_checkAndCompleteProcesses(engine, currentTime);

    if (nextEventIndex >= eventsCount && engine->pendingRequestsCount_ == 0 &&
        !SimulationEngine_hasFutureRelease(engine, currentTime))
    {
      break;
    }

    ++currentTime;
  }

  return engine->metrics_;
}

// Xử lí các event tại thời điểm currentTime
void SimulationEngine_processEventsAt(SimulationEngine *engine, int currentTime, const Event *events, size_t eventsCount, size_t *nextEventIndex)
{
  while (*nextEventIndex < eventsCount && events[*nextEventIndex].time == currentTime)
  {
    const Event *event = &events[*nextEventIndex];
    SimulationEngine_ensureProcessExists(engine, event->processId);
    Process *process = getProcess(engine, event->processId);

    if (!Process_isAlive(process))
    {
      int *remaining = getRemainingCount(engine, event->processId);
      if (remaining)
        (*remaining)--;
      (*nextEventIndex)++;
      continue;
    }

    Resource *resource = SimulationEngine_ensureResourceExists(engine, event->resourceId);

    if (strcmp(event->action, "request") == 0)
    {
      if (Resource_isFree(resource))
      {
        PendingRequest request;
        request.processId = my_strdup(event->processId);
        request.resourceId = my_strdup(event->resourceId);
        request.requestTime = currentTime;
        request.duration = event->duration;
        request.retryCount = 0;

        SimulationEngine_allocateResource(engine, process, &request, currentTime);

        char buf[512];
        snprintf(buf, sizeof(buf), "%s requests %s -> Granted", event->processId, event->resourceId);
        SimulationEngine_log(engine, currentTime, buf);

        if (event->duration == 0)
        {
          SimulationEngine_completeProcess(engine, process, currentTime);
        }

        free(request.processId);
        free(request.resourceId);
      }
      else
      {
        // Nếu resource đang bị giữ bởi một process khác, block process hiện tại và thêm vào pending request
        SimulationEngine_blockProcess(engine, process, event, currentTime);
        char buf[512];
        snprintf(buf, sizeof(buf), "%s requests %s -> Blocked (held by %s)", event->processId, event->resourceId, resource->owner);
        SimulationEngine_log(engine, currentTime, buf);
      }
    }

    int *remaining = getRemainingCount(engine, event->processId);
    if (remaining)
      (*remaining)--;

    (*nextEventIndex)++;
  }
}

// Giải phóng tài nguyên hết hạn tại thời điểm currentTime
void SimulationEngine_releaseExpiredResources(SimulationEngine *engine, int currentTime)
{
  size_t i;
  char **toRelease = NULL; // Mảng tạm để lưu danh sách resourceId cần giải phóng sau khi duyệt xong
  size_t count = 0;
  size_t capacity = 0;

  for (i = 0; i < engine->resourcesCount_; i++)
  {
    Resource *resource = &engine->resources_[i].value;
    if (resource->has_releaseTime && resource->releaseTime <= currentTime)
    {
      if (count >= capacity)
      {
        capacity = capacity == 0 ? 10 : capacity * 2;
        toRelease = realloc(toRelease, capacity * sizeof(char *));
      }
      toRelease[count++] = my_strdup(resource->id);
    }
  }

  // Release thật sự sau khi đã duyệt hết để tránh ảnh hưởng đến vòng lặp
  for (i = 0; i < count; i++)
  {
    SimulationEngine_releaseResource(engine, toRelease[i]);
    free(toRelease[i]);
  }
  free(toRelease);
}

// Cấp phát tài nguyên cho các blocked request tại thời điểm currentTime (nếu có thể)
void SimulationEngine_grantPendingRequests(SimulationEngine *engine, int currentTime)
{
  size_t keep = 0;
  size_t i;

  // Duyệt pendingRequests_
  for (i = 0; i < engine->pendingRequestsCount_; i++)
  {
    PendingRequest *request = &engine->pendingRequests_[i];
    Process *process = getProcess(engine, request->processId);
    bool remove = false;

    if (!Process_isAlive(process))
    {
      remove = true;
    }
    else if (request->requestTime > currentTime)
    {
      remove = false;
    }
    else
    {
      Resource *resource = SimulationEngine_ensureResourceExists(engine, request->resourceId);
      if (!Resource_isFree(resource))
      {
        remove = false;
      }
      else
      {
        SimulationEngine_allocateResource(engine, process, request, currentTime);
        char buf[512];
        snprintf(buf, sizeof(buf), "%s acquires %s -> Granted (was waiting)", request->processId, request->resourceId);
        SimulationEngine_log(engine, currentTime, buf);

        // Nếu request này có duration = 0, hoàn thành process ngay lập tức mà không cần phải chờ timeout
        if (request->duration == 0)
        {
          SimulationEngine_completeProcess(engine, process, currentTime);
        }
        remove = true;
      }
    }

    if (remove)
    {
      free(request->processId);
      free(request->resourceId);
    }
    else
    {
      // Dồn các phần tử phía sau về trước nếu có phần tử bị xóa
      if (keep != i)
      {
        engine->pendingRequests_[keep] = engine->pendingRequests_[i];
      }
      keep++;
    }
  }
  engine->pendingRequestsCount_ = keep;
}

void SimulationEngine_applyTimeouts(SimulationEngine *engine, int currentTime)
{
  size_t recordsCount = 0;
  TimeoutRecord *records = TimeoutManager_checkTimeouts(
      &engine->timeoutManager_, currentTime, engine->processes_, engine->processesCount_,
      engine->resources_, engine->resourcesCount_, engine->pendingRequests_, &engine->pendingRequestsCount_,
      &engine->deadlockDetector_, &recordsCount);

  size_t i;
  for (i = 0; i < recordsCount; i++)
  {
    TimeoutRecord *record = &records[i];
    engine->metrics_.timeoutEvents++;

    if (!record->deadlockedAtTimeout)
    {
      engine->metrics_.falsePositives++;
    }

    if (record->retried)
      engine->metrics_.retryEvents++;
    if (record->rolledBack)
      engine->metrics_.rollbackEvents++;

    const char *fp = record->deadlockedAtTimeout ? "deadlock" : "false positive";
    char base[256];
    snprintf(base, sizeof(base), "%s TIMEOUT (waited %d, %s)", record->processId, record->waitingTime, fp);

    bool cycleBefore = DeadlockDetector_detectDeadlock(&engine->deadlockDetector_);

    if (record->killed)
    {
      DeadlockDetector_removeProcess(&engine->deadlockDetector_, record->processId);
      engine->metrics_.killedProcesses++;
      char buf[512];
      snprintf(buf, sizeof(buf), "%s -> Killed", base);
      SimulationEngine_log(engine, currentTime, buf);
    }
    else if (record->rolledBack)
    {
      DeadlockDetector_removeProcess(&engine->deadlockDetector_, record->processId);
      SimulationEngine_replayProcess(engine, record->processId, currentTime);
      char buf[512];
      snprintf(buf, sizeof(buf), "%s -> Rolled back", base);
      SimulationEngine_log(engine, currentTime, buf);
    }
    else if (record->retried)
    {
      DeadlockDetector_removeWaitingProcess(&engine->deadlockDetector_, record->processId);
      char buf[512];
      snprintf(buf, sizeof(buf), "%s -> Retry", base);
      SimulationEngine_log(engine, currentTime, buf);
    }

    bool cycleAfter = DeadlockDetector_detectDeadlock(&engine->deadlockDetector_);

    if (record->deadlockedAtTimeout && cycleBefore && !cycleAfter)
    {
      engine->metrics_.deadlockResolved++;
      SimulationEngine_log(engine, currentTime, "Deadlock resolved (cycle broken)");
    }

    free(record->processId);
    free(record->resourceId);
  }
  free(records);
}

void SimulationEngine_replayProcess(SimulationEngine *engine, const char *processId, int currentTime)
{
  EventVector *it = getEventVector(engine, processId);
  if (!it)
    return;

  size_t keep = 0;
  size_t i;
  for (i = 0; i < engine->pendingRequestsCount_; i++)
  {
    if (strcmp(engine->pendingRequests_[i].processId, processId) == 0)
    {
      free(engine->pendingRequests_[i].processId);
      free(engine->pendingRequests_[i].resourceId);
    }
    else
    {
      if (keep != i)
        engine->pendingRequests_[keep] = engine->pendingRequests_[i];
      keep++;
    }
  }
  engine->pendingRequestsCount_ = keep;

  int *remaining = ensureRemainingCount(engine, processId);
  *remaining = 0;

  Process *process = getProcess(engine, processId);
  process->state = PROCESS_STATE_NEW;
  process->has_requestTime = false;
  if (process->waitingFor)
  {
    free(process->waitingFor);
    process->waitingFor = NULL;
  }

  for (i = 0; i < it->count; i++)
  {
    const Event *event = &it->events[i];
    if (strcmp(event->action, "request") == 0)
    {
      if (engine->pendingRequestsCount_ >= engine->pendingRequestsCapacity_)
      {
        engine->pendingRequestsCapacity_ = engine->pendingRequestsCapacity_ == 0 ? 10 : engine->pendingRequestsCapacity_ * 2;
        engine->pendingRequests_ = realloc(engine->pendingRequests_, engine->pendingRequestsCapacity_ * sizeof(PendingRequest));
      }
      PendingRequest *req = &engine->pendingRequests_[engine->pendingRequestsCount_++];
      req->processId = my_strdup(event->processId);
      req->resourceId = my_strdup(event->resourceId);
      req->requestTime = currentTime;
      req->duration = event->duration;
      req->retryCount = 0;
    }
  }
}

// Cấp phát tài nguyên cho các blocked request tại thời điểm currentTime
void SimulationEngine_allocateResource(SimulationEngine *engine, Process *process, PendingRequest *request, int currentTime)
{
  Resource *resource = SimulationEngine_ensureResourceExists(engine, request->resourceId);

  if (resource->owner)
    free(resource->owner);
  resource->owner = my_strdup(process->id);

  // Thêm tài nguyên vào danh sách heldResources của process
  addHeldResource(process, request->resourceId);
  process->state = PROCESS_STATE_RUNNING;
  process->has_requestTime = false;

  if (process->waitingFor)
  {
    free(process->waitingFor);
    process->waitingFor = NULL;
  }

  if (request->duration > 0)
  {
    resource->has_releaseTime = true;
    resource->releaseTime = currentTime + request->duration;
  }
  // Nếu duration = 0, ta không cần set has_releaseTime vì tài nguyên sẽ được giải phóng ngay
  // Ngoài ra, việc complete process sẽ do hàm SimulationEngine_completeProcess đảm nhiệm
  // (được gọi trong hàm SimulationEngine_grantPendingRequests, SimulationEngine_processEventsAt)
  else
  {
    resource->has_releaseTime = false;
  }

  // Xóa mối quan hệ chờ trong deadlock detector
  DeadlockDetector_removeWaitingProcess(&engine->deadlockDetector_, process->id);
}

// Hoàn thành process, giải phóng tất cả tài nguyên mà process đang giữ và cập nhật trạng thái của process
void SimulationEngine_completeProcess(SimulationEngine *engine, Process *process, int currentTime)
{
  if (process->state == PROCESS_STATE_COMPLETED || process->state == PROCESS_STATE_TERMINATED)
  {
    return;
  }

  // Xóa process khỏi deadlock detector
  DeadlockDetector_removeProcess(&engine->deadlockDetector_, process->id);

  process->state = PROCESS_STATE_COMPLETED;
  process->has_requestTime = false;

  // Giải phóng các tài nguyên liên quan
  if (process->waitingFor)
  {
    free(process->waitingFor);
    process->waitingFor = NULL;
  }

  size_t i;
  for (i = 0; i < process->heldResourcesCount; i++)
  {
    Resource *resource = SimulationEngine_ensureResourceExists(engine, process->heldResources[i]);
    if (resource->owner)
    {
      free(resource->owner);
      resource->owner = NULL;
    }
    resource->has_releaseTime = false;
  }

  for (i = 0; i < process->heldResourcesCount; i++)
  {
    free(process->heldResources[i]);
  }
  process->heldResourcesCount = 0;

  // Cập nhật metrics
  if (!process->completionCounted)
  {
    engine->metrics_.completedProcesses++;
    process->completionCounted = true;
  }

  char buf[256];
  snprintf(buf, sizeof(buf), "%s -> Completed", process->id);
  SimulationEngine_log(engine, currentTime, buf);
}

// Giải phóng một tài nguyên cụ thể, cập nhật trạng thái của process đang giữ tài nguyên đó
// Hàm này được gọi khi một tài nguyên hết hạn hoặc khi một process bị kill/rollback và cần giải phóng tất cả tài nguyên mà nó đang giữ
void SimulationEngine_releaseResource(SimulationEngine *engine, const char *resourceId)
{
  Resource *resource = SimulationEngine_ensureResourceExists(engine, resourceId);
  if (!resource->owner)
    return;

  char *ownerId = my_strdup(resource->owner);

  free(resource->owner);
  resource->owner = NULL;
  resource->has_releaseTime = false;

  // Loại bỏ tài nguyên khỏi danh sách heldResources của process đang giữ nó
  Process *ownerProcess = getProcess(engine, ownerId);
  if (ownerProcess)
  {
    removeHeldResource(ownerProcess, resourceId);
  }
  free(ownerId);
}

// Được gọi tại cuối mỗi vòng lặp, sau khi đã xử lí tất cả event và timeout tại thời điểm currentTime (trong hàm SimulationEngine_run)
// Kiểm tra xem có process nào đang chạy mà không giữ tài nguyên nào, không còn event nào để thực hiện và không đang chờ tài nguyên nào nữa hay không. Nếu có, hoàn thành process đó.
void SimulationEngine_checkAndCompleteProcesses(SimulationEngine *engine, int currentTime)
{
  size_t i;
  for (i = 0; i < engine->processesCount_; i++)
  {
    Process *process = &engine->processes_[i].value;
    int *remaining = getRemainingCount(engine, process->id);
    int remCount = remaining ? *remaining : 0;

    if (process->state == PROCESS_STATE_RUNNING &&
        process->heldResourcesCount == 0 &&
        remCount == 0 &&
        process->waitingFor == NULL)
    {
      SimulationEngine_completeProcess(engine, process, currentTime);
    }
  }
}

// Được gọi khi một process request một tài nguyên nhưng tài nguyên đó đang bị giữ bởi một process khác
void SimulationEngine_blockProcess(SimulationEngine *engine, Process *process, const Event *event, int currentTime)
{
  // Update trạng thái, ghi nhận thời điểm chờ
  process->state = PROCESS_STATE_BLOCKED;
  process->has_requestTime = true;
  process->requestTime = currentTime;

  if (process->waitingFor)
    free(process->waitingFor);
  process->waitingFor = my_strdup(event->resourceId);

  if (engine->pendingRequestsCount_ >= engine->pendingRequestsCapacity_)
  {
    engine->pendingRequestsCapacity_ = engine->pendingRequestsCapacity_ == 0 ? 10 : engine->pendingRequestsCapacity_ * 2;
    engine->pendingRequests_ = realloc(engine->pendingRequests_, engine->pendingRequestsCapacity_ * sizeof(PendingRequest));
  }

  // Thêm vào pendingRequests_ của toàn bộ engine
  PendingRequest *req = &engine->pendingRequests_[engine->pendingRequestsCount_++];
  req->processId = my_strdup(event->processId);
  req->resourceId = my_strdup(event->resourceId);
  req->requestTime = currentTime;
  req->duration = event->duration;
  req->retryCount = 0;

  Resource *resource = SimulationEngine_ensureResourceExists(engine, event->resourceId);
  // Thêm vào DeadlockDetector
  if (resource->owner)
  {
    DeadlockDetector_addWaitRelation(&engine->deadlockDetector_, process->id, resource->owner);
  }
}

// Kiểm tra xem trong tương lai còn tài nguyên nào có thể được release hay không
// Giúp xác định điều kiện dừng của hàm SimulationEngine_run()
bool SimulationEngine_hasFutureRelease(const SimulationEngine *engine, int currentTime)
{
  size_t i;
  for (i = 0; i < engine->resourcesCount_; i++)
  {
    const Resource *resource = &engine->resources_[i].value;
    if (resource->has_releaseTime && resource->releaseTime > currentTime)
    {
      return true;
    }
  }
  return false;
}

// Giải phóng bộ nhớ của SimulationEngine và các trường dữ liệu liên quan
void SimulationEngine_destroy(SimulationEngine *engine)
{
    if (!engine) return;

    SimulationEngine_resetState(engine);

    if (engine->processes_) free(engine->processes_);
    if (engine->resources_) free(engine->resources_);
    if (engine->pendingRequests_) free(engine->pendingRequests_);
    if (engine->remainingEventCount_) free(engine->remainingEventCount_);
    if (engine->processEvents_) free(engine->processEvents_);

    free(engine);
}

