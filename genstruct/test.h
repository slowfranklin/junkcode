#define _LEN(x)
#define GENSTRUCT

GENSTRUCT struct test2 {
	int x1;
	char *foo;
	struct test2 *next;
};

GENSTRUCT struct test1 {
	char foo[100];
	char *foo2[20];
	int xlen;
	int *iarray; _LEN(xlen);
	unsigned slen;
	char **strings; _LEN(slen);
	char *s2[5];
	double d1, d2, d3;
	struct test2 *test2;
	int alen;
	struct test2 *test2_array; _LEN(alen);
	struct test2 *test2_fixed[2];
	int plen;
	struct test2 **test2_parray; _LEN(plen)
};
