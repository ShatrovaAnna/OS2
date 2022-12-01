#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define COPY_BUFFER_SIZE 4096
#define FD_LIMIT_TIMEOUT_SEC 1

pthread_attr_t threadAttr;

void ReleaseProcessResources(void) {
	pthread_attr_destroy(&threadAttr);
}

void err_exit(int err, const char* msg) {
	char buf[100];
	strerror_r(err, buf, 100);
	printf("err in %s: %s", msg, buf);
	exit(-1);
}

typedef struct {
	char* srcPath;
	char* destPath;
	mode_t mode;
} CopyInfo;

CopyInfo* CreateCopyInfo(char* srcPath, char* destPath, mode_t mode) {
	CopyInfo* info = (CopyInfo*)calloc(1, sizeof(*info));
	if (!info) {
		err_exit(errno, "calloc");
	}
	info->srcPath = srcPath;
	info->destPath = destPath;
	info->mode = mode;
	return info;
}

void DestroyCopyInfo(CopyInfo* info) {
	if (info) {
		free(info->srcPath);
		free(info->destPath);
		free(info);
	}
}

void CopyFile(CopyInfo* info);
void CopyDir(CopyInfo* info);

void* FileCopyThread(void* arg) {
	CopyInfo* copyInfo = (CopyInfo*)arg;
	//printf("Copying FILE %s  -->  %s", copyInfo->srcPath, copyInfo->destPath);
	CopyFile(copyInfo);
	//printf("Copied FILE %s  -->  %s", copyInfo->srcPath, copyInfo->destPath);
	DestroyCopyInfo(copyInfo);
	return NULL;
}

void* DirCopyThread(void* arg) {
	CopyInfo* copyInfo = (CopyInfo*)arg;
	//printf("Copying DIR %s  -->  %s", copyInfo->srcPath, copyInfo->destPath);
	CopyDir(copyInfo);
	//printf("Copied DIR %s  -->  %s", copyInfo->srcPath, copyInfo->destPath);
	DestroyCopyInfo(copyInfo);
	return NULL;
}

void StartDirCopyThread(CopyInfo* copyInfo) {
	pthread_t thread;
	bool fdLimitReached = false;

	while (true) {
		if (fdLimitReached) {
			sleep(FD_LIMIT_TIMEOUT_SEC);
		}
		int err = pthread_create(&thread, &threadAttr, &DirCopyThread, copyInfo);
		if (!err) {
			break;
		}
		if (err != EAGAIN) {
			err_exit(err, "pthread_create");
		}
		fdLimitReached = true;
	}
}

void StartFileCopyThread(CopyInfo* copyInfo) {
	pthread_t thread;
	bool fdLimitReached = false;

	while (true) {
		if (fdLimitReached) {
			sleep(FD_LIMIT_TIMEOUT_SEC);
		}
		int err = pthread_create(&thread, &threadAttr, &FileCopyThread, copyInfo);
		if (!err) {
			break;
		}
		if (err != EAGAIN) {
			err_exit(err, "pthread_create");
		}
		fdLimitReached = true;
	}
}

int OpenFile(const char* file, int flags) {
	bool fdLimitReached = false;
	while (true) {
		if (fdLimitReached) {
			sleep(FD_LIMIT_TIMEOUT_SEC);
		}
		int fd = open(file, O_RDONLY);
		if (fd >= 0) {
			return fd;
		}
		if (errno != EMFILE) {
			err_exit(errno, "open file");
		}
		fdLimitReached = true;
	}
}

int CreateFile(const char* file, mode_t mode) {
	bool fdLimitReached = false;
	while (true) {
		if (fdLimitReached) {
			sleep(FD_LIMIT_TIMEOUT_SEC);
		}
		int fd = creat(file, mode);
		if (fd >= 0) {
			return fd;
		}
		if (errno != EMFILE) {
			err_exit(errno, "create file");
		}
		fdLimitReached = true;
	}
}

int ReadDir(DIR* dir, struct dirent* prevEntry, struct dirent** entry) {
	bool fdLimitReached = false;
	while (true) {
		if (fdLimitReached) {
			sleep(FD_LIMIT_TIMEOUT_SEC);
		}
		int err = readdir_r(dir, prevEntry, entry);
		if (err != EMFILE) {
			return err;
		}
		fdLimitReached = true;
	}
}

DIR* OpenDir(const char* dirName) {
	bool fdLimitReached = false;
	while (true) {
		if (fdLimitReached) {
			sleep(FD_LIMIT_TIMEOUT_SEC);
		}
		DIR* dir = opendir(dirName);
		if (dir) {
			return dir;
		}
		if (!dir && errno != EMFILE) {
			return NULL;
		}
		fdLimitReached = true;
	}
}

void CopyFile(CopyInfo* info) {
	int srcFd = OpenFile(info->srcPath, O_RDONLY);
	int destFd = CreateFile(info->destPath, info->mode);

	void* buffer = calloc(1, COPY_BUFFER_SIZE);
	if (!buffer) {
		err_exit(errno, "calloc");
	}
	ssize_t readBytes;
	while ((readBytes = read(srcFd, buffer, COPY_BUFFER_SIZE))) {
		ssize_t writtenBytes = write(destFd, buffer, readBytes);
		if (writtenBytes < readBytes) {
			err_exit(errno, "write");
		}
	}
	if (readBytes < 0) {
		err_exit(errno, "read");
	}
	free(buffer);
	close(srcFd);
	close(destFd);
}

bool StringsEqual(const char* LHS, const char* RHS, size_t maxLength) {
	size_t len = strnlen(LHS, maxLength);
	return len == strnlen(RHS, maxLength) && 0 == strncmp(LHS, RHS, len);
}

char* AppendPath(const char* dir, const char* name, size_t maxLength) {
	char* path = (char*)calloc(maxLength, sizeof(*path));
	if (!path) {
		err_exit(errno, "calloc path");
	}
	path = strncpy(path, dir, maxLength);
	if (!path) {
		err_exit(errno, "strncopy path");
	}

	size_t pathLen = strnlen(path, maxLength);
	if (pathLen >= maxLength) {
		printf("file name length limit reached: %s", path);
		exit(-1);
	}
	strcat(path, "/");
	path = strncat(path, name, maxLength - pathLen - 1);
	if (!path) {
		err_exit(errno, "strncat path");
	}
	return path;
}

void CopyDir(CopyInfo* info) {
	size_t maxPathLength = (size_t)pathconf(info->srcPath, _PC_PATH_MAX);

	if (0 != mkdir(info->destPath, info->mode) && errno != EEXIST) {
		err_exit(errno, "mkdir");
	}
	DIR* srcDir = OpenDir(info->srcPath);
	if (!srcDir) {
		err_exit(errno, "err opening dir");
	}
	size_t entryLen = offsetof(struct dirent, d_name) + fpathconf(dirfd(srcDir), _PC_NAME_MAX) + 1;

	struct dirent* prevEntry = (struct dirent*)calloc(entryLen, 1);
	if (!prevEntry) {
		err_exit(errno, "calloc");
	}
	struct dirent* entry = NULL;
	struct stat entryStat;
	int err;
	while (0 == (err = ReadDir(srcDir, prevEntry, &entry))) {
		if (!entry) {
			break;
		}
		if (StringsEqual(entry->d_name, ".", maxPathLength) || StringsEqual(entry->d_name, "..", maxPathLength)) {
			continue;
		}

		char* srcPath = AppendPath(info->srcPath, entry->d_name, maxPathLength);
		char* destPath = AppendPath(info->destPath, entry->d_name, maxPathLength);

		if (stat(srcPath, &entryStat)) {
			err_exit(errno, "stat");
		}

		CopyInfo* copyInfo = CreateCopyInfo(srcPath, destPath, entryStat.st_mode);

		if (S_ISREG(entryStat.st_mode)) {
			StartFileCopyThread(copyInfo);
		}
		else if (S_ISDIR(entryStat.st_mode)) {
			StartDirCopyThread(copyInfo);
		}
		else {
			printf("Incorrect file type: %s\n", srcPath);
			exit(-1);
		}
	}
	if (err) {
		err_exit(err, "readdir");
	}
	if (0 != closedir(srcDir)) {
		err_exit(errno, "closedir");
	}
}

int main(int argc, char const* argv[]) {

	if (argc != 3) {
		printf("Usage: invalid input\n");
		exit(-1);
	}

	int err = pthread_attr_init(&threadAttr);
	if (err) {
		err_exit(err, "pthread_attr_init");
	}
	if (atexit(&ReleaseProcessResources)) {
		err_exit(errno, "atexit");
	}

	err = pthread_attr_setdetachstate(&threadAttr, PTHREAD_CREATE_JOINABLE);
	if (err) {
		err_exit(err, "pthread_attr_setdetachstate");
	}

	size_t srcLen = strlen(argv[1]);
	size_t destLen = strlen(argv[2]);

	char* srcDirPath = (char*)calloc(srcLen + 1, sizeof(*srcDirPath));
	if (!srcDirPath) {
		err_exit(errno, "calloc");
	}
	char* destDirPath = (char*)calloc(destLen + 1, sizeof(*destDirPath));
	if (!destDirPath) {
		err_exit(errno, "calloc");
	}
	strncpy(srcDirPath, argv[1], srcLen);
	strncpy(destDirPath, argv[2], destLen);
	struct stat srcDirStat;
	if (stat(srcDirPath, &srcDirStat)) {
		err_exit(errno, "stat");
	}

	CopyInfo* copyInfo = CreateCopyInfo(srcDirPath, destDirPath, srcDirStat.st_mode);
	StartDirCopyThread(copyInfo);

	pthread_exit(NULL);
	return 0;
}
