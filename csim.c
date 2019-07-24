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
		if (tmp == 's') // set 갯수 2^s
		{
			sbit = atoi(optarg);
			cache.snum = 2 << sbit;
		}
		else if (tmp == 'E') // set당 line 갯수
		{
			cache.lnum = atoi(optarg);
		}
		else if (tmp == 'b') // block bit의 수 2^b
		{
			bbit = atoi(optarg);
		}
		else if (tmp == 't') // trace file 읽기 형식으로 열기
		{
			f = fopen(optarg, "r");
		}
	}

	cache.set = malloc(sizeof(st) * cache.snum); // set 갯수만큼 cache 동적할당

	for (int a = 0; a < cache.snum; ++a) // line 갯수만큼 set 동적할당
    {
		cache.set[a].line = malloc(sizeof(ln) * cache.lnum);
	}

	while (fscanf(f, " %c %x%*c%*d", &typ, &ad) != EOF) {

		if (typ == 'L' || typ == 'S') // Load나 Store일 때는 1회 메모리 접근
	    {
		       access(ad);	
		}
		else if (typ == 'M') // Modify일 때는 2회 메모리 접근
		{
		       access(ad);
		       access(ad);
	    }

	}

	printSummary(hit_count, miss_count, eviction_count); // hit, miss, eviction 출력

	for (int a = 0; a < cache.snum; ++a) // 동적 메모리 해제
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

	unsigned int snum = (0x7fffffff >> (31 - sbit)) & tmpset; // 주소에서 set 번호 구하기
	int tag = 0xffffffff & tmptag; // 주소에서 tag 구하기

	st *set = &cache.set[snum]; // 해당 번호에 맞는 set을 선택

	for (int a = 0; a < cache.lnum; ++a) // hit인지 확인
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

	++miss_count; // hit 실패했으므로 miss

	for (int a = 0; a < cache.lnum; ++a) // write할 line을 찾아서 write
	{
		if (!(set->line[a].v))
		{
			set->line[a].v = true;
			set->line[a].tag = tag;
			timeset(set, a);

			return;
		}
	}

	++eviction_count; // write할 line이 없으므로 evicition

	for (int a = 0; a < cache.lnum; ++a) // 가장 오래된 line이 제거되고 재작성됨
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


void timeset(st *set, int lnum) // 오래있던 line일수록 old가 0으로 가까워지게 함
{
	for (int a = 0; a < cache.lnum; ++a) // 새로 들어온 line보다 기존의 line의 old가 크다면 기존 line의 old를 1 감소시킴
	{
		if (set->line[a].v)
		{
			if (set->line[a].old > set->line[lnum].old)
			{
				--(set->line[a].old);
			}
		}
	}

	set->line[lnum].old = cache.lnum - 1; // 새로 들어온 line의 old를 최대치로 초기화
}
