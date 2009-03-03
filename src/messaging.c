#include <cgreen/messaging.h>

#if defined WINCE || defined WIN32
#include <stdio.h>
#include <windows.h>
#else
#include <sys/msg.h>
#endif

#include <stdlib.h>

#define message_content_size(Type) (sizeof(Type) - sizeof(long))

typedef struct CgreenMessageQueue_ {
#if defined WINCE || defined WIN32
    FILE* pReadQueue;
    FILE* pWriteQueue;
    unsigned int owner;
#else
    int queue;
    pid_t owner;
#endif
    int tag;

} CgreenMessageQueue;

typedef struct CgreenMessage_ {
    long type;
    int result;
} CgreenMessage;

static CgreenMessageQueue *queues = NULL;
static int queue_count = 0;

static void clean_up_messaging(void);

int start_cgreen_messaging(int tag) {
#if defined WINCE
    MSGQUEUEOPTIONS msgOptions = {0};
    msgOptions.dwFlags       = MSGQUEUE_ALLOW_BROKEN;
    msgOptions.dwMaxMessages = 0;
    msgOptions.cbMaxMessage  = sizeof(CgreenMessage);
    msgOptions.dwSize        = (DWORD)sizeof(msgOptions);
    msgOptions.bReadAccess   = TRUE;

    queues = (CgreenMessageQueue *)realloc(queues, sizeof(CgreenMessageQueue) * ++queue_count);
    queues[queue_count - 1].pReadQueue = CreateMsgQueue(L"CG_CORE_TEST_IPC", &msgOptions);

    msgOptions.bReadAccess = FALSE;
    queues[queue_count - 1].pWriteQueue = CreateMsgQueue(L"CG_CORE_TEST_IPC", &msgOptions);
    queues[queue_count - 1].owner = 0;
    queues[queue_count - 1].tag = tag;
    return queue_count - 1;
#elif defined WIN32
    HANDLE hReadPipe = NULL;
    HANDLE hWritePipe = NULL;
    
    if(!CreatePipe(&hReadPipe, &hWritePipe, NULL, 100000))
       return -1;

    queues = (CgreenMessageQueue *)realloc(queues, sizeof(CgreenMessageQueue) * ++queue_count);
    queues[queue_count - 1].pReadQueue = hReadPipe; 
    queues[queue_count - 1].pWriteQueue = hWritePipe; 
    queues[queue_count - 1].owner = 0;
    queues[queue_count - 1].tag = tag;
    return queue_count - 1;
#else
    if (queue_count == 0) {
        atexit(&clean_up_messaging);
    }
    queues = (CgreenMessageQueue *)realloc(queues, sizeof(CgreenMessageQueue) * ++queue_count);
    queues[queue_count - 1].queue = msgget((long)getpid(), 0666 | IPC_CREAT);
    queues[queue_count - 1].owner = getpid();
    queues[queue_count - 1].tag = tag;
    return queue_count - 1;
#endif
}

void send_cgreen_message(int messaging, int result) {

#if defined WINCE
    DWORD dwBytesWritten = 0;
    DWORD dwFlags = 0;
#elif defined WIN32
    DWORD dwBytesWritten = 0;
#endif

    CgreenMessage *message = (CgreenMessage *)malloc(sizeof(CgreenMessage));
    message->type = queues[messaging].tag;
    message->result = result;

#if defined WINCE
    if(!WriteMsgQueue(queues[messaging].pWriteQueue, message, sizeof(CgreenMessage), INFINITE, dwFlags))
        dwBytesWritten = 0;
#elif defined WIN32
    if(!WriteFile(queues[messaging].pWriteQueue, message, sizeof(CgreenMessage), &dwBytesWritten, NULL))
        dwBytesWritten = 0;
#else
    msgsnd(queues[messaging].queue, message, sizeof(CgreenMessage), 0);
#endif
    free(message);
}

int receive_cgreen_message(int messaging) {
#if defined  WINCE
    DWORD dwBytesRead = 0;
    DWORD dwFlags = 0;
#elif defined WIN32
    DWORD dwBytesRead = 0;
    DWORD dwTotalBytesAvail = 0;
#endif

    int result = 0;
    CgreenMessage *message = (CgreenMessage *)malloc(sizeof(CgreenMessage));
    memset(message, 0, sizeof(CgreenMessage));

#if defined WINCE
    ReadMsgQueue(queues[messaging].pReadQueue, message,
                 sizeof(CgreenMessage), &dwBytesRead, 0, &dwFlags);
    result = (dwBytesRead > 0 ? message->result : 0);
#elif defined WIN32
    PeekNamedPipe(queues[messaging].pReadQueue, NULL, 0, NULL, &dwTotalBytesAvail, NULL);
    if((dwTotalBytesAvail) && (dwTotalBytesAvail % sizeof(CgreenMessage) == 0))
        if(!ReadFile(queues[messaging].pReadQueue, message, sizeof(CgreenMessage), &dwBytesRead, NULL))
            dwBytesRead = 0;
    result = (dwBytesRead > 0 ? message->result : 0);
#else
    ssize_t received = msgrcv(queues[messaging].queue,
                              message,
                              sizeof(CgreenMessage),
                              queues[messaging].tag,
                              IPC_NOWAIT);
    result = (received > 0 ? message->result : 0);
#endif


    free(message);
    return result;
}

static void clean_up_messaging(void) {
    int i;
    for (i = 0; i < queue_count; i++) 
    {
#if defined WINCE
        CloseMsgQueue(queues[i].pReadQueue);
        CloseMsgQueue(queues[i].pWriteQueue);
#elif defined WIN32
        DisconnectNamedPipe(queues[i].pReadQueue);
        DisconnectNamedPipe(queues[i].pWriteQueue);
#else
        if (queues[i].owner == getpid())
        {
            msgctl(queues[i].queue, IPC_RMID, NULL);
        }
#endif
    }
    free(queues);
    queues = NULL;
    queue_count = 0;
}
