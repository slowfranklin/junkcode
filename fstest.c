/* filesystem verification tool, designed to detect data corruption on a filesystem

   tridge@samba.org, March 2002
 */
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>

/* variables settable on the command line */
static int loop_count = 100;
static int num_files = 1;
static int file_size = 1024*1024;
static char *base_dir = ".";

typedef unsigned char uchar;

#define BUF_SIZE 1024

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

/* generate a buffer for a particular child, fnum etc. Just use a simple buffer
   to make debugging easy 
*/
static void gen_buffer(uchar *buf, int loop, int child, int fnum, int ofs)
{
	uchar v = (loop+child+fnum+(ofs/BUF_SIZE)) % 256;
	memset(buf, v, BUF_SIZE);
}

/* 
   check if a buffer from disk is correct
*/
static void check_buffer(uchar *buf, int loop, int child, int fnum, int ofs)
{
	uchar buf2[BUF_SIZE];
	gen_buffer(buf2, loop, child, fnum, ofs);
	
	if (memcmp(buf, buf2, BUF_SIZE) != 0) {
		int i, j;
		for (i=0;buf[i] == buf2[i] && i<BUF_SIZE;i++) ;
		fprintf(stderr,"Corruption in child %d fnum %d at offset %d\n",
			child, fnum, ofs+i);

		printf("Correct:   ");
		for (j=0;j<MIN(20, BUF_SIZE-i);j++) {
			printf("%02x ", buf2[j+i]);
		}
		printf("\n");

		printf("Incorrect: ");
		for (j=0;j<MIN(20, BUF_SIZE-i);j++) {
			printf("%02x ", buf[j+i]);
		}
		printf("\n");
		exit(1);
	}
}

/*
  create a file with a known data set for a child
 */
static void create_file(const char *dir, int loop, int child, int fnum)
{
	uchar buf[BUF_SIZE];
	int size, fd;
	char fname[1024];

	sprintf(fname, "%s/file%d", dir, fnum);
	fd = open(fname, O_WRONLY|O_CREAT|O_TRUNC, 0644);
	if (fd == -1) {
		perror(fname);
		exit(1);
	}

	for (size=0; size<file_size; size += BUF_SIZE) {
		gen_buffer(buf, loop, child, fnum, size);
		if (pwrite(fd, buf, BUF_SIZE, size) != BUF_SIZE) {
			fprintf(stderr,"Write failed at offset %d\n", size);
			exit(1);
		}
	}

	close(fd);
}

/* 
   check that a file has the right data
 */
static void check_file(const char *dir, int loop, int child, int fnum)
{
	uchar buf[BUF_SIZE];
	int size, fd;
	char fname[1024];

	sprintf(fname, "%s/file%d", dir, fnum);
	fd = open(fname, O_RDONLY);
	if (fd == -1) {
		perror(fname);
		exit(1);
	}

	for (size=0; size<file_size; size += BUF_SIZE) {
		if (pread(fd, buf, BUF_SIZE, size) != BUF_SIZE) {
			fprintf(stderr,"read failed at offset %d\n", size);
			exit(1);
		}
		check_buffer(buf, loop, child, fnum, size);
	}

	close(fd);
}

/* 
   revsusive directory traversal - used for cleanup
   fn() is called on all files/dirs in the tree
 */
void traverse(const char *dir, int (*fn)(const char *))
{
	DIR *d;
	struct dirent *de;

	d = opendir(dir);
	if (!d) return;

	while ((de = readdir(d))) {
		char fname[1024];
		struct stat st;

		if (strcmp(de->d_name,".") == 0) continue;
		if (strcmp(de->d_name,"..") == 0) continue;

		sprintf(fname, "%s/%s", dir, de->d_name);
		if (lstat(fname, &st)) {
			perror(fname);
			continue;
		}

		if (S_ISDIR(st.st_mode)) {
			traverse(fname, fn);
		}

		fn(fname);
	}

	closedir(d);
}

/* the main child function - this creates/checks the file for one child */
static void run_child(int child)
{
	int i, loop;
	char dir[1024];

	sprintf(dir, "%s/child%d", base_dir, child);

	/* cleanup any old files */
	if (remove(dir) != 0 && errno != ENOENT) {
		printf("Child %d cleaning %s\n", child, dir);
		traverse(dir, remove);
		remove(dir);
	}

	if (mkdir(dir, 0755) != 0) {
		perror(dir);
		exit(1);
	}

	for (loop = 0; loop < loop_count; loop++) {
		printf("Child %d loop %d\n", child, loop);
		for (i=0;i<num_files;i++) {
			create_file(dir, loop, child, i);
		}
		for (i=0;i<num_files;i++) {
			check_file(dir, loop, child, i);
		}
	}

	/* cleanup afterwards */
	printf("Child %d cleaning up %s\n", child, dir);
	traverse(dir, remove);
	remove(dir);

	exit(0);
}

static void usage(void)
{
	printf("\n"
"Usage: fstest [options]\n"
"\n"
" -n num_children       set number of child processes\n"
" -f num_files          set number of files\n"
" -s file_size          set file sizes\n"
" -p path               set base path\n"
" -l loops              set loop count\n"
" -h                    show this help message\n");
}

/* main program */
int main(int argc, char *argv[])
{
	int c;
	extern char *optarg;
	extern int optind;
	int num_children = 1;
	int i, status, ret;

	while ((c = getopt(argc, argv, "n:s:f:p:l:h")) != -1) {
		switch (c) {
		case 'n':
			num_children = strtol(optarg, NULL, 0);
			break;
		case 'f':
			num_files = strtol(optarg, NULL, 0);
			break;
		case 's':
			file_size = strtol(optarg, NULL, 0);
			break;
		case 'p':
			base_dir = optarg;
			break;
		case 'l':
			loop_count = strtol(optarg, NULL, 0);
			break;
		case 'h':
			usage();
			exit(0);
		default:
			usage();
			exit(1);
		}
	}

	argc -= optind;
	argv += optind;

	/* fork and run run_child() for each child */
	for (i=0;i<num_children;i++) {
		if (fork() == 0) {
			run_child(i);
			exit(0);
		}
	}

	ret = 0;

	/* wait for children to exit */
	while (waitpid(0, &status, 0) == 0 || errno != ECHILD) {
		if (WEXITSTATUS(status) != 0) {
			ret = WEXITSTATUS(status);
			printf("Child exited with status %d\n", ret);
		}
	}

	if (ret != 0) {
		printf("fstest failed with status %d\n", ret);
	}

	return ret;
}