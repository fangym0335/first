#include "mycomm.h"
#include "dostring.h"
#include "mystring.h"

void TestIsEmptyString()
{
	mybool ret;

	char *s = NULL;

	DMsg( "Test IsEmptyString" );

	DMsg( "test s=null" );
	ret = IsEmptyString( s );
	DMsg( "ret = [%s]", ( ret )?"TRUE":"FALSE" );

	s = ( char *)Malloc( 10 );
	memset( s, 0, 10 );

	DMsg( "test s=\"\" " );
	ret = IsEmptyString( s );
	DMsg( "ret = [%s]\n", ( ret )?"TRUE":"FALSE" );

	strcpy( s, "12345" );
	MyDebug( "test s=\"12345\" " );
	ret = IsEmptyString( s );
	DMsg( "ret = [%s]\n", ( ret )?"TRUE":"FALSE" );

	Free(s);
	DMsg( "Test IsEmptyString OK" );

	return;
}

void TestRightTrim()
{
	char s1[100];
	char s2[100];

	DMsg( "Test RightTrim" );

	strcpy( s1, " 123456  " );
	DMsg( "test s = [%s]", s1 );
	strcpy( s2, RightTrim( s1 ) );
	DMsg( "s2 = [%s], s1 = [%s]", s2, s1 );

	strcpy( s1, " 1 2   34   5  6  " );
	DMsg( "test s = [%s]", s1 );
	strcpy( s2, RightTrim( s1 ) );
	DMsg( "s2 = [%s], s1 = [%s]", s2, s1 );

	strcpy( s1,"        	    " );
	DMsg( "test s = [%s]", s1 );
	strcpy( s2, RightTrim( s1 ) );
	DMsg( "s2 = [%s], s1 = [%s]", s2, s1 );

	strcpy( s1,"" );
	DMsg( "test s = [%s]", s1 );
	strcpy( s2, RightTrim( s1 ) );
	DMsg( "s2 = [%s], s1 = [%s]", s2, s1 );
/*
	DMsg( "test s = NULL", s1 );
	strcpy( s2, RightTrim( NULL ) );
	DMsg( "s2 = [%s], s1 = [%s]", s2, s1 );
*/
	DMsg( "Test RightTrim OK" );

}

void TestLeftTrim()
{
	char s1[100];
	char s2[100];

	DMsg( "Test LeftTrim" );

	strcpy( s1, " 123456  " );
	DMsg( "test s = [%s]", s1 );
	strcpy( s2, LeftTrim( s1 ) );
	DMsg( "s2 = [%s], s1 = [%s]", s2, s1 );

	strcpy( s1, " 1  234  5  6  " );
	DMsg( "test s = [%s]", s1 );
	strcpy( s2, LeftTrim( s1 ) );
	DMsg( "s2 = [%s], s1 = [%s]", s2, s1 );

	strcpy( s1,"        	    " );
	DMsg( "test s = [%s]", s1 );
	strcpy( s2, LeftTrim( s1 ) );
	DMsg( "s2 = [%s], s1 = [%s]", s2, s1 );

	strcpy( s1,"" );
	DMsg( "test s = [%s]", s1 );
	strcpy( s2, LeftTrim( s1 ) );
	DMsg( "s2 = [%s], s1 = [%s]", s2, s1 );

/*	DMsg( "test s = NULL", s1 );
	strcpy( s2, LeftTrim( NULL ) );
	DMsg( "s2 = [%s], s1 = [%s]", s2, s1 );
*/
	DMsg( "Test LeftTrim OK" );

}

void TestAllTrim()
{
	char s1[100];
	char s2[100];

	DMsg( "Test AllTrim" );

	strcpy( s1, " 123456  " );
	DMsg( "test s = [%s]", s1 );
	strcpy( s2, AllTrim( s1 ) );
	DMsg( "s2 = [%s], s1 = [%s]", s2, s1 );

	strcpy( s1, " 1   2  34   5  6  " );
	DMsg( "test s = [%s]", s1 );
	strcpy( s2, AllTrim( s1 ) );
	DMsg( "s2 = [%s], s1 = [%s]", s2, s1 );

	strcpy( s1,"        	    " );
	DMsg( "test s = [%s]", s1 );
	strcpy( s2, AllTrim( s1 ) );
	DMsg( "s2 = [%s], s1 = [%s]", s2, s1 );

	strcpy( s1,"" );
	DMsg( "test s = [%s]", s1 );
	strcpy( s2, AllTrim( s1 ) );
	DMsg( "s2 = [%s], s1 = [%s]", s2, s1 );
/*
	DMsg( "test s = NULL", s1 );
	strcpy( s2, AllTrim( NULL ) );
	DMsg( "s2 = [%s], s1 = [%s]", s2, s1 );
*/
	DMsg( "Test AllTrim OK" );

}

void TestStringToLower()
{
	char s1[100];
	char s2[100];

	DMsg( "Test StringToLower" );

	Trace(strcpy( s1, "akjfaoei" ));
	DMsg( "test s = [%s]", s1 );
	Trace(strcpy( s2, StrToLower(s1) ));
	DMsg( "s2 = [%s], s1 = [%s]", s2, s1 );

	strcpy( s1, "123akDEaoei" );
	DMsg( "test s = [%s]", s1 );
	strcpy( s2, StrToLower(s1) );
	DMsg( "s2 = [%s], s1 = [%s]", s2, s1 );

	strcpy( s1, "´ÞºéÇåei1   " );
	DMsg( "test s = [%s]", s1 );
	strcpy( s2, StrToLower(s1) );
	DMsg( "s2 = [%s], s1 = [%s]", s2, s1 );

	strcpy( s1, "" );
	DMsg( "test s = [%s]", s1 );
	strcpy( s2, StrToLower(s1) );
	DMsg( "s2 = [%s], s1 = [%s]", s2, s1 );

/*
	strcpy( s1, "NULL" );
	strcpy( s2, StrToLower(NULL) );
*/
	DMsg( "Test StringToLower OK" );
}

void TestStringToUpper()
{
	char s1[100];
	char s2[100];

	DMsg( "Test StringToUpper" );

	strcpy( s1, "akjfaoei" );
	DMsg( "test s = [%s]", s1 );
	strcpy( s2, StrToUpper(s1) );
	DMsg( "s2 = [%s], s1 = [%s]", s2, s1 );

	strcpy( s1, "123akDEaoei" );
	DMsg( "test s = [%s]", s1 );
	strcpy( s2, StrToUpper(s1) );
	DMsg( "s2 = [%s], s1 = [%s]", s2, s1 );

	strcpy( s1, "´ÞºéÇåei1   " );
	DMsg( "test s = [%s]", s1 );
	strcpy( s2, StrToUpper(s1) );
	DMsg( "s2 = [%s], s1 = [%s]", s2, s1 );

	strcpy( s1, "" );
	DMsg( "test s = [%s]", s1 );
	strcpy( s2, StrToUpper(s1) );
	DMsg( "s2 = [%s], s1 = [%s]", s2, s1 );
/*
	strcpy( s1, "NULL" );
	strcpy( s2, StrToUpper(NULL) );
*/
	DMsg( "Test StringToUpper OK" );
}

void TestFillBuff()
{
	char s[100];
	int ret;


	DMsg( "Test FillBuff" );

	strcpy( s, "123456" );
	DMsg( " test s = [%s]", s );
	ret = FillBuff( s, 20, 'i', FILLBEHIND );
	DMsg( "ret = [%d], s = [%s], len = [%d]", ret, s, strlen(s) );

	strcpy( s, "123456" );
	DMsg( " test s = [%s]", s );
	ret = FillBuff( s,  20, 'i', FILLAHEAD );
	DMsg( "ret = [%d], s = [%s], len = [%d]", ret, s, strlen(s) );

/*	FillBuff( s, 20, 20, 'i', FILLAHEAD );
	FillBuff( s, 19, 20, 'i', FILLBEHIND );
	FillBuff( s, 21, 20, 'i', 3 );
	FillBuff( s, 0, 20, 'i', FILLBEHIND );
	FillBuff( s, 19, 0, 'i', FILLBEHIND );
*/


	strcpy( s, "123456" );
	DMsg( " test s = [%s]", s );
	FillBuff( s, 4294967294, 'i', FILLAHEAD );
	DMsg( "ret = [%d], s = [%s], len = [%d]", ret, s, strlen(s) );

	DMsg( "Test FillBuff OK" );
}

void TestFillBuffWithMod()
{
	char s[100];
	int ret;

	DMsg( "Test FillBuffWithMod" );

	strcpy( s, "11100001101" );
	DMsg( " test s = [%s]", s );
	ret = FillBuffWithMod( s, 24, 'i', FILLBEHIND );
	DMsg( "ret = [%d], s = [%s], len = [%d]", ret, s, strlen(s) );

	strcpy( s, "11100001101" );
	DMsg( " test s = [%s]", s );
	ret = FillBuffWithMod( s,  4, 'i', FILLAHEAD );
	DMsg( "ret = [%d], s = [%s], len = [%d]", ret, s, strlen(s) );

	strcpy( s, "011100001101" );
	DMsg( " test s = [%s]", s );
	ret = FillBuffWithMod( s, 4, 'i', FILLAHEAD );
	DMsg( "ret = [%d], s = [%s], len = [%d]", ret, s, strlen(s) );

/*	FillBuffWithMod( s, 5, 4, 'i', FILLAHEAD );
	FillBuffWithMod( s, 12, 4, 'i', FILLBEHIND );
	FillBuffWithMod( s, sizeof(s), 101, 'i', FILLBEHIND );
	FillBuffWithMod( s, 0, 20, 'i', FILLBEHIND );
	FillBuffWithMod( s, 20, 0, 'i', FILLBEHIND );
*/


	strcpy( s, "11100001101" );
	DMsg( " test s = [%s]", s );
	ret = FillBuffWithMod( s, -2, 'i', FILLAHEAD );
	DMsg( "ret = [%d], s = [%s], len = [%d]", ret, s, strlen(s) );



	DMsg( "Test FillBuffWithMod OK" );

}

void TestCommArea()
{
	char s[100];
	String * p;

	Trace(p = InitCommArea(10));

	Trace(SelectWorkingCommArea(p));

	Trace(SetValue( "1", "T1" ));
	ShowString( p );
	Trace(SetValue( "%", "T%" ));
	ShowString( p );
	Trace(SetValue(";", "T;" ));
	ShowString( p );
	Trace(SetValue( "=", "T=" ));
	ShowString( p );
	Trace(SetValue( "This%is;5&", "This%is;5&" ));
	ShowString( p );
	Trace(Assert( GetValue("1", s, sizeof(s) ) >= 0 ));
	DMsg( "key = [%s], value = [%s]", "1", s );

	Trace(Assert( GetValue( "%", s, sizeof(s) ) >= 0 ));
	DMsg( "key = [%s], value = [%s]", "%", s );

	Trace(Assert( GetValue( ";", s, sizeof(s) ) >= 0 ));
	DMsg( "key = [%s], value = [%s]", ";", s );

	Trace(Assert( GetValue( "This%is;5&", s, sizeof(s) ) >= 0 ));
	DMsg( "key = [%s], value = [%s]", "This%is;5&", s );

	Trace(DelKey( "1" ));
	ShowString( p );
	Trace(DelKey( ";" ));
	ShowString( p );
	Trace(DelKey( "=" ));
	ShowString( p );
	Trace(DelKey( "This%is;5&" ));
	ShowString( p );
	Trace(DelKey( "%" ));
	ShowString( p );
	DelString( &p );

}

void TestString()
{
	String *pString;

	Trace(pString = NewString( 10 ));

	ClearString( pString );

	Trace(CommStringAssign( pString, "abc  = [%d], d = [%s]", 1, "abc" ));
	ShowString( pString );

	Trace(CommStringAdd( pString, "efag" ));
	ShowString( pString );

	Trace(StringAssign( pString, "chq" ));
	ShowString( pString );

	DelString( &pString );

}

main()
{
	InitMemCtl();
	TestIsEmptyString();
	TestRightTrim();
	TestLeftTrim();
	TestAllTrim();
	TestStringToLower();
	TestStringToUpper();
	TestFillBuff();
	TestFillBuffWithMod();
	TestString();
	TestCommArea();


	return 0;
}

