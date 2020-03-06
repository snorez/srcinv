#include <stdio.h>

void test0(int a)
{
	fprintf(stderr, "%d\n", a);
}

void test1(int a)
{
	fprintf(stderr, "%d\n", a+1);
}

int main(int argc, char *argv[])
{
	void (*cb)(int);
	if (argc == 1)
		cb = test0;
	else
		cb = test1;

	cb(1);
	return 0;
}
