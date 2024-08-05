#ifndef SHMEM_UTILS_H
#define SHMEM_UTILS_H

#include <string>
#include <semaphore.h>
#include <sstream>
#include <iomanip>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdexcept>
#include <cstring>
#include <iostream>
#include <chrono>
#include <thread>
#include <memory>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

typedef unsigned char Byte;
typedef int FileDescriptor;
typedef std::chrono::milliseconds milliseconds;

namespace ShmemUtils
{
    // Sublogger specific to ShmemUtils
    // Function to get or create the logger instance
    std::shared_ptr<spdlog::logger> getLogger();
    
    /**
     * @brief Checks if a semaphore with the given name exists.
     *
     * @param semName The name of the semaphore.
     * @return true if the semaphore exists, false otherwise.
     */
    bool semExists(const std::string &semName);

    /**
     * @brief Connects to an existing semaphore or waits for it to be created.
     *
     * @param semName The name of the semaphore.
     * @param waitTime Time to wait in seconds before checking again.
     * @return A pointer to the connected semaphore.
     */
    sem_t *connectSem(const std::string &semName, const milliseconds &waitTime = milliseconds(10), const milliseconds &timeout = milliseconds(100 * 1000));

    /**
     * @brief Creates a new semaphore with the given name.
     *
     * @param semName The name of the semaphore.
     * @return A pointer to the created semaphore.
     */
    sem_t *createSem(const std::string &semName);

    /**
     * @brief Checks if shared memory with the given name exists.
     *
     * @param shmName The name of the shared memory.
     * @return true if the shared memory exists, false otherwise.
     */
    bool shmExists(const std::string &shmName);

    /**
     * @brief Connects to existing shared memory or waits for it to be created.
     *
     * @param shmPtr Reference to a pointer to the shared memory.
     * @param shmName The name of the shared memory.
     * @param waitTime Time to wait in seconds before checking again.
     * @param timeout Timeout in milliseconds.
     * @return The file descriptor of the connected shared memory.
     */
    FileDescriptor connectShm(Byte *&shmPtr, const std::string &shmName, const milliseconds &waitTime = milliseconds(10), const milliseconds &timeout = milliseconds(100 * 1000));

    /**
     * @brief Creates new shared memory with the given name and size.
     *
     * @param shmPtr Reference to a pointer to the shared memory.
     * @param shmName The name of the shared memory.
     * @param size The size of the shared memory in bytes.
     * @return The file descriptor of the created shared memory.
     */
    FileDescriptor createShm(Byte *&shmPtr, const std::string &shmName, const size_t &size);

    /**
     * @brief Clears the semaphore by waiting until its value reaches zero.
     *
     * @param sem A pointer to the semaphore to clear.
     */
    void clearSem(sem_t *sem);

    /**
     * @brief Clears the shared memory by setting all bytes to zero.
     *
     * @param shmPtr A pointer to the shared memory.
     * @param size The size of the shared memory in bytes.
     */
    void clearShm(Byte *shmPtr, size_t size);

    /**
     * @brief Closes the shared memory.
     *
     * @param shmFd The file descriptor of the shared memory.
     * @param shmPtr A pointer to the shared memory.
     * @param size The size of the shared memory in bytes.
     */
    void closeShm(FileDescriptor shmFd, Byte *shmPtr, const size_t &size);

    /**
     * @brief Closes the semaphore.
     *
     * @param sem A pointer to the semaphore.
     */
    void closeSem(sem_t *sem);

    /**
     * @brief Unlinks (removes) the shared memory with the given name.
     *
     * @param shmName The name of the shared memory.
     */
    void unlinkShm(const std::string &shmName);

    /**
     * @brief Unlinks (removes) the semaphore with the given name.
     *
     * @param semName The name of the semaphore.
     */
    void unlinkSem(const std::string &semName);

    /**
     * @brief Gets the size of the shared memory.
     *
     * @param shmFd The file descriptor of the shared memory.
     * @return The size of the shared memory in bytes.
     */
    size_t getShmSize(const FileDescriptor &shmFd);

    /**
     * @brief Gets the current value of the semaphore.
     *
     * @param sem A pointer to the semaphore.
     * @return The value of the semaphore.
     */
    int getSemValue(sem_t *sem);

    /**
     * @brief Sets the value of the semaphore.
     *
     * @param sem A pointer to the semaphore.
     * @param val The value to set.
     */
    void setSemValue(sem_t *sem, const size_t &val);

    /**
     * @brief Waits for the semaphore to become > 0.
     *
     * @param sem A pointer to the semaphore.
     * @param waitTime Milliseconds to wait before checking again. Default to 10 seconds.
     * @param timeout Timeout in milliseconds.
     * @param callback A function to be called when the semaphore becomes > 0.
     * @return 0 on success, error code on failure.
     */
    int waitSem(sem_t *sem, const milliseconds &waitTime = milliseconds(10), const milliseconds &timeout = milliseconds(100 * 1000), std::function<bool()> callback = nullptr);

    /**
     * @brief Posts on the semaphore.
     *
     * @param sem A pointer to the semaphore.
     * @return 0 on success, error code on failure.
     */
    int postSem(sem_t *sem);

} // namespace ShmemUtils

#endif // SHMEM_UTILS_H
