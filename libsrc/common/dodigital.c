/*******************************
	*CopyRight Cuihq
	*All Right Reserved
	*
	*文件名称: dodigital.c
	*摘要:
	*
	*作者: 崔洪清
	*日期: 2004/06/01
*******************************/
#include "dodigital.h"

int IsDigital( const char *dec )
{
	int i;

	Assert( dec != NULL && *dec != 0 );

	for( i = 0; i < strlen( dec ); i++ )
		if( dec[i] < '0' || dec[i] > '9' )
			return FALSE;

	return TRUE;
}

int IsHex( const char * hex )
{
	int		i;

	Assert( hex != NULL && *hex != 0 );

	if( ( hex[0] == '0' ) && ( hex[1] == 'x' || hex[1] == 'X' ) )
		i = 2;
	else if( hex[0] == 'x' || hex[0] == 'X' )
		i = 1;
	else
		i = 0;

	for( ; i < strlen( hex ); i++ )
		if ( ( hex[ i ] < '0' || hex[ i ] > '9' ) &&
		 	( hex[ i ] < 'a' || hex[ i ] > 'f' ) &&
		 	( hex[ i ] < 'A' || hex[ i ] > 'F' ) )
		 	return FALSE;

	return TRUE;
}

int	IsBin( const char * bin )
{
	int		i;

	Assert( bin != NULL && *bin != 0 );

	for( i = 0; i < strlen( bin ); i++ )
		if( ( bin[i] != '0' ) && ( bin[i] != '1' ) )
			return FALSE;

	return TRUE;
}


void DecToHex( long dec, char *hex )
{
	Assert( hex != NULL );
	sprintf( hex,"%X",dec );
}

void DecToBin( long dec,char *bin )
{
	char	hex[ 17 ];

	Assert( bin != NULL );

	DecToHex( dec, hex );

	HexToBin( hex, bin );
}

void HexToBin( const char *hex, char *bin )
{
	int		i;

	Assert( hex != NULL && *hex != 0 && IsHex(hex) );
	Assert( bin != NULL);

	if( ( hex[0] == '0' ) && ( hex[1] == 'x' || hex[1] == 'X' ) )
		i = 2;
	else if( hex[0] == 'x' || hex[0] == 'X' )
		i = 1;
	else
		i = 0;

	*bin = 0;

	for( ; i < strlen( hex ); i++ )
	{
		switch( hex[ i ] )
		{
			case '0':
				strcat( bin, "0000" );
				break;
			case '1':
				strcat( bin, "0001" );
				break;
			case '2':
				strcat( bin, "0010" );
				break;
			case '3':
				strcat( bin, "0011" );
				break;
			case '4':
				strcat( bin, "0100" );
				break;
			case '5':
				strcat( bin, "0101" );
				break;
			case '6':
				strcat( bin, "0110" );
				break;
			case '7':
				strcat( bin, "0111" );
				break;
			case '8':
				strcat( bin, "1000" );
				break;
			case '9':
				strcat( bin, "1001" );
				break;
			case 'A':
			case 'a':
				strcat( bin, "1010" );
				break;
			case 'B':
			case 'b':
				strcat( bin, "1011" );
				break;
			case 'C':
			case 'c':
				strcat( bin, "1100" );
				break;
			case 'D':
			case 'd':
				strcat( bin, "1101" );
				break;
			case 'E':
			case 'e':
				strcat( bin, "1110" );
				break;
			case 'F':
			case 'f':
				strcat( bin, "1111" );
				break;
		}
	}
	return;
}


void HexToDec( const char *hex, long *dec )
{

	Assert( hex != NULL && *hex != 0 && IsHex(hex) );
	Assert( dec != NULL );

	*dec = 0;

	sscanf( hex, "%X", dec );

	return;
}

void BinToHex( const char *bin, char *hex )
{
	char	*tmpBin;
	size_t	tmpBinLen;
	char	tmpStr[5];
	int 	tmpChr;
	int		i;
	int		j;
	size_t	len;

	Assert( bin != NULL && *bin != 0 && IsBin( bin ) );
	Assert( hex != NULL );

	len = ( strlen( bin ) % 4 == 0 ) ? ( strlen( bin ) / 4 ) : ( strlen( bin ) /4 + 1 );

	tmpBinLen = len * 4 + 1;
	tmpBin = (char *)Malloc( tmpBinLen );
	if( tmpBin == NULL )
	{
		*hex = 0;
		return;
	}

	strcpy( tmpBin, bin );
	if( strlen(tmpBin)%4 != 0 )
	{
		if( FillBuffWithMod( tmpBin, 4, '0', FILLAHEAD ) < 0 )
		{
			*hex = 0;
			Free( tmpBin );
			return;
		}
	}
	memset( tmpStr, 0, sizeof( tmpStr ) );

	for( i = 0,j = 0; i < strlen( tmpBin ); i = i + 4,j++ )
	{
		strncpy( tmpStr, tmpBin + i, 4 );

		tmpChr = 0;
		if( tmpStr[0] == '1' )
			tmpChr = tmpChr + 0x08;
		if( tmpStr[1] == '1' )
			tmpChr = tmpChr + 0x04;
		if( tmpStr[2] == '1' )
			tmpChr = tmpChr + 0x02;
		if( tmpStr[3] == '1' )
			tmpChr = tmpChr + 0x01;

		if( tmpChr < 10 )
			hex[j] = tmpChr + '0';
		else
			hex[j] = tmpChr + 'A' - 10;
	}

	hex[j] = 0;
	Free( tmpBin );
	return;
}

void BinToDec( const char *bin, long *dec )
{
	char	hex[ 64 ];

	Assert( bin != NULL && *bin != 0 && IsBin(bin) );
	Assert( dec != NULL );

	*dec = 0;
	BinToHex( bin, hex );
	if( *hex == 0 )
	{
		*dec = 0;
		return;
	}
	HexToDec( hex, dec );
}

double MaxFloat( double a, double b )
{
	return a > b ? a : b;
}

double MinFloat( double a, double b )
{
	return a > b ? b : a;
}

int FloatComp( double a, double b )
{
	if( a - b > CONST_ZERO )
		return 1;
	else if( ( a - b >= -CONST_ZERO ) && ( a - b <= CONST_ZERO ) )
		return 0;
	else
		return -1;
}
