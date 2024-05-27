#include <cstdio>
#include <string>
#include <iostream>
#include <fstream>
#include <sys/ioctl.h>
#include "miosix/filesystem/ioctl.h"
#include "miosix.h"
#include "miosix/filesystem/file_access.h"
#include "miosix/filesystem/fat32/fat32.h"

using namespace std;
using namespace miosix;

bool deviceTest();
void mkfsTest();
void loopMountTest();

const char* filename = "/sd/test.txt";

int main(int argc, char* argv[])
{
    deviceTest();
    mkfsTest();
    // loopMountTest();
    return 1;
}

bool deviceTest(){
    
    FILE* deviceSd = fopen("/dev/sda", "r");

    if(deviceSd == NULL){
        printf("Device not found\n");
        return false;
    } else {
        printf("Device found\n");
    }

    int fileDescriptor = fileno(deviceSd);

    printf("File descriptor: %d\n", fileDescriptor);

    //Get all info
    unsigned int readSize, eraseSize, writeSize;
    unsigned long long deviceSize;
    ioctl(fileDescriptor, IOCTL_GET_READ_SIZE, &readSize);
    ioctl(fileDescriptor, IOCTL_GET_VOLUME_SIZE, &deviceSize);
    ioctl(fileDescriptor, IOCTL_GET_ERASE_SIZE, &eraseSize);
    ioctl(fileDescriptor, IOCTL_GET_WRITE_SIZE, &writeSize);

    printf("SDIODriver::ioctl() - Card size: %lluKB, %lluGB\n", deviceSize/1024, deviceSize/(1024*1024*1024));
    printf("SDIODriver::ioctl() - Erase size: %dB\n",eraseSize);
    printf("SDIODriver::ioctl() - Write count: %dB\n",writeSize);
    printf("SDIODriver::ioctl() - Read size: %dB\n",readSize);
    return true;       
}

void mkfsTest(){
    // Unmount fs
    if(FilesystemManager::instance().umount("/sd") != 0)
        printf("Failed to unmount fat32 fs\n");
    else printf("Old fat32 fs unmounted\n");

    // Format SD card
    FilesystemManager& fileSystemManager = FilesystemManager::instance();
    const char* deviceName = "/dev/sda";

    int result = fileSystemManager.mkfat32(deviceName);

    if(result != 0){
        printf("Error formatting SD card %d\n", result);
        return;
    }

    printf("SD card formatted\n");

    // Create /sd directory
    result = mkdir("/sd", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    if(result != 0){
        printf("Failed to create directory /sd, it may already exist, error %d\n", result);
    } else printf("Created /sd folder\n");

    // Mount fs
    intrusive_ref_ptr<FileBase> device;
    StringPart sp("sda");
    intrusive_ref_ptr<DevFs> devFs = fileSystemManager.getDevFs();
    result = devFs.get()->open(device, sp, O_RDWR, 0);
    if(result != 0){
        printf("Error opening devFs, error %d\n", result);
        return;
    }
    printf("DevFs opened\n");
    Fat32Fs* fat32 = new Fat32Fs(device);
    if(fat32->mountFailed()){
        printf("Error mounting SD card\n");
        return;
    }
    printf("SD card mounted\n");
    intrusive_ref_ptr<FilesystemBase> fsRef(fat32);
    int kmountResult = fileSystemManager.kmount("/sd", fsRef);
    if(kmountResult!=0)
    {
        printf("Failed to mount fat32 to /sd, error %d\n", kmountResult);
    }else printf("Fat32 mounted on /sd\n");

    //Create file
    FILE* file = fopen(filename, "w");
    if(file == NULL){
        printf("Error creating file\n");
        return;
    }else{
        printf("File created\n");
    }

    //Write to file
    char text[] = "This is a test";
    int written = fwrite(text, 1, sizeof(text), file);
    if(written == sizeof(text)){
        printf("Text written to file\n");
    }

    //Close file
    fclose(file);
}

void loopMountTest(){
    const char* filefspath = "/sd/filefs";
    FILE* file = fopen(filefspath, "w");
    if(file == NULL){
        printf("Error creating file\n");
        return;
    }else{
        printf("File created\n");
    }

    fclose(file);

    // Unexpected usage fault
    FilesystemManager& fileSystemManager = FilesystemManager::instance();
    int result = fileSystemManager.mkfat32(filefspath);

    if(result != 0){
        printf("Error formatting file as fs %d\n", result);
        return;
    }

    printf("File formatted as fs\n");
}