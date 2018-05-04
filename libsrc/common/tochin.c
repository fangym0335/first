#include "tochin.h"

char	num[10][3] = { 	"¡„",
						"“º",
						"∑°",
						"»˛",
                     	"À¡",
						"ŒÈ",
						"¬Ω",
						"∆‚",
						"∞∆",
						"æ¡"};

char	num1[5][3] = {	" ∞",
						"∞€",
						"«™",
						"ÕÚ",
						"“⁄"};

char	unit[3][3] = { 	"‘≤",
						"Ω«",
						"∑÷"};

char	com[1][3]  = { 	"’˚" };

int to_int(char c_num)
{
	char tmp[2];
	int	 tmp_int;

	tmp[0] = c_num;
	tmp[1] = '\0';
	sscanf(tmp, "%d", &tmp_int);
	return(tmp_int);
}

long	to_ge(double d_num)
{
	long	tmp;

	tmp = (long)d_num;
	tmp = tmp % 10;
	return(tmp);
}
long	to_ten(double d_num)
{
	long	tmp;

	tmp = (long)d_num;
	tmp = tmp % 100;
	tmp = tmp / 10;
	return(tmp);
}

long	to_bai(double d_num)
{
	long	tmp;

	tmp = (long)d_num;
	tmp = tmp % 1000;
	tmp = tmp / 100;
	return(tmp);
}

long	to_qian(double d_num)
{
	long	tmp;

	tmp = (long)d_num;
	tmp = tmp % 10000;
	tmp = tmp / 1000;
	return(tmp);
}

long	to_wan(double d_num)
{
	long	tmp;

	tmp = (long)d_num;
	tmp = tmp % 100000000;
	tmp = tmp / 10000;
	return(tmp);
}

long	to_yi(double d_num)
{
	long	tmp;

	tmp = (long)d_num;
	tmp = tmp / 100000000;
	return(tmp);
}

void to_chin(double d_num, char * chin)
{
	char buf[18];
	char result[64][2];
    int	 loc = 1;

	memset(result, ' ' , sizeof(result));
	sprintf(buf, "%017.2lf", d_num);

    if (memcmp(buf, "000000000000000.00", 17) == 0)
    {
      result[64 - loc][0] = com[0][0]; /*’˚*/
      result[64 - loc][1] = com[0][1];
      loc ++;
      result[64 - loc][0] = unit[0][0]; /*‘≤*/
      result[64 - loc][1] = unit[0][1];
      loc ++;
      result[64 - loc][0] = num[0][0]; /*¡„*/
      result[64 - loc][1] = num[0][1];
      loc ++;
      goto end;
    }

    if (buf[16] == '0')
    {
      result[64 - loc][0] = com[0][0]; /*’˚*/
      result[64 - loc][1] = com[0][1];
      loc ++;
    }
    else
    {
      result[64 - loc][0] = unit[2][0]; /*∑÷*/
      result[64 - loc][1] = unit[2][1];
      loc ++;
      result[64 - loc][0] = num[to_int(buf[16])][0];
      result[64 - loc][1] = num[to_int(buf[16])][1];
      loc ++;
	}

    if (buf[15] != '0')
    {
      result[64 - loc][0] = unit[1][0]; /*Ω«*/
      result[64 - loc][1] = unit[1][1];
      loc ++;
      result[64 - loc][0] = num[to_int(buf[15])][0];
      result[64 - loc][1] = num[to_int(buf[15])][1];
      loc ++;
    }
	else
	{
      if (to_yi(d_num) == 0 && to_wan(d_num) == 0 && to_qian(d_num) == 0
          && to_bai(d_num) == 0 && to_ten(d_num) == 0 && to_ge(d_num) == 0)
       goto end;
      if (buf[16] != '0')
      {
        result[64 - loc][0] = num[0][0]; /*¡„*/
        result[64 - loc][1] = num[0][1];
        loc ++;
      }
	}

    if (to_yi(d_num) == 0 && to_wan(d_num) == 0 && to_qian(d_num) == 0
        && to_bai(d_num) == 0 && to_ten(d_num) == 0 && to_ge(d_num) == 0)
       goto end;

    result[64 - loc][0] = unit[0][0]; /*‘≤*/
    result[64 - loc][1] = unit[0][1];
    loc ++;

    if (buf[13] != '0')
	{
      result[64 - loc][0] = num[to_int(buf[13])][0];
      result[64 - loc][1] = num[to_int(buf[13])][1];
      loc ++;
	}

    if (to_yi(d_num) == 0 && to_wan(d_num) == 0 && to_qian(d_num) == 0
        && to_bai(d_num) == 0 && to_ten(d_num) == 0)
       goto end;

    if (buf[12] != '0')
	{
      result[64 - loc][0] = num1[0][0];  /* ∞*/
      result[64 - loc][1] = num1[0][1];
      loc ++;
      result[64 - loc][0] = num[to_int(buf[12])][0];
      result[64 - loc][1] = num[to_int(buf[12])][1];
      loc ++;
	}
	else
	{
      if (buf[13] != '0')
      {
        result[64 - loc][0] = num[0][0]; /*¡„*/
        result[64 - loc][1] = num[0][1];
        loc ++;
      }
	}

    if (to_yi(d_num) == 0 && to_wan(d_num) == 0 && to_qian(d_num) == 0
        && to_bai(d_num) == 0)
       goto end;

    if (buf[11] != '0')
	{
      result[64 - loc][0] = num1[1][0];  /*∞Ÿ*/
      result[64 - loc][1] = num1[1][1];
      loc ++;
      result[64 - loc][0] = num[to_int(buf[11])][0];
      result[64 - loc][1] = num[to_int(buf[11])][1];
      loc ++;
	}
	else
    {
      if (memcmp(result[64 - loc + 1], "¡„", 2) != 0 &&
          memcmp(result[64 - loc + 1], "‘≤", 2) != 0)
	  {
        result[64 - loc][0] = num[0][0]; /*¡„*/
        result[64 - loc][1] = num[0][1];
        loc ++;
	  }
	}

    if (to_yi(d_num) == 0 && to_wan(d_num) == 0 && to_qian(d_num) == 0)
       goto end;

    if (buf[10] != '0')
	{
      result[64 - loc][0] = num1[2][0];  /*«ß*/
      result[64 - loc][1] = num1[2][1];
      loc ++;
      result[64 - loc][0] = num[to_int(buf[10])][0];
      result[64 - loc][1] = num[to_int(buf[10])][1];
      loc ++;
	}
	else
	{
      if (memcmp(result[64 - loc + 1], "¡„", 2) != 0 &&
          memcmp(result[64 - loc + 1], "‘≤", 2) != 0 )
	  {
        result[64 - loc][0] = num[0][0]; /*¡„*/
        result[64 - loc][1] = num[0][1];
        loc ++;
	  }
	}

    if (to_yi(d_num) == 0 && to_wan(d_num) == 0)
       goto end;

    if (to_wan(d_num) != 0)
	{
      result[64 - loc][0] = num1[3][0];  /*ÕÚ*/
      result[64 - loc][1] = num1[3][1];
      loc ++;

      if (buf[9] != '0')
	  {
        result[64 - loc][0] = num[to_int(buf[9])][0];
        result[64 - loc][1] = num[to_int(buf[9])][1];
        loc ++;
	  }
      if (memcmp(buf, "000000000", 9) == 0)
         goto end;

      if (buf[8] != '0')
	  {
        result[64 - loc][0] = num1[0][0];  /* ∞*/
        result[64 - loc][1] = num1[0][1];
        loc ++;
        result[64 - loc][0] = num[to_int(buf[8])][0];
        result[64 - loc][1] = num[to_int(buf[8])][1];
        loc ++;
	  }
      else
      {
        if (memcmp(result[64 - loc + 1], "ÕÚ", 2) != 0 &&
            memcmp(result[64 - loc + 1], "¡„", 2) != 0)
	    {
          result[64 - loc][0] = num[0][0]; /*¡„*/
          result[64 - loc][1] = num[0][1];
          loc ++;
	    }
      }
      if (memcmp(buf, "00000000", 8) == 0)
         goto end;
      if (buf[7] != '0')
	  {
        result[64 - loc][0] = num1[1][0];  /*∞Ÿ*/
        result[64 - loc][1] = num1[1][1];
        loc ++;
        result[64 - loc][0] = num[to_int(buf[7])][0];
        result[64 - loc][1] = num[to_int(buf[7])][1];
        loc ++;
	  }
      else
      {
        if (memcmp(result[64 - loc + 1], "ÕÚ", 2) != 0 &&
            memcmp(result[64 - loc + 1], "¡„", 2) != 0)
	    {
          result[64 - loc][0] = num[0][0]; /*¡„*/
          result[64 - loc][1] = num[0][1];
          loc ++;
	    }
      }

      if (memcmp(buf, "0000000", 7) == 0)
         goto end;

      if (buf[6] != '0')
	  {
        result[64 - loc][0] = num1[2][0];  /*«ß*/
        result[64 - loc][1] = num1[2][1];
        loc ++;
        result[64 - loc][0] = num[to_int(buf[6])][0];
        result[64 - loc][1] = num[to_int(buf[6])][1];
        loc ++;
	  }
      else
      {
        if (memcmp(result[64 - loc + 1], "ÕÚ", 2) != 0 && to_yi(d_num) != 0 &&
            memcmp(result[64 - loc + 1], "¡„", 2) != 0)
	    {
          result[64 - loc][0] = num[0][0]; /*¡„*/
          result[64 - loc][1] = num[0][1];
          loc ++;
	    }
      }
	}
    else
	{
      if (memcmp(result[64 - loc + 1], "¡„", 2) != 0)
	  {
        result[64 - loc][0] = num[0][0]; /*¡„*/
        result[64 - loc][1] = num[0][1];
        loc ++;
	  }
	}
    if (to_yi(d_num) == 0)
       goto end;

    result[64 - loc][0] = num1[4][0];  /*“⁄*/
    result[64 - loc][1] = num1[4][1];
    loc ++;

    if (buf[5] != '0')
	{
      result[64 - loc][0] = num[to_int(buf[5])][0];
      result[64 - loc][1] = num[to_int(buf[5])][1];
      loc ++;
	}

    if (memcmp(buf, "00000", 5) == 0)
       goto end;

    if (buf[4] != '0')
	{
      result[64 - loc][0] = num1[0][0];  /* ∞*/
      result[64 - loc][1] = num1[0][1];
      loc ++;
      result[64 - loc][0] = num[to_int(buf[4])][0];
      result[64 - loc][1] = num[to_int(buf[4])][1];
      loc ++;
	}
    else
    {
      if (memcmp(result[64 - loc + 1], "“⁄", 2) != 0 &&
          memcmp(result[64 - loc + 1], "¡„", 2) != 0)
	  {
        result[64 - loc][0] = num[0][0]; /*¡„*/
        result[64 - loc][1] = num[0][1];
        loc ++;
	  }
    }
    if (memcmp(buf, "0000", 4) == 0)
       goto end;

    if (buf[3] != '0')
	{
      result[64 - loc][0] = num1[1][0];  /*∞Ÿ*/
      result[64 - loc][1] = num1[1][1];
      loc ++;
      result[64 - loc][0] = num[to_int(buf[3])][0];
      result[64 - loc][1] = num[to_int(buf[3])][1];
      loc ++;
	}
    else
    {
      if (memcmp(result[64 - loc + 1], "“⁄", 2) != 0 && buf[4] != '0'&&
          memcmp(result[64 - loc + 1], "¡„", 2) != 0)
	  {
        result[64 - loc][0] = num[0][0]; /*¡„*/
        result[64 - loc][1] = num[0][1];
        loc ++;
	  }
    }

    if (memcmp(buf, "000", 3) == 0)
       goto end;

    if (buf[2] != '0')
	{
      result[64 - loc][0] = num1[2][0];  /*«ß*/
      result[64 - loc][1] = num1[2][1];
      loc ++;
      result[64 - loc][0] = num[to_int(buf[2])][0];
      result[64 - loc][1] = num[to_int(buf[2])][1];
      loc ++;
	}
    else
    {
      if (memcmp(result[64 - loc + 1], "“⁄", 2) != 0)
	  {
        result[64 - loc][0] = num[0][0]; /*¡„*/
        result[64 - loc][1] = num[0][1];
        loc ++;
	  }
    }

    if (memcmp(buf, "00", 2) == 0)
       goto end;

    if (buf[1] != '0')
	{
      result[64 - loc][0] = num1[3][0];  /*ÕÚ*/
      result[64 - loc][1] = num1[3][1];
      loc ++;
      result[64 - loc][0] = num[to_int(buf[1])][0];
      result[64 - loc][1] = num[to_int(buf[1])][1];
      loc ++;
	}

    if (buf[0] != '0')
	{
      result[64 - loc][0] = num1[0][0];  /* ∞*/
      result[64 - loc][1] = num1[0][1];
      loc ++;
      result[64 - loc][0] = num[to_int(buf[0])][0];
      result[64 - loc][1] = num[to_int(buf[0])][1];
      loc ++;
	}

    end:
    memcpy(chin, result[64 - loc + 1], (loc- 1)*2);
	chin[(loc-1)*2]=0;
}

