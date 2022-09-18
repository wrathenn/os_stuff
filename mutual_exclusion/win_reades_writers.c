#include <stdio.h>
#include <stdbool.h>
#include <windows.h>
 
#define W_SLEEP_MS 100
#define R_SLEEP_MS 150
 
#define W_CNT 4
#define R_CNT 5
 
#define ITERS 4
 
HANDLE mutex, canRead, canWrite;
 
HANDLE writersThreads[W_CNT];
HANDLE readersThreads[R_CNT];
 
bool workingWriter = false;
 
LONG waitingWriters = 0;
LONG waitingReaders = 0;
LONG activeReaders = 0;
 
int value = 0;
 
void startWrite()
{
    InterlockedIncrement(&waitingWriters);
 
    if (workingWriter || activeReaders > 0)
    // WaitForSingleObject ждет пока объект, дескриптор которого был передан, перейдет в сигнальное состояние. Можно применить к событиям, мьютексам, семафорам, процессам, потокам и т.п.
        WaitForSingleObject(canWrite, INFINITE);
 
    InterlockedDecrement(&waitingWriters);
    workingWriter = true;
}
 
void stopWrite()
{
    workingWriter = false;
 
    if (waitingReaders > 0)
        SetEvent(canRead);
    else
        SetEvent(canWrite);
}
 
DWORD WINAPI writer(LPVOID lpParams)
{
    for (int i = 0; i < ITERS; i++)
    {
        startWrite();
 
        value++;
        printf("<<Writer[ID = %d]: write value %d\n", lpParams, value);
 
        stopWrite();
        Sleep(W_SLEEP_MS);
    }
 
    return 0;
}
 
void startRead()
{
    WaitForSingleObject(mutex, INFINITE);
 
    InterlockedIncrement(&waitingReaders);
 
    if (workingWriter || waitingWriters > 0)
        WaitForSingleObject(canRead, INFINITE);
 
    InterlockedDecrement(&waitingReaders);
    InterlockedIncrement(&activeReaders);
    SetEvent(canRead);
 
    ReleaseMutex(mutex);
}
 
void stopRead()
{
    InterlockedDecrement(&activeReaders);
 
    if (activeReaders == 0) {
        SetEvent(canWrite);
        ResetEvent(canRead);
    }
}
 
DWORD WINAPI reader(LPVOID lpParams)
{
    while (value < W_CNT * ITERS)
    {
        startRead();
 
        printf(">>Reader[ID = %d]: read value %d\n", lpParams, value);
 
        stopRead();
        
        Sleep(R_SLEEP_MS);
    }
 
    return 0;
}
 
int main()
{
    // HANDLE - дескриптор мьютекса
    // 1 - LPSECURITY_ATTRIBUTES lpMutexAttributes - указатель на структуру SECURITY_ATTRIBUTES, установка атрибутов защиты. По умолчанию - NULL
    // 2 - BOOL bInitialOwner - если TRUE, то созданный мьютекс будет захвачен текущим потоком. FALSE - свободный мьютекс.
    // 3 - LPCWSTR - имя для создания именнованного мьютекса. NULL - неименованный мьютекс.
    if ((mutex = CreateMutex(NULL, FALSE, NULL)) == NULL)
    {
        perror("Mutex error");
        exit(-1);
    }
    // HANDLE - дескриптор ивента
    // 1 - LPSECURITY_ATTRIBUTES lpEventAttributes - указатель на структуру SECURITY_ATTRIBUTES, установка атрибутов защиты. По умолчанию - NULL
    // 2 - BOOL bManualReset - тип ивента, TRUE - переключаемый вручную (с ручным сбросом), FALSE - переключаемый автоматически (с автосбросом)
    // 3 - BOOL bInitialState - начальное состояние, если True, то изначально в сигнальном состоянии
    // 4 - PChar lpName - имя ивента (для разделения между потоками, при создании с тем же именем вернет дескриптор сущ.). Может быть NULL, если нужен неименованный поток.
    if ((canRead = CreateEvent(NULL, FALSE, TRUE, NULL)) == NULL)
    {
        perror("CanRead event error");
        exit(-1);
    }
 
    if ((canWrite = CreateEvent(NULL, TRUE, TRUE, NULL)) == NULL)
    {
        perror("CanWrite event error");
        exit(-1);
    }
 
    for (int i = 0; i < W_CNT; i++)
    {
        // HANDLE - дескриптор потока, хранит статистическую информацию о нем. Инициализируется код завершения потока - STILL ACTIVE, счетчику простоя потока присваивается 1.
        // Создается структура CONTEXT, формируется стек потока - в адр. пр-ве процесса резервируется регион размером в 2 стр. Значения lpStartAddr, lpvThreadParm помещаются в стек.
        // Иниц. регистры - указатель стека и счетчик команд в структуре CONTEXT потока.
        // CreateThread
        // 1 - LPSECURITY_ATTRIBUTES lpsa - указатель на структуру SECURITY_ATTRIBUTES, установка атрибутов защиты. По умолчанию - NULL
        // 2 - DWORD cbStack - какую часть адресного пространства поток может использовать под стек. По умолчанию - 0
        // 3 - LPTHREAD_START_ROUTINE - адрес функции потока, с которого начнется его выполнение.
        // 4 - LPVOID lpvThreadParam - указатель на переменную, которая передается в поток
        // 5 - DWORD fdwCreate - флаги, управляющие созданием потока. 0 - исполнение потока начнется немедленно, CREATE_SUSPENDED - поток находится в состоянии ожидания.
        // 6 - LPDWORD lpIDThread - адрес переменной типа DWORD, в которую функция возвращает идентификатор потока
        if ((writersThreads[i] = CreateThread(NULL, 0, writer, i, 0, NULL)) == NULL)
        {
            perror("Writer thread creation error");
            exit(-1);
        }
    }
 
    for (int i = 0; i < R_CNT; i++)
    {
        if ((readersThreads[i] = CreateThread(NULL, 0, reader, i, 0, NULL)) == NULL)
        {
            perror("Reader thread creation error");
            exit(-1);
        }
    }

    // Количество, массив *HANDLE, BOOL bWaitAll, DWORD wdMilliSeconds
    WaitForMultipleObjects(W_CNT, writersThreads, TRUE, INFINITE);
    WaitForMultipleObjects(R_CNT, readersThreads, TRUE, INFINITE);

    for (int i = 0; i < R_CNT; i++)
    {
        CloseHandle(readersThreads[i]);
    }

    for (int i = 0; i < W_CNT; i++)
    {
        CloseHandle(writersThreads[i]);
    }
 
    CloseHandle(mutex);
    CloseHandle(canRead);
    CloseHandle(canWrite);
 
    return 0;
}