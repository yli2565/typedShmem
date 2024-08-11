#ifndef SHMEM_BASE_H
#define SHMEM_BASE_H

#include <string>
#include <memory>
#include <spdlog/spdlog.h>
#include "ShmemUtils.h"

#define DCap 1024
class ShmemBase
{
public:
    // Constructors and Destructor
    ShmemBase(const std::string &name, size_t capacity = DCap);
    ShmemBase(size_t capacity = DCap);
    ~ShmemBase();

    // Core Operations
    void create();
    void resize(size_t newCapacity, bool keepContent = true);
    void connect();
    void reconnect();
    void clear();
    void close();
    void unlink();

    // Access and Modification
    Byte &operator[](size_t i);
    Byte &operator[](int i_);
    void setBytes(size_t index, const Byte *data, size_t len);
    void getBytes(size_t index, Byte *data, size_t len);

    template <typename T>
    void getArray(size_t index, T *data, size_t len, size_t offset = 0)
    {
        this->getBytes(offset + index * sizeof(T), reinterpret_cast<Byte *>(&data), sizeof(T) * len);
    }
    template <typename T>
    void setArray(size_t index, const T *data, size_t len, size_t offset = 0)
    {
        this->setBytes(offset + index * sizeof(T), reinterpret_cast<const Byte *>(&data), sizeof(T) * len);
    }

    template <typename T>
    T get(size_t index, size_t offset = 0)
    { // Access the shared memory as an array with some offset
        T result;
        this->getBytes(offset + index * sizeof(T), reinterpret_cast<Byte *>(&result), sizeof(T));
        return result;
    }
    template <typename T>
    void set(size_t index, const T &value, size_t offset = 0)
    {
        this->setBytes(offset + index * sizeof(T), reinterpret_cast<const Byte *>(&value), sizeof(T));
    }

    // Utility Functions
    std::shared_ptr<spdlog::logger> &getLogger();

    // Setters
    void setCapacity(size_t capacity);

    // Accessors
    const std::string &getName() const;
    size_t getCapacity() const;
    size_t getUsedSize() const;
    int getVersion() const;
    bool isConnected() const;
    bool ownsSharedMemory() const;

    // Debug
    void printMemoryView(size_t maxBytesToDisplay = 16);

protected:
    // Utility Functions
    void checkConnection();
    size_t pad(size_t size, size_t align);

    // Naming Helpers
    std::string counterSemName() const;
    std::string versionSemName() const;
    std::string writeLockName() const;

    // Semaphore Management
    sem_t *createCounterSem();
    sem_t *connectCounterSem();
    sem_t *createVersionSem();
    sem_t *connectVersionSem();
    sem_t *createWriteLock();
    sem_t *connectWriteLock();

    // shared memory
    int shmFd;
    Byte *shmPtr;
    // related semaphore
    sem_t *counterSem;
    sem_t *versionSem;
    sem_t *writeLock;

    // Constants
    const milliseconds waitTime = milliseconds(10);

private:
    // Member Variables
    std::string name;
    size_t capacity;
    bool connected;
    bool ownShm;
    size_t usedSize;
    int version;
    // logger
    std::shared_ptr<spdlog::logger> logger;
    // Debug
    std::vector<std::vector<size_t>> writeRecord;
};

#endif // SHMEM_BASE_H
