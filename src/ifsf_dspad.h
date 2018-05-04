/*******************************************************************
1.枚举类型FP_STATE定义了加油点的9种状态,FP_TR_STATE定义了交易数据的3种状态.
2.枚举类型FP_AD,FP_XXX_AD定义了IFSF的基地址.
-------------------------------------------------------------------
2007-07-14 by chenfm
*******************************************************************/
#ifndef __IFSF_FPAD_H__
#define __IFSF_FPAD_H__

#include "ifsf_def.h"

//All key indexes are defined in ifsf_def.h

/*--------------------------------------------------------------*/
//重要数据结构--------------
//各种状态:
enum FP_STATE{
	FP_State_Inoperative=1, FP_State_Closed, FP_State_Idle, FP_State_Calling, FP_State_Authorised,
	FP_State_Started, FP_State_Suspended_Started, FP_State_Fuelling, FP_State_Suspended_Fuelling
	}; //gFp_state;
enum FP_TR_STATE{FP_Cleared_Transaction = 1, FP_Payable_Transaction, FP_Locked_Transaction
	}; //gFp_tr_state;


//各基地址,见IFSF 3-01,59-60
enum FP_AD{ //主地址
	FP_AD_COM_SV=0,FP_AD_C_DAT, 
	FP_AD_FP_ID=0x21,FP_AD_FP_ID2,FP_AD_FP_ID3,FP_AD_FP_ID4,
	FP_AD_PR_ID=0x41,FP_AD_PR_ID2,FP_AD_PR_ID3,FP_AD_PR_ID4,FP_AD_PR_ID5,
	FP_AD_PR_ID6,FP_AD_PR_ID7,FP_AD_PR_ID8,
	FP_AD_PR_DAT=0x61,
	FP_AD_M_ID=0x81,FP_AD_M_ID2,FP_AD_M_ID3,FP_AD_M_ID4,FP_AD_M_ID5,FP_AD_M_ID6,
	FP_AD_M_ID7,FP_AD_M_ID8,FP_AD_M_ID9,FP_AD_M_ID10,FP_AD_M_ID11,FP_AD_M_ID12,
	FP_AD_M_ID13,FP_AD_M_ID14,FP_AD_M_ID15,FP_AD_M_ID16,
	FP_AD_SW_DAT=0xa1 //新版IFSF已经不用.
	};// gFp_ad;	
enum FP_LN_ID_AD{ //逻辑油枪地址. ?目前待协议转换加油机的每个加油点有2个物理油枪,2个逻辑油枪?
	FP_AD_LN_ID=0x11,FP_AD_LN_ID2,FP_AD_LN_ID3,FP_AD_LN_ID4,
	FP_AD_LN_ID5,FP_AD_LN_ID6,FP_AD_LN_ID7,FP_AD_LN_ID8
	};// gFp_ln_id_ad; 
/*错误地址,暂不定义
enum FP_ER_ID_AD{FP_AD_ER_ID=1,FP_AD_ER_ID2,FP_AD_ER_ID3,FP_AD_ER_ID4,FP_AD_ER_ID5,FP_AD_ER_ID6
	FP_AD_ER_ID7,FP_AD_ER_ID8,FP_AD_ER_ID9,FP_AD_ER_ID10,FP_AD_ER_ID11,FP_AD_ER_ID12,FP_AD_ER_ID13
	FP_AD_ER_ID14,FP_AD_ER_ID15,FP_AD_ER_ID16,FP_AD_ER_ID17,FP_AD_ER_ID18,FP_AD_ER_ID19,
	FP_AD_ER_ID20,FP_AD_ER_ID21,FP_AD_ER_ID22,FP_AD_ER_ID23,FP_AD_ER_ID24,FP_AD_ER_ID25,
	FP_AD_ER_ID26,FP_AD_ER_ID27,FP_AD_ER_ID28,FP_AD_ER_ID29,FP_AD_ER_ID30,FP_AD_ER_ID31,
	FP_AD_ER_ID32,FP_AD_ER_ID33,FP_AD_ER_ID34,FP_AD_ER_ID35,FP_AD_ER_ID36,FP_AD_ER_ID37,
	FP_AD_ER_ID38,FP_AD_ER_ID39,FP_AD_ER_ID40,FP_AD_ER_ID41,FP_AD_ER_ID42,FP_AD_ER_ID43,
	FP_AD_ER_ID44,FP_AD_ER_ID45,FP_AD_ER_ID46,FP_AD_ER_ID47,FP_AD_ER_ID48,FP_AD_ER_ID49,
	FP_AD_ER_ID50,FP_AD_ER_ID51,FP_AD_ER_ID52,FP_AD_ER_ID53,FP_AD_ER_ID54,FP_AD_ER_ID55,
	FP_AD_ER_ID56,FP_AD_ER_ID57,FP_AD_ER_ID58,FP_AD_ER_ID59,FP_AD_ER_ID60,FP_AD_ER_ID61,
	FP_AD_ER_ID62,FP_AD_ER_ID63,FP_AD_ER_ID64}gFp_er_id_ad;
*/
enum FP_FM_ID_AD{ //加油模式地址
	FP_AD_FM_ID=0x11,FP_AD_FM_ID2,FP_AD_FM_ID3,FP_AD_FM_ID4,
	FP_AD_FM_ID5,FP_AD_FM_ID6,FP_AD_FM_ID7,FP_AD_FM_ID8
	};//gFp_fm_id_ad; //本PCD设备只用一种加油模式.
/*--------------------------------------------------------------*/
#endif
