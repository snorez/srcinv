/* test show_detail */
#include <stdio.h>

struct xxx {
	int aaa;
	union {
		int bbb;
		int ccc;
	};
	int ddd;
	struct {
		int eee;
		int fff;
	};
	int ggg;
};

static void init_xxx(struct xxx *tmp)
{
	tmp->aaa = 1;
	tmp->bbb = 2;
	tmp->ccc = 3;
	tmp->ddd = 4;
	tmp->eee = 5;
	tmp->fff = 6;
	tmp->ggg = 7;
}

int main(int argc, char *argv[])
{
	struct xxx tmp;
	init_xxx(&tmp);
	return 0;
}
