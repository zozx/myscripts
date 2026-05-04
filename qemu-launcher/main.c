#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <limits.h>
#include <dirent.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(int argc, char *argv[]) {
	if (argc != 1) {
		fprintf(stderr, "This script does not accept argument.\n");
		return EXIT_FAILURE;
	}

	char *target_dir;
	char exe_path[PATH_MAX];
	ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
	if (len != -1) {
		exe_path[len] = '\0';
		char *tmp_path = strdup(exe_path);
		target_dir = strdup(dirname(tmp_path));
		free(tmp_path);
	} else {
		target_dir = strdup(".");
	}

	struct dirent *de;
	DIR *dp = opendir(target_dir);
	if (dp == NULL) {
		fprintf(stderr, "Could not open current directory");
		free(target_dir);
		return EXIT_FAILURE;
	}

	int n = 0, cap = 16;
	char **dir = calloc(cap, sizeof(char*));
	if (!dir) {
		fprintf(stderr, "Unable to allocate memory for directory list\n");
		for (int i = 0; i < n; i++) free(dir[i]);
		free(target_dir);
		closedir(dp);
		return EXIT_FAILURE;
	}
	
	char *tmp_path = strdup(exe_path);
	char *exe_name = strdup(basename(tmp_path));
	free(tmp_path);
	while ((de = readdir(dp)) != NULL) {
		if (de -> d_name[0] == '.' || strcmp(de -> d_name, exe_name) == 0) continue;
		if (n >= cap) {
			cap *= 2;
			char **new_dir = realloc(dir, cap * sizeof(char*));
			if (!new_dir) {
				fprintf(stderr, "Unable to reallocate memory for directory list\n");
				for (int i = 0; i < n; i++) free(dir[i]);
				free(dir);
				free(target_dir);
				free(exe_name);
				closedir(dp);
				return EXIT_FAILURE;
			}
			dir = new_dir;
		}
		dir[n++] = strdup(de -> d_name);
	}
	free(exe_name);
	closedir(dp);

	printf("The following %s:", n == 1 ? "directory exists" : "directories exist");

	for (int i = 1; i <= n; i++) {
		printf("\n\t%d. %s", i, dir[i - 1]);
	}

	printf("\nChoose: ");
	int choose;
	if (scanf("%d", &choose) != 1) {
		fprintf(stderr, "Error: Invalid input (not a number).\n");
		for (int i = 0; i < n; i++) free(dir[i]);
		free(dir);
		free(target_dir);
		return EXIT_FAILURE;
	}

	if (choose > 0 && choose <= n) {
		pid_t pid = fork();
		if (pid < 0) {
			perror("fork failed");
			for (int i = 0; i < n; i++) free(dir[i]);
			free(dir);
			free(target_dir);
			return EXIT_FAILURE;
		} else if (pid == 0) {
			char img_drive[PATH_MAX + 16];
			snprintf(img_drive, sizeof(img_drive), "file=%s/%s/%s.img,format=raw", target_dir, dir[choose - 1], dir[choose - 1]);
			free(target_dir);
			char *qemu_args[] = {
				"qemu-system-x86_64",
				"-name", dir[choose - 1],
				"-bios", "/usr/share/edk2/x64/OVMF.4m.fd",
				"-boot", "d",
				"-drive", img_drive,
				"-enable-kvm",
				"-smp", "4",
				"-m", "8G",
				"-net", "user,hostfwd=tcp::2222-:22",
				"-net", "nic",
				NULL
			};
			printf("Starting QEMU: %s\n", dir[choose - 1]);
			execvp("qemu-system-x86_64", qemu_args);
			perror("execvp failed");
			for (int i = 0; i < n; i++) free(dir[i]);
			free(dir);
			exit(EXIT_FAILURE);
		} else {
			free(target_dir);
			printf("Waiting for VM (PID: %d) to finish...\n", pid);
			int status;
			waitpid(pid, &status, 0);
			if (WIFEXITED(status)) {
				printf("VM exited with status %d\n", WEXITSTATUS(status));
			}
		}
	} else {
		fprintf(stderr, "num %d does not exist\n", choose);
		for (int i = 0; i < n; i++) free(dir[i]);
		free(dir);
		free(target_dir);
		return EXIT_FAILURE;
	}

	for (int i = 0; i < n; i++) free(dir[i]);
	free(dir);

	return EXIT_SUCCESS;
}
