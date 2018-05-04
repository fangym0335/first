#ifndef __BITSET_H__
#define __BITSET_H__

#include <string.h>
#include <limits.h>
#include "mycomm.h"
#include "dostring.h"

typedef struct {
	int	bitCnt;				/*位的个数. 位的范围为0 － bitCnt-1 */
	size_t	size;			/*字符串的空间*/
	unsigned char*	p;		/*存储实际的位信息*/
}BITSET;

/*成功返回指向分配的BitSet的指针.内存溢出时返回NULL*/
BITSET *NewBitSet( int maxBitCnt );
BITSET *NewBitSetWithValue( const unsigned char *p, size_t len ); /*p中每个字节代表4位*/
BITSET *NewBitSetWithBinArray( const unsigned char *p, size_t len ); /*p的每个字节只能取'0'或者'1'.每个字节代表一位*/

void	SetBitOn( BITSET *pBitSet, int setBit );	/*setBit 从0开始到bitCnt-1*/
void	SetBitOff( BITSET *pBitSet, int setBit );
void	FillBitSet( BITSET *pBitSet );
void	ClearBitSet( BITSET *pBitSet );
int		IsBitOn( const BITSET *pBitSet, int setBit );
void	DelBitSet( BITSET **pBitSet );
char*	DumpBitSetToBinArray( const BITSET *pBitSet, char *p, size_t bufsize ); /*bufsize为p的size.包括结束符*/
int		DumpBitSet( const BITSET *pBitSet, int *bitCnt, char *p, size_t bufsize );

#endif
