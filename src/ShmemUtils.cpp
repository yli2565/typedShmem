#include "ShmemUtils.h"

std::shared_ptr<spdlog::logger> ShmemUtils::getLogger()
{
    static std::shared_ptr<spdlog::logger> sublogger = []()
    {
        auto logger = spdlog::get("ShmUtils");
        if (!logger)
        {
            logger = spdlog::default_logger()->clone("ShmUtils");
            spdlog::register_logger(logger);
        }
        return logger;
    }();
    return sublogger;
}

bool ShmemUtils::semExists(const std::string &semName)
{
    sem_t *sem_tmp = sem_open(semName.c_str(), O_CREAT | O_EXCL, 0666, 0);
    if (sem_tmp == SEM_FAILED)
    {
        return true;
    }
    else
    {
        sem_close(sem_tmp);
        sem_unlink(semName.c_str());
        return false;
    }
}
sem_t *ShmemUtils::connectSem(const std::string &semName, const milliseconds &waitTime, const milliseconds &timeout)
{
    auto startTime = std::chrono::steady_clock::now();
    float timeCnt = 0.f;
    sem_t *sem = nullptr;

    while (true)
    {
        if (semExists(semName))
        {
            sem = sem_open(semName.c_str(), O_RDWR, 0666);
            if (sem == SEM_FAILED)
            {
                getLogger()->error("Failed to open semaphore {}: {}", semName, strerror(errno));
                throw std::runtime_error("Failed to open semaphore even when it should exist");
            }
            getLogger()->info("Connected to semaphore {} in {:.2f} seconds", semName, timeCnt);
            break;
        }
        else
        {
            auto currentTime = std::chrono::steady_clock::now();
            timeCnt = std::chrono::duration<float>(currentTime - startTime).count();

            if (timeCnt >= std::chrono::duration<float>(timeout).count())
            {
                getLogger()->error("Timeout reached. Failed to connect to semaphore {}", semName);
                break;
            }

            getLogger()->debug("Waiting for semaphore {} ... [ {:.1f} seconds ]", semName, timeCnt);
            std::this_thread::sleep_for(waitTime);
        }
    }

    return sem;
}

sem_t *ShmemUtils::createSem(const std::string &semName)
{
    if (semExists(semName))
    {
        sem_unlink(semName.c_str());
    }
    sem_t *semTmp = sem_open(semName.c_str(), O_CREAT | O_EXCL, 0666, 0);

    if (semTmp == SEM_FAILED)
    {
        getLogger()->error("Failed to create semaphore");
        throw std::runtime_error("Failed to create semaphore");
    }
    getLogger()->info("Created Semaphore {}", semName);
    return semTmp;
}

bool ShmemUtils::shmExists(const std::string &shmName)
{
    int shmFdTmp = shm_open(shmName.c_str(), O_CREAT | O_EXCL | O_RDWR, 0666);
    if (shmFdTmp == -1)
        return true;
    else
    {
        shm_unlink(shmName.c_str());
        return false;
    }
}

FileDescriptor ShmemUtils::connectShm(Byte *&shmPtr, const std::string &shmName, const milliseconds &waitTime, const milliseconds &timeout)
{
    auto startTime = std::chrono::steady_clock::now();
    float timeCnt = 0.f;
    FileDescriptor shmFd = -1;

    while (true)
    {
        shmFd = shm_open(shmName.c_str(), O_RDWR, 0666);
        if (shmFd != -1)
        {
            size_t size = getShmSize(shmFd);
            shmPtr = static_cast<Byte *>(mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, 0));
            getLogger()->info("Connected to shared memory {} in {} seconds", shmName, timeCnt);
            break;
        }
        else
        {
            if (errno == ENOENT)
            {
                auto currentTime = std::chrono::steady_clock::now();
                timeCnt = std::chrono::duration<float>(currentTime - startTime).count();

                if (timeCnt >= std::chrono::duration<float>(timeout).count())
                {
                    getLogger()->error("Timeout reached. Failed to connect to shared memory {}", shmName);
                    break;
                }

                getLogger()->debug("Waiting for shared memory {} ... [ {:.1f} seconds ]", shmName, timeCnt);
                std::this_thread::sleep_for(waitTime);
            }
            else
            {
                getLogger()->error("Failed to open shared memory with error: {}", strerror(errno));
                break;
            }
        }
    }

    return shmFd;
}

FileDescriptor ShmemUtils::createShm(Byte *&shmPtr, const std::string &shmName, const size_t &size)
{
    if (shmExists(shmName))
        shm_unlink(shmName.c_str());
    FileDescriptor shmFd = shm_open(shmName.c_str(), O_CREAT | O_EXCL | O_RDWR, 0666);
    if (shmFd == -1)
    {
        getLogger()->error("Failed to create shared memory");
        throw std::runtime_error("Failed to create shared memory");
    }
    if (ftruncate(shmFd, size) == -1)
    {
        getLogger()->error("Failed to set size of shared memory");
        throw std::runtime_error("Failed to set size of shared memory");
    }
    shmPtr = static_cast<Byte *>(mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, 0));
    if (shmPtr == MAP_FAILED)
    {
        getLogger()->error("Failed to map shared memory");
        throw std::runtime_error("Failed to map shared memory");
    }
    // init with zeros
    clearShm(shmPtr, size);

    getLogger()->info("Create shared memory. shmName: {}, shmPtr: {}, shmFd: {}, size: {}", shmName, static_cast<const void *>(shmPtr), shmFd, size);
    return shmFd;
}

void ShmemUtils::clearSem(sem_t *sem)
{
    while (getSemValue(sem) > 0)
    {
        sem_wait(sem);
    }
}
void ShmemUtils::clearShm(Byte *shmPtr, size_t size)
{
    std::memset(shmPtr, 0, size);
    getLogger()->debug("Cleared shared memory. shmPtr: {}, size: {}", static_cast<const void *>(shmPtr), size);
}

void ShmemUtils::closeShm(FileDescriptor shmFd, Byte *shmPtr, const size_t &size)
{
    getLogger()->debug("Closed shared memory. shmPtr: {}, shmFd: {}, size: {}", static_cast<const void *>(shmPtr), shmFd, size);

    if (shmPtr != nullptr)
    {
        munmap(shmPtr, size);
        shmPtr = nullptr;
    }
    if (shmFd != -1)
    {
        ::close(shmFd);
        shmFd = -1;
    }
}

void ShmemUtils::closeSem(sem_t *sem)
{
    getLogger()->debug("Closed semaphore {}", static_cast<const void *>(sem));

    if (sem == nullptr)
        return;
    sem_close(sem);
    sem = nullptr;
}

void ShmemUtils::unlinkShm(const std::string &shmName)
{
    shm_unlink(shmName.c_str());
    getLogger()->info("Unlinked shared memory {}", shmName);
}

void ShmemUtils::unlinkSem(const std::string &semName)
{
    sem_unlink(semName.c_str());
    getLogger()->info("Unlinked semaphore {}", semName);
}

size_t ShmemUtils::getShmSize(const FileDescriptor &shmFd)
{
    if (shmFd == -1)
        throw std::runtime_error("Shared Memory not established properly");
    struct stat shm_stat;
    if (fstat(shmFd, &shm_stat) == -1)
        throw std::runtime_error("Failed to get shared memory size");

    return shm_stat.st_size;
}

int ShmemUtils::getSemValue(sem_t *sem)
{
    int val = 0;
    sem_getvalue(sem, &val);
    getLogger()->debug("Get {} semaphore value: {}", static_cast<const void *>(sem), val);
    return val;
}
void ShmemUtils::setSemValue(sem_t *sem, const size_t &val)
{
    int curVal = 0;
    sem_getvalue(sem, &curVal);
    int oldVal = curVal;
    while (static_cast<size_t>(curVal) != val)
    {
        if (static_cast<size_t>(curVal) > val)
        {
            sem_wait(sem);
            curVal -= 1;
        }
        else
        {
            sem_post(sem);
            curVal += 1;
        }
    }
    sem_getvalue(sem, &curVal);
    if (static_cast<size_t>(curVal) != val)
    {
        getLogger()->error("Failed to set {} semaphore value: {}->{}", static_cast<const void *>(sem), oldVal, val);
        throw std::runtime_error("Failed to set semaphore");
    }
    getLogger()->debug("Set {} semaphore value: {}->{}", static_cast<const void *>(sem), oldVal, curVal);
}
int ShmemUtils::waitSem(sem_t *sem, const milliseconds &waitTime, const milliseconds &timeout, std::function<bool()> callback)
{
    auto start = std::chrono::steady_clock::now();
    while (true)
    {
        // Try to wait on the semaphore without blocking
        if (sem_trywait(sem) == 0)
        {
            // Successfully acquired the semaphore
            auto elapsed = std::chrono::steady_clock::now() - start;
            float elapsedSeconds = std::chrono::duration_cast<std::chrono::duration<float>>(elapsed).count();
            int val = 0;
            sem_getvalue(sem, &val);
            getLogger()->debug("Waited on {} semaphore in {:.1f} seconds. sem: {}->{}", static_cast<const void *>(sem), elapsedSeconds, val + 1, val);
            return 0;
        }

        // Check if the callback function returns true
        if (callback != nullptr && callback())
        {
            // Break the loop if callback returns true
            return EINTR;
        }

        // Check if the timeout has been reached
        auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed >= timeout)
        {
            // Break the loop if timeout has been reached
            return ETIMEDOUT;
        }

        // Add a small sleep to avoid busy-waiting
        std::this_thread::sleep_for(waitTime);

        // Update time count for logging purposes
        auto elapsedMillis = std::chrono::duration_cast<milliseconds>(elapsed).count();
        getLogger()->debug("\rWaiting sem {} ... [ {:.1f} seconds ]", static_cast<const void *>(sem), static_cast<float>(elapsedMillis) / 1000.0f);
    }
}
int ShmemUtils::postSem(sem_t *sem)
{
    sem_post(sem);
    int val = 0;
    sem_getvalue(sem, &val);
    getLogger()->debug("Posted on {} semaphore. sem: {}->{}", static_cast<const void *>(sem), val - 1, val);
    return 0;
}