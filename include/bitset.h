#ifndef __BITSET_H__
#define __BITSET_H__

#include <string.h>
#include <limits.h>
#include "mycomm.h"
#include "dostring.h"

typedef struct {
	int	bitCnt;				/*λ�ĸ���. λ�ķ�ΧΪ0 �� bitCnt-1 */
	size_t	size;			/*�ַ����Ŀռ�*/
	unsigned char*	p;		/*�洢ʵ�ʵ�λ��Ϣ*/
}BITSET;

/*�ɹ�����ָ������BitSet��ָ��.�ڴ����ʱ����NULL*/
BITSET *NewBitSet( int maxBitCnt );
BITSET *NewBitSetWithValue( const unsigned char *p, size_t len ); /*p��ÿ���ֽڴ���4λ*/
BITSET *NewBitSetWithBinArray( const unsigned char *p, size_t len ); /*p��ÿ���ֽ�ֻ��ȡ'0'����'1'.ÿ���ֽڴ���һλ*/

void	SetBitOn( BITSET *pBitSet, int setBit );	/*setBit ��0��ʼ��bitCnt-1*/
void	SetBitOff( BITSET *pBitSet, int setBit );
void	FillBitSet( BITSET *pBitSet );
void	ClearBitSet( BITSET *pBitSet );
int		IsBitOn( const BITSET *pBitSet, int setBit );
void	DelBitSet( BITSET **pBitSet );
char*	DumpBitSetToBinArray( const BITSET *pBitSet, char *p, size_t bufsize ); /*bufsizeΪp��size.����������*/
int		DumpBitSet( const BITSET *pBitSet, int *bitCnt, char *p, size_t bufsize );

#endif
