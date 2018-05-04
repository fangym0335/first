/*
 * =====================================================================================
 *
 *       Filename:  tcc.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  05/30/2011 05:49:27 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Guojie Zhu (mn), <zhugj@css-intelligent.com>
 *        Company:  CSSI
 *
 * =====================================================================================
 */

#ifndef TCC_H
#define TCC_H   

#define TCC_ADJUST_DATA_FILE    "/tmp/tcc_adjust.csv"      // 校罐项目采集的数据文件


int dump_tcc_adjust_data_file();
int write_tcc_adjust_data(const unsigned char *date_time, const char *fmt, ...);



#endif /* TCC_H */
