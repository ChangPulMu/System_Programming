#include "cachelab.h"
#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef struct
{
	int old;
	int tag;
	bool v;
} ln;

typedef struct
{
	ln *line;
} st;

typedef struct
{
	unsigned int snum;
	unsigned int lnum;
	st *set;
} ch;

ch cache;
int sbit, bbit;
unsigned int hit_count = 0, miss_count = 0, eviction_count = 0;

void access(int addr);
void timeset(st *set, int lnum);

int main(int argc, char *argv[])
{
	FILE *f = 0;
	char typ;
	int tmp, ad;

	while ((tmp = getopt(argc, argv, "s:E:b:t:")) != -1)
	{
		if (tmp == 's') // set ���� 2^s
		{
			sbit = atoi(optarg);
			cache.snum = 2 << sbit;
		}
		else if (tmp == 'E') // set�� line ����
		{
			cache.lnum = atoi(optarg);
		}
		else if (tmp == 'b') // block bit�� �� 2^b
		{
			bbit = atoi(optarg);
		}
		else if (tmp == 't') // trace file �б� �������� ����
		{
			f = fopen(optarg, "r");
		}
	}

	cache.set = malloc(sizeof(st) * cache.snum); // set ������ŭ cache �����Ҵ�

	for (int a = 0; a < cache.snum; ++a) // line ������ŭ set �����Ҵ�
    {
		cache.set[a].line = malloc(sizeof(ln) * cache.lnum);
	}

	while (fscanf(f, " %c %x%*c%*d", &typ, &ad) != EOF) {

		if (typ == 'L' || typ == 'S') // Load�� Store�� ���� 1ȸ �޸� ����
	    {
		       access(ad);	
		}
		else if (typ == 'M') // Modify�� ���� 2ȸ �޸� ����
		{
		       access(ad);
		       access(ad);
	    }

	}

	printSummary(hit_count, miss_count, eviction_count); // hit, miss, eviction ���

	for (int a = 0; a < cache.snum; ++a) // ���� �޸� ����
	{
		free(cache.set[a].line);
	}
	free(cache.set);

	fclose(f);

	return 0;
}


void access(int ad)
{
	int tmptag = ad >> (sbit + bbit);
	int tmpset = ad >> bbit;

	unsigned int snum = (0x7fffffff >> (31 - sbit)) & tmpset; // �ּҿ��� set ��ȣ ���ϱ�
	int tag = 0xffffffff & tmptag; // �ּҿ��� tag ���ϱ�

	st *set = &cache.set[snum]; // �ش� ��ȣ�� �´� set�� ����

	for (int a = 0; a < cache.lnum; ++a) // hit���� Ȯ��
	{
		if (set->line[a].v)
		{
			if (set->line[a].tag == tag)
			{
				++hit_count;
				timeset(set, a);

				return;
			}
		}
	}

	++miss_count; // hit ���������Ƿ� miss

	for (int a = 0; a < cache.lnum; ++a) // write�� line�� ã�Ƽ� write
	{
		if (!(set->line[a].v))
		{
			set->line[a].v = true;
			set->line[a].tag = tag;
			timeset(set, a);

			return;
		}
	}

	++eviction_count; // write�� line�� �����Ƿ� evicition

	for (int a = 0; a < cache.lnum; ++a) // ���� ������ line�� ���ŵǰ� ���ۼ���
	{
		if (!(set->line[a].old))
		{
			set->line[a].v = true;
			set->line[a].tag = tag;
			timeset(set, a);

			return;
		}
	}
}


void timeset(st *set, int lnum) // �����ִ� line�ϼ��� old�� 0���� ��������� ��
{
	for (int a = 0; a < cache.lnum; ++a) // ���� ���� line���� ������ line�� old�� ũ�ٸ� ���� line�� old�� 1 ���ҽ�Ŵ
	{
		if (set->line[a].v)
		{
			if (set->line[a].old > set->line[lnum].old)
			{
				--(set->line[a].old);
			}
		}
	}

	set->line[lnum].old = cache.lnum - 1; // ���� ���� line�� old�� �ִ�ġ�� �ʱ�ȭ
}
