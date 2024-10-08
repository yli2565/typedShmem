#include "ShmemBase.h"
#include "ShmemUtils.h"

std::string sliceToHexString(const unsigned char *array, size_t start, size_t length)
{
    std::ostringstream hexStream;
    hexStream << std::hex << std::uppercase; // Set hex output to uppercase

    for (size_t i = start; i < start + length; ++i)
    {
        hexStream << "\\" << std::setw(2) << std::setfill('0') << static_cast<int>(array[i]);
    }

    return "0x" + hexStream.str();
}

// Constructor and Destructor
ShmemBase::ShmemBase(const std::string &name, size_t capacity)
{
    // init metrics
    this->name = name;
    this->capacity = capacity;
    this->connected = false;
    this->ownShm = false;
    this->usedSize = 0;
    this->version = -1;

    // init shared memory
    this->shmFd = -1;
    this->shmPtr = nullptr;

    // init semaphores
    this->counterSem = nullptr;
    this->versionSem = nullptr;
    this->writeLock = nullptr;

    // init logger
    this->logger = spdlog::default_logger()->clone("ShmBase:" + name);
    std::string pattern = "[" + std::to_string(getpid()) + "] [%n] %v";
    this->logger->set_pattern(pattern);

    // this->logger->info("Initialized shared memory base object {}", this->name);

    // DEBUG
    this->writeRecord = {};
}

ShmemBase::ShmemBase(size_t capacity) : ShmemBase("", capacity) {}

ShmemBase::~ShmemBase()
{
    try
    {
        this->close();
        this->unlink();
        this->logger->info("Destructed shared memory object {}", this->name);
    }
    catch (const std::exception &e)
    {
        this->logger->error("Exception during destructor: {}", e.what());
    }
}

// Core Operations
void ShmemBase::create()
{
    if (this->connected)
    {
        this->logger->error("Attempt to create an already connected shared memory. please close the previous connection first or use resize()");
        throw std::runtime_error("ShmemBase already connected, please close the previous connection first or use resize()");
    }

    // create shared memory
    this->shmFd = ShmemUtils::createShm(this->shmPtr, this->name, this->capacity);
    this->ownShm = true;
    this->connected = true;

    this->usedSize = 0;

    // create related semaphore
    this->counterSem = this->createCounterSem();

    // create version semaphore
    this->versionSem = this->createVersionSem();
    this->version = 0;

    // create write lock
    this->writeLock = this->createWriteLock();

    this->logger->info("Created shared memory {}", this->name);
}

void ShmemBase::resize(size_t newCapacity, bool keepContent)
{
    if (!this->connected)
    {
        this->logger->error("Try to resize an unconnected shared memory. please connect or create the ShmemBase first");
        throw std::runtime_error("ShmemBase not connected, please connect or create the ShmemBase first");
    }

    if (newCapacity < this->usedSize)
    {
        this->logger->error("New capacity {} is less than current used size {}", newCapacity, this->usedSize);
        throw std::invalid_argument("New capacity must be greater than current used size");
    }
    // Acquire write lock to prevent concurrent writing/resizing
    assert(ShmemUtils::waitSem(this->writeLock) == 0);

    size_t oldSize = 0;
    Byte *tempShm = nullptr;
    size_t oldCapacity = this->capacity;

    // Record old data
    if (keepContent)
    {
        oldSize = this->usedSize;
        tempShm = new Byte[oldCapacity];
        std::memcpy(tempShm, this->shmPtr, oldCapacity);
    }

    // Increase the version Semaphore to indicate this shm is resized
    ShmemUtils::postSem(this->versionSem);

    // Close and recreate shared memory
    ShmemUtils::closeShm(this->shmFd, this->shmPtr, this->capacity);
    this->shmFd = ShmemUtils::createShm(this->shmPtr, this->name, newCapacity); // This will automatically unlink the old shm

    // Restore old data
    if (keepContent)
    {
        std::memcpy(this->shmPtr, tempShm, oldCapacity);
        delete[] tempShm;
    }

    // Release write lock
    ShmemUtils::postSem(this->writeLock);

    if (keepContent)
    {
        this->usedSize = oldSize;
    }
    this->capacity = newCapacity;

    this->logger->info("Resized shared memory object {} from {} to {}", this->name, oldCapacity, newCapacity);
}

void ShmemBase::connect()
{
    if (this->connected)
    {
        this->logger->error("Attempt to connect an already connected shared memory. please close the previous connection first or use reconnect()");
        throw std::runtime_error("ShmemBase already connected, please close the previous connection first or use reconnect()");
    }

    this->shmFd = ShmemUtils::connectShm(this->shmPtr, this->name, this->waitTime);
    this->ownShm = false;
    this->connected = true;

    this->capacity = ShmemUtils::getShmSize(this->shmFd);
    this->usedSize = 0; // used size only record the furthest index reached by this process

    // connect related semaphore
    this->counterSem = this->connectCounterSem();

    this->versionSem = this->connectVersionSem();
    this->version = ShmemUtils::getSemValue(this->versionSem);

    this->writeLock = this->connectWriteLock();

    this->logger->info("Connected to shared memory object {}", this->name);
}

void ShmemBase::reconnect()
{
    if (!this->connected)
    {
        this->logger->error("Try to reconnect an unconnected shared memory. please connect or create the ShmemBase first", this->name);
        throw std::runtime_error("ShmemBase not connected, please connect or create the ShmemBase first");
    }

    this->shmFd = ShmemUtils::connectShm(this->shmPtr, this->name, this->waitTime);
    // Reconnect won't change the ownership of the shm
    // As ownership is just the responsibility to clean up the shm
    this->connected = true;

    this->capacity = ShmemUtils::getShmSize(this->shmFd);
    // Reconnect won't change the usedSize either

    // The version sem should be already connected
    this->version = ShmemUtils::getSemValue(this->versionSem);

    this->logger->info("Reconnected to shared memory object {}", this->name);
}

void ShmemBase::clear()
{
    this->checkConnection();

    assert(ShmemUtils::waitSem(this->writeLock) == 0);
    ShmemUtils::clearShm(this->shmPtr, this->usedSize);
    ShmemUtils::postSem(this->writeLock);
    this->usedSize = 0;

    ShmemUtils::clearSem(this->counterSem);

    this->logger->info("Cleared shared memory object {}", this->name);
}

void ShmemBase::close()
{
    ShmemUtils::closeShm(this->shmFd, this->shmPtr, this->usedSize);

    // close related sem
    ShmemUtils::closeSem(this->counterSem);
    ShmemUtils::closeSem(this->versionSem);
    ShmemUtils::closeSem(this->writeLock);

    this->connected = false;
    this->version = -1;

    this->usedSize = 0;

    this->logger->info("Closed shared memory object {}", this->name);
}

void ShmemBase::unlink()
{
    if (this->ownShm)
    {
        ShmemUtils::unlinkShm(this->name);

        // Unlink related semaphore
        ShmemUtils::unlinkSem(this->counterSemName());
        ShmemUtils::unlinkSem(this->versionSemName());
        ShmemUtils::unlinkSem(this->writeLockName());
        this->logger->info("Unlinked shared memory object {}", this->name);
    }
    else
    {
        this->logger->info("This process does not own {} shared memory object", this->name);
    }
}

// Access and Modification
Byte &ShmemBase::operator[](size_t i)
{
    checkConnection();
    if (i >= this->usedSize)
        throw std::out_of_range("Index out of range");

    // User can use this byte reference to modify the shared memory, it's unsafe, just for debug purpose
    this->logger->warn("Provide a unsafe byte reference at [{}]", i);
    return *(shmPtr + i);
}

Byte &ShmemBase::operator[](int i_)
{
    checkConnection();
    int i = i_;
    if (i < 0)
        return this->operator[](static_cast<size_t>(usedSize + i)); // Convert negative index to positive
    else
        return this->operator[](static_cast<size_t>(i));
}

void ShmemBase::setBytes(size_t index, const Byte *data, size_t len)
{
    checkConnection();

    // Acquire write lock
    assert(ShmemUtils::waitSem(this->writeLock) == 0);

    if (index + len > this->capacity)
    {
        this->logger->error("Index {} out of range [{}:{}]", index, 0, this->capacity);
        throw std::out_of_range("Index out of range");
    }
    std::memcpy(shmPtr + index, data, len);

    // Release write lock
    ShmemUtils::postSem(this->writeLock);

    // Increase the counter semaphore
    ShmemUtils::postSem(this->counterSem);

    this->usedSize = std::max(this->usedSize, index + len);
    this->logger->debug("Set bytes [{}:{}]", index, index + len);

    this->writeRecord.push_back({index, index + len});
}
void ShmemBase::getBytes(size_t index, Byte *data, size_t len)
{
    checkConnection();

    if (index + len > this->capacity)
    {
        this->logger->error("Index {} out of range [{}:{}]", index, 0, this->capacity);
        throw std::out_of_range("Index out of range");
    }
    std::memcpy(data, shmPtr + index, len);

    this->usedSize = std::max(this->usedSize, index + len);
    this->logger->debug("Get bytes [{}:{}]", index, index + len);
}
// Setters
void ShmemBase::setCapacity(size_t capacity)
{
    this->capacity = capacity;
    this->logger->info("Set capacity to {}", capacity);
}

// Accessors
const std::string &ShmemBase::getName() const
{
    return this->name;
}
size_t ShmemBase::getCapacity() const
{
    return this->capacity;
}
size_t ShmemBase::getUsedSize() const
{
    return this->usedSize;
}
int ShmemBase::getVersion() const
{
    return this->version;
}
bool ShmemBase::isConnected() const
{
    return this->connected;
}
bool ShmemBase::ownsSharedMemory() const
{
    return this->ownShm;
}

// Utility Functions
std::shared_ptr<spdlog::logger> &ShmemBase::getLogger()
{
    return this->logger;
}

// Temporary Util functions
int ShmemBase::getCounterSemValue() const
{
    return ShmemUtils::getSemValue(this->counterSem);
}

void ShmemBase::postCounterSem()
{
    ShmemUtils::postSem(this->counterSem);
}

void ShmemBase::waitCounterSem()
{
    ShmemUtils::waitSem(this->counterSem);
}

size_t ShmemBase::pad(size_t size, size_t align)
{
    return (size + align - 1) & ~(align - 1);
}

void ShmemBase::checkConnection()
{
    if (!this->connected)
    {
        this->logger->error("ShmemBase {} is accessed before getting connected", this->name);
        throw std::runtime_error("ShmemBase not connected, please connect or create the ShmemBase first");
    }
    else
    {
        if (this->version != ShmemUtils::getSemValue(this->versionSem))
        {
            // Version mismatch, it means some other process might have resize the shared memory
            this->reconnect();
        }
    }
}

// Naming Helpers
std::string ShmemBase::counterSemName() const
{
    return name + "_counter_sem";
}

std::string ShmemBase::versionSemName() const
{
    return this->name + "_version_sem";
}

std::string ShmemBase::writeLockName() const
{
    return this->name + "_write_sem";
}

// Semaphore Management
sem_t *ShmemBase::createCounterSem()
{
    this->counterSem = ShmemUtils::createSem(counterSemName().c_str());
    ShmemUtils::setSemValue(this->counterSem, 0);
    this->logger->info("Created Counter Semaphore {}: {}", counterSemName(), static_cast<void *>(this->counterSem));
    return this->counterSem;
}
sem_t *ShmemBase::connectCounterSem()
{
    this->counterSem = ShmemUtils::connectSem(counterSemName().c_str(), this->waitTime);
    this->logger->info("Connected to Counter Semaphore {}: {}", counterSemName(), static_cast<void *>(this->counterSem));
    return this->counterSem;
}
sem_t *ShmemBase::createVersionSem()
{
    this->versionSem = ShmemUtils::createSem(versionSemName().c_str());

    this->logger->info("Created Version Semaphore {}: {}", versionSemName(), static_cast<void *>(this->versionSem));
    return this->versionSem;
}
sem_t *ShmemBase::connectVersionSem()
{
    this->versionSem = ShmemUtils::connectSem(versionSemName().c_str(), this->waitTime);

    this->logger->info("Connected to Version Semaphore {}: {}", versionSemName(), static_cast<void *>(this->versionSem));
    return this->versionSem;
}

sem_t *ShmemBase::createWriteLock()
{
    this->writeLock = ShmemUtils::createSem(writeLockName().c_str());
    ShmemUtils::setSemValue(this->writeLock, 1); // Initialize write lock to 1 (allow writing)

    this->logger->info("Created write lock semaphore {}: {}", writeLockName(), static_cast<void *>(this->writeLock));
    return this->writeLock;
}
sem_t *ShmemBase::connectWriteLock()
{
    this->writeLock = ShmemUtils::connectSem(writeLockName().c_str(), this->waitTime);

    this->logger->info("Connected to write lock semaphore {}: {}", writeLockName(), static_cast<void *>(this->writeLock));
    return this->writeLock;
}

void ShmemBase::borrow(const ShmemBase &other)
{
    if (this != &other)
    {
        try
        {
            this->close();
            this->unlink();
        }
        catch (const std::exception &e)
        {
        }

        this->name = other.name;
        this->capacity = other.capacity;
        this->connected = false;
        this->ownShm = false;
        this->usedSize = 0;
        this->version = -1;

        // init shared memory
        this->shmFd = -1;
        this->shmPtr = nullptr;

        // init semaphores
        this->counterSem = nullptr;
        this->versionSem = nullptr;
        this->writeLock = nullptr;

        // switch logger
        this->logger = spdlog::default_logger()->clone("ShmBase:" + this->name);
        std::string pattern = "[" + std::to_string(getpid()) + "] [%n] %v";
        this->logger->set_pattern(pattern);

        // Ownership should be transferred
        if (other.isConnected())
        {
            this->connect();
        }
    }
}
void ShmemBase::steal(ShmemBase &&other)
{
    if (this != &other)
    {
        borrow(other);
        this->ownShm = true;
        other.ownShm = false;
    }
}

void ShmemBase::printMemoryView(size_t maxBytesToDisplay)
{
    checkConnection(); // Ensure the shared memory is connected

    // Print critical information
    this->logger->info("========================================================");
    this->logger->info("Shared Memory Object: {}", this->name);
    this->logger->info("Connected: {}", this->connected ? "Yes" : "No");
    this->logger->info("Owns Shared Memory: {}", this->ownShm ? "Yes" : "No");
    this->logger->info("Capacity: {} bytes", this->capacity);
    this->logger->info("Used Size: {} bytes", this->usedSize);
    this->logger->info("Version: {}", this->version);
    this->logger->info("Counter Sem: {}", ShmemUtils::getSemValue(this->counterSem));
    this->logger->info("Version Sem: {}", ShmemUtils::getSemValue(this->versionSem));
    this->logger->info("Write Lock: {}", ShmemUtils::getSemValue(this->writeLock));

    // Determine how many bytes to display
    this->logger->info("Displaying valid memory: {} access", this->writeRecord.size());

    // Print the memory content in a human-readable format
    for (size_t i = 0; i < this->writeRecord.size(); ++i)
    {
        size_t start = this->writeRecord[i][0];
        size_t end = this->writeRecord[i][1];
        this->logger->info("[{:08}:{:08}]\t{}", start, end, sliceToHexString(this->shmPtr, start, (end - start) < maxBytesToDisplay ? (end - start) : maxBytesToDisplay));
    }
    this->logger->info("========================================================");
}
