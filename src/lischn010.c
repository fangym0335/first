/*
 * Filename: lischn010.c
 *
 * Description: ¼ÓÓÍ»úĞ­Òé×ª»»Ä£¿é -- ÀÏ³¤¿Õ·Ç¿¨»ú  4 Line CL, 4800,8,e,1
 *
 * ²Î¿¼ÎÄµµ<< CK1313Î¢»ú¿ØÖÆºÍ¼ÆÁ¿¼ÓÓÍ»úÍ¨ĞÅ½Ó¿Ú¶¨Òå>>
 *
 * 2008.04 by Yongsheng Li <Liys@css-intelligent.com>
 * 
 *
 *  History:   
 *
 *  Note: ´ËÄ£¿é´úÂëÓëlischn010Ò»ÖÂ, ±¦µÂ¶ûºÍÀÏ³¤¿Õ·Ç¿¨»úĞ­ÒéÒÔ¼°ÊµÏÖ¾ùÒ»ÖÂ;
 *        ÈôÒªµ÷ÕûÁ½¸öÄ£¿é(lischn006 & lischn010)ĞèÍ¬²½µ÷Õû. Guojie zhu @ 2009.7.18
 *
 */

#include "pub.h"
#include "doshm.h"
#include "tcp.h"
#include "dosem.h"
#include "domsgq.h"
#include "commipc.h"
#include "tty.h"
#include "ifsf_def.h"
#include "ifsf_pub.h"
#include "ifsf_dspad.h"

#define GUN_EXTEND_LEN  4

#define PUMP_OFF	0x80 //0x00
#define PUMP_CALL	0x00 //0x40
#define PUMP_BUSY	0x20
#define PUMP_AUTH       0xa0

#define TRY_TIME	5
#define INTERVAL	150000 // 150ms

static int DoCmd();
static void DoPolling();
static int DoAuth(int pos);
static int DoStarted(int i);
static int DoBusy(int pos);
static int DoCall(int pos);
static int DoEot(int pos);
static int DoOpen(int pos, __u8 *msgbuffer);
static int DoClose(int pos, __u8 *msgbuffer);
static int DoRation(int pos);
static int DoStop(int pos);
static int DoMode();
static int DoPrice();
static int ChangePrice();
static int GetTransData(int pos);
static int GetPumpStatus(int pos, __u8 *status);
static int GetPumpOnline(int pos, __u8 *status);
static int GetPumpTotal(int pos);
static int GetPrice(int pos);
static int InitPumpTotal();
static int InitCurrentPrice();
static int InitGunLog();
static int GetCurrentLNZ(int pos);
static int SetPort010();
static int ReDoSetPort010();
static int SetCtrlMode(int pos);
static int keep_in_touch_xs(int x_seconds);
static int CheckPriceErr(int pos);

static PUMP pump;
static int node;
static int IFSFpos;
static DISPENSER_DATA *lpDispenser_data;
static NODE_CHANNEL *lpNode_channel;
static int fp_state[MAX_FP_CNT];              // 0 - IDLE, 1 - CALLING , 2 - FUELLING
static int dostop_flag[MAX_FP_CNT] = {0};     // ±ê¼ÇÊÇ·ñ³É¹¦×ö¹ıÍ£Ö¹¼ÓÓÍ²Ù×÷
static int control_flag_valid = 0;            // ±ê¼Ç¸ÃÓÍ»ú×´Ì¬×ÖÖĞµÄ ÊÜ¿Ø/×Ô¿Ø ±ê¼ÇÎ»ÊÇ·ñÓĞĞ§
static int reset_ctrl_flag[MAX_FP_CNT] = {0}; // ±ê¼ÇÊÇ·ñĞèÒªÖØÉèÖ÷¿Ø

int ListenChn010(int chnLog)
{
	long	indtyp;
	int	ret;
	int	fd;
	int	j;
	int	i;
	__u8    cmd[8];
	__u8    status;
	
	sleep(5);   //·ÀÖ¹´Ë½ø³ÌÍË³ö,¶øÖ÷½ø³ÌÃ»ÓĞÆô¶¯Íê±Ï. Ã¿¸ölischnxxx¶¼ĞèÒª
	RunLog("Listen Channel  010 ,logic channel[%d]", chnLog);
	memset(&pump, 0, sizeof(pump));
	pump.chnLog = chnLog;
	pump.panelCnt = 0;



	//1.   É¨Ãè¹²ÏíÄÚ´æ, ¼ì²é¸ÃÍ¨µÀÁ¬½ÓµÄ´®¿Ú, Ãæ°å		
	for (i = 0; i < MAX_CHANNEL_CNT; i++) {
		if (chnLog == gpIfsf_shm->node_channel[i].chnLog) {	// Âß¼­Ãæ°åºÅÏàÍ¬
			IFSFpos = i;
			lpNode_channel = (NODE_CHANNEL *)(&(gpIfsf_shm->node_channel[i]));
			// node_channelºÍdispenser_data±ØĞëÒ»Ò»¶ÔÓ¦
			lpDispenser_data = (DISPENSER_DATA *)(&gpIfsf_shm->dispenser_data[i]);
			node = lpNode_channel->node;
			pump.port =lpNode_channel->serialPort;
			pump.chnPhy = lpNode_channel->chnPhy;
			pump.panelCnt = lpNode_channel->cntOfFps;

			int idx;			
			for (j = 0; j < pump.panelCnt; j++) {
				// initial panelPhy - addr of fp for communication 
				for (idx = 0; idx < gpIfsf_shm->gunCnt; idx++) {
					if ((gpIfsf_shm->gun_config[idx].node == node) &&
						(gpIfsf_shm->gun_config[idx].isused == 1) &&
						(gpIfsf_shm->gun_config[idx].logfp == j + 1)) {
						// pump.panelPhy[] == 0xA* or 0xB*
						pump.panelPhy[j] = 0x90 +  \
							((gpIfsf_shm->gun_config[idx].phyfp) * 16);
						break;
					}
				}
				pump.gunCnt[j] =  gpIfsf_shm->node_channel[i].cntOfFpNoz[j];
			}
				
			break;
		}
	}
	pump.indtyp = chnLog;
	bzero(&pump.newPrice, FP_PR_NB_CNT * 4);
	
	//2.´ò¿ª´®¿Ú(Á¬½ÓÔÚ´®¿ÚÉÏµÄÀ©Õ¹¿Ú)£¬ÉèÖÃ´®¿ÚÊôĞÔ
	
	strcpy(pump.cmd, alltty[pump.port-1]);
	pump.fd = OpenSerial(pump.cmd);
	if (pump.fd < 0) {
		RunLog("´ò¿ª´®¿Ú[%s]Ê§°Ü", pump.cmd);
		return -1;
	}

	//RunLog("´ò¿ª´®¿Ú[%s].fd=[%d]", pump.cmd, pump.fd);

	ret = SetPort010();
	if (ret < 0) {
		return -1;
	}
	

	//3.  ²âÊÔ¼ÓÓÍ»úÊÇ·ñÔÚÏß,Èç¹û²»ÔÚÏßÔòµÈ´ı
	while (1) {		
		ret = GetPumpOnline(0, &status);
		if (ret == 0) {
			//ÉèÖÃ¼ÓÓÍ»úµÄÖ÷¿ØÄ£Ê½	
			for (j = 0; j < pump.panelCnt; ++j) {		
				if (SetCtrlMode(j) != 0) {						
					continue;
				}
			}
	  		
			break;
		}

		gpIfsf_shm->node_online[IFSFpos] = 0; //²»ÔÚÏß,²»²úÉúĞÄÌø.
		UserLog("½Úµã %d ²»ÔÚÏß", node);
		
		sleep(1);
	}

	// 3.5  
	ret = GetPumpStatus(0, &status);
	if (ret < 0) {
		return -1;
	} else {
		// ÓÍ»úÊÜ¿Ø±ê¼ÇÎ»ÊÇ·ñÓĞĞ§
		control_flag_valid = (status & 0x08);
		RunLog("Node[%d]ÊÜ¿Ø±êÖ¾Î»ÊÇ·ñÓĞĞ§±ê¼Ç%02x", node, control_flag_valid);
	}

	//4.³õÊ¼»¯Ä¬ÈÏµ±Ç°Âß¼­Ç¹ºÅ
	ret = InitGunLog();
	if (ret < 0) {
		RunLog("³õÊ¼»¯Ä¬ÈÏµ±Ç°Âß¼­Ç¹ºÅÊ§°Ü");
		return -1;
	}

	 //5.³õÊ¼»¯ÓÍÆ·µ¥¼Ûµ½Êı¾İ¿â
	ret = InitCurrentPrice();
	if (ret < 0) {
		RunLog("³õÊ¼»¯ÓÍÆ·µ¥¼Ûµ½Êı¾İ¿âÊ§°Ü");
		return -1;
	}

	//6.³õÊ¼»¯±ÃÂëµ½Êı¾İ¿â
	ret = InitPumpTotal();
	if (ret < 0) {
		RunLog("³õÊ¼»¯±ÃÂëµ½Êı¾İ¿âÊ§°Ü");
		return -1;
	}


       //+++++++++++++++ 7. Ò»ÇĞ¾ÍĞ÷, ÉÏÏß+++++++++++++++++++++++//
       
//	gpIfsf_shm->node_online[IFSFpos] = node;  
	ChangeNode_Status(node, NODE_ONLINE);
	UserLog("½Úµã %d ÔÚÏß,×´Ì¬[%02x]!", node, status);
	keep_in_touch_xs(12);               // È·±£CDÒÑ¾­ÊÕµ½ĞÄÌøÖ®ºóÔÙ×öºóĞø²Ù×÷

	//8. ÓÍ»úÄ£¿é³õÊ¼»¯Íê±Ï,×ª»»FP×´Ì¬Îª Close 
	// when dispenser is initialized , the status is Closed. it's waiting for set command from CD. 
	for (i = 0; i < lpNode_channel->cntOfFps; i++) {
		if ((__u8)FP_State_Closed != lpDispenser_data->fp_data[i].FP_State) {
			ret = ChangeFP_State(node, 0x21 + i, FP_State_Closed, NULL);
		}
	}

	while (1) {
		if (gpGunShm->sysStat == RESTART_PROC ||
			gpGunShm->sysStat == STOP_PROC) {
			RunLog("ÏµÍ³ÍË³ö");
			exit(0);
		}

		while (gpGunShm->sysStat == PAUSE_PROC) {
			__u8 status;
			GetPumpStatus(0, &status);              // Note. ÓëÓÍ»ú±£³ÖÁªÏµ
			Ssleep(INTERVAL);
			RunLog("ÏµÍ³ÔİÍ£");
		}

		indtyp = pump.indtyp;

		// Ê×ÏÈ¼ì²éÊÇ·ñÓĞÃüÁî
		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		ret = -1;

		if (gpIfsf_shm->auth_flag - FCC_SWITCH_AUTH != 0) {  // ºóÌ¨ÔÚÏß
			// ²é¿´ÊÇ·ñÓĞ×Ô¼ºµÄÏûÏ¢
			ret = RecvMsg(ORD_MSG_TRAN, pump.cmd, &pump.cmdlen, &indtyp, TIME_FOR_RECV_CMD); 
			if (ret == 0) {
				ret = DoCmd();
				if (ret != 0) {
					RunLog("Node %d ´¦ÀíºóÌ¨ÃüÁîÊ§°Ü", node);
				}
			}

			DoPolling();
//			Ssleep(100000);       // Õı³£ÂÖÑ¯¼ä¸ôÎª100ms + INTERVAL
		} else {
			Ssleep(200000);
			DoPolling();
		}
	}

}

/*
 * func: Ö´ĞĞÉÏÎ»»úÃüÁî
 */
static int DoCmd()
{
	int ret;
	int fp_id;
	int pos;    //pos is No. of  fuelling poit(panel) of a dispenser: 0, 1, 2, 3
	__u8 msgbuffer[48];


	if (pump.cmd[0] != 0x2F) {
		// ÌáÈ¡µ÷ÓÃÃüÁîÖ´ĞĞº¯ÊıÇ°µÄ¹²ÓĞ²Ù×÷
		fp_id = pump.cmd[2];
		if (fp_id == 0x00) {  // fp_id == 0x00 ±íÊ¾¶ÔËùÓĞfp²Ù×÷, ¶ÔÓ¦ pos ÖÃÎª 255
			pos = 255;
		} else if ((fp_id >= 0x21) && (fp_id <= 0x24)) {
			pos = fp_id - 0x21;
		} else {
			if (pump.cmd[0] != 0x18 && pump.cmd[0] != 0x19) {  // DoPrice & DoMode²»ĞèÒª´¦ÀíÃæ°åºÅ
				RunLog("½Úµã[%d] ·Ç·¨µÄÃæ°åµØÖ·",node);
				return -1;
			}
		}

		bzero(msgbuffer, sizeof(msgbuffer));
		memcpy(msgbuffer, &pump.cmd[3], pump.cmd[4] + 2);  // ±¾´Î²Ù×÷µÄ Acknowledge Message
	}

	ret = -1;
	switch (pump.cmd[0]) {
		case 0x18:	// ¸ü¸ÄÓÍ¼Û
			ret = DoPrice();		
			break;		
		case 0x19:	// ¸ü¸Ä¿ØÖÆÄ£Ê½
//			ret = DoMode();	      // Unsupported
			break;
		case 0x1C:	// Terminate_FP
			ret = HandleStateErr(node, pos, Terminate_FP,
				lpDispenser_data->fp_data[pos].FP_State, msgbuffer); 
			if (ret < 0) {       // ·¢Éú´íÎó£¬µ±Ç°×´Ì¬²»ÔÊĞí×ö¸Ã²Ù×÷
				break;
			}

			if ((__u8)FP_State_Calling == lpDispenser_data->fp_data[pos].FP_State) {
				ChangeFP_State(node, pos + 0x21, FP_State_Idle, msgbuffer);
				ChangeFP_State(node, pos + 0x21, FP_State_Calling, NULL);
				break;
				/*
				 * ÔÚCallingµÄ×´Ì¬ÏÂÖ´ĞĞ´Ë²Ù×÷, °´ÕÕĞ­Òé·µ»ØACK,²¢ÇÒ¸Ä±ä×´Ì¬ÎªIdle
				 * Ö®ºóÔÙPollingµÄÊ±ºò¸ù¾İÓÍ»ú×´Ì¬×ª±äFP_StateÎªCalling,
				 * ±ÜÃâÔÚÌ§Ç¹×´Ì¬ÏÂ·¢Ô¤ÖÃÒı·¢×´Ì¬×ª»»»ìÂÒ
				 * Guojie Zhu @ 2009.03.13
				 */
			}

			ret = DoStop(pos);
			if (ret == 0) {
				ret = ChangeFP_State(node, pos + 0x21, FP_State_Idle, msgbuffer);
				dostop_flag[pos] = 1;
			} else {
				SendCmdAck(NACK, msgbuffer);
				ChangeFP_State(node, pos + 0x21,
					lpDispenser_data->fp_data[pos].FP_State, NULL);
			}

			break;
		case 0x1D:	// Release_FP
			ret = HandleStateErr(node, pos, Do_Auth,
				lpDispenser_data->fp_data[pos].FP_State, msgbuffer); 
			if (ret < 0) {
				break;
			}
			ret = DoAuth(pos);				
			if (ret == 0) {       // exec success, change fp state
				ret = ChangeFP_State(node, pos + 0x21, FP_State_Started, msgbuffer);
			} else {
				SendCmdAck(NACK, msgbuffer);
				ChangeFP_State(node, pos + 0x21,
					lpDispenser_data->fp_data[pos].FP_State, NULL);
			}

			break;
		case 0x3D:      // Preset  amount or volume
		case 0x4D:
			ret = HandleStateErr(node, pos, Do_Ration,
				lpDispenser_data->fp_data[pos].FP_State, msgbuffer); 
			if (ret < 0) {
				lpDispenser_data->fp_data[pos].Log_Noz_Mask = 0xFF;
				bzero(&lpDispenser_data->fp_data[pos].Remote_Volume_Preset, 5);
				bzero(&lpDispenser_data->fp_data[pos].Remote_Amount_Prepay, 5);
				break;
			}
			ret = DoRation(pos);
			if (ret == 0) {       // exec success, change fp state
				if ((__u8)FP_State_Calling == lpDispenser_data->fp_data[pos].FP_State) {
					ret = SendData2CD(&msgbuffer[2], msgbuffer[1], 5);
					if (ret < 0) {
						RunLog("Send Ack Msg[%s] to CD failed",
							Asc2Hex(&msgbuffer[2], msgbuffer[1]));
					}
				} else {
					ret = ChangeFP_State(node, pos + 0x21, FP_State_Authorised, msgbuffer);
				}
			} else {
				SendCmdAck(NACK, msgbuffer);
				ChangeFP_State(node, pos + 0x21,
					lpDispenser_data->fp_data[pos].FP_State, NULL);
			}
			bzero(&lpDispenser_data->fp_data[pos].Remote_Amount_Prepay, 5);
			bzero(&lpDispenser_data->fp_data[pos].Remote_Volume_Preset, 5);

			break;	
		case 0x1E:     // Open_FP
			ret = HandleStateErr(node, pos, Open_FP,
				lpDispenser_data->fp_data[pos].FP_State, msgbuffer); 
			if (ret < 0) {
				break;
			}
			ret = DoOpen(pos, msgbuffer);
			break;
		case 0x1F:     // Close_FP		      
			ret = HandleStateErr(node, pos, Close_FP,
				lpDispenser_data->fp_data[pos].FP_State, msgbuffer); 
			if (ret < 0) {
				break;
			}
			ret = DoClose(pos, msgbuffer);
			break;	
		case 0x2F:
			ret = upload_offline_tr(node, &pump.cmd[3]);
			break;
		default:       // Unknow
			RunLog("½ÓÊÜµ½Î´¶¨ÒåÃüÁî[0x%02x]", pump.cmd[0]);
			break;
	}

	return ret;
}


/*
 * func: Open_FP
 */
static int DoOpen(int pos, __u8 *msgbuffer) {
	int ret;
	unsigned int i;
      
	ret = SetHbStatus(node, 0);
	if (ret<0) {
		RunLog("ĞÄÌø×´Ì¬¸Ä±ä´íÎó");
		SendCmdAck(NACK, msgbuffer);
		return -1;
	}

	if (pos == 255) {
		SendCmdAck(ACK, msgbuffer);
		for (i = 0; i < pump.panelCnt; i++) {
			ret = ChangeFP_State(node, 0x21 + i, FP_State_Idle, NULL);
		}
	} else {
		ret = ChangeFP_State(node, pos + 0x21, FP_State_Idle, msgbuffer);
	}

	return ret;
}

/*
 * func: Close_FP
 */
static int DoClose(int pos, __u8 *msgbuffer) 
{
	int i, ret;

	if (pos == 255) {
		SendCmdAck(ACK, msgbuffer);
		for (i = 0; i < pump.panelCnt; i++) {			
			ret = ChangeFP_State(node, 0x21 + i, FP_State_Closed, NULL);
		}
	} else {
		ret = ChangeFP_State(node, pos + 0x21, FP_State_Closed, msgbuffer);
	}
	
	return ret;
}

static int DoStarted(int i)
{
	int ret;

	if (lpDispenser_data->fp_data[i].Log_Noz_State == 0) {
		ret = GetCurrentLNZ(i);
		if (ret < 0) {
			return -1;
		}
	}

	ret = BeginTr4Fp(node, i + 0x21);
	if (ret < 0) {
		ret = BeginTr4Fp(node, i + 0x21);
		if (ret < 0) {
			RunLog("½Úµã[%d]Ãæ°å[%d],BeingTr4FP Failed", node, i + 1);
			return -1;
		}
	}

	// GetCurrentLNZ & BeginTr4Fp ³É¹¦, ÔòÖÃÎ» fp_state[i] Îª 2
	fp_state[i] = 2;

	return 0;
}

// Only full mode? cann't be changed.
static int DoMode() {
	return -1;
}

/*
 * func: PreSet/PrePay & Release_FP
 */
static int DoRation(int pos)  
{
	int kind;
	int ret;
	int trynum;
	__u8 amount[6];
	

	if (pump.cmd[0] == 0x3D) {    // kind = 0xB9 ¶¨Á¿; kind = 0xB5 ¶¨¶î
		kind = 0xB9;
	} else {
		kind = 0xB5;
	}
	
	if (kind == 0xB9) {            //¶¨Á¿
		amount[0] = lpDispenser_data->fp_data[pos].Remote_Volume_Preset[2];	
		amount[1] = lpDispenser_data->fp_data[pos].Remote_Volume_Preset[3];
		amount[2] = lpDispenser_data->fp_data[pos].Remote_Volume_Preset[4];			      	
	} else if (kind == 0xB5) {     //¶¨¶î
		amount[0] = lpDispenser_data->fp_data[pos].Remote_Amount_Prepay[2];	
		amount[1] = lpDispenser_data->fp_data[pos].Remote_Amount_Prepay[3];
		amount[2] = lpDispenser_data->fp_data[pos].Remote_Amount_Prepay[4];	  	
	}

	memset(pump.cmd, 0, sizeof(pump.cmd));
	pump.cmdlen = 0;

	for (trynum = 0; trynum < TRY_TIME; trynum++) {		
		Ssleep(INTERVAL);
		pump.cmd[0] = 0xf5;
		pump.cmd[1] = 0x05 | pump.panelPhy[pos];
		
		memcpy(pump.cmd + 2, amount, 3);

		pump.cmd[5] = kind;
		pump.cmd[6] = Check_xor_bdr(&pump.cmd[0], 6);							
		pump.cmdlen = 7;
		pump.wantLen = 4;

		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy,
			       	pump.cmd, pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]ÏÂ·¢Ô¤ÖÃÃüÁîÊ§°Ü", node, pos + 1);
			continue;
		}

		RunLog("Node[%d].FP[%d]ÏÂ·¢Ô¤ÖÃÃüÁî[%s]³É¹¦", 
			node, pos + 1, Asc2Hex(pump.cmd, pump.cmdlen));

		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]ÏÂ·¢Ô¤ÖÃÃüÁî³É¹¦, È¡·µ»ØĞÅÏ¢Ê§°Ü", node, pos + 1);
			continue;
		} else if (pump.cmd[2] != 0xB9 && pump.cmd[2] != 0xB5) {
			RunLog("Node[%d].FP[%d]Ô¤ÖÃÊ§°Ü(ÒÑÓĞÔ¤ÖÃ/¼ÓÓÍÖĞ/Ô¤ÖÃĞ¡ÓÚ×îĞ¡Á¿)", node, pos + 1);
			continue;
		}

		if (0xF5 != pump.cmd[0]) {
			RunLog("Node[%d].FP[%d]·µ»ØÊı¾İ´íÎó", node, pos + 1);
			continue;
		}

		if (Check_xor_bdr(&pump.cmd[0], pump.wantLen - 1) !=
						pump.cmd[pump.wantLen - 1]) {
			RunLog("Node[%d].FP[%d]·µ»ØÊı¾İĞ£Ñé´íÎó", node, pos + 1);
			continue;
		}

		ret = DoAuth(pos);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d], Ô¤ÖÃ¼ÓÓÍÊÚÈ¨Ê§°Ü!", node, pos + 1);
			continue;				
		}

		return 0;
	}

	return -1;
}

/*
 * func: ½ÓÊÕÓÍÆ·µ¥¼Û, ´æÈënewPrice[][], ÔÚIDLE×´Ì¬Ö´ĞĞ±ä¼Û
 */
static int DoPrice()
{
	__u8 pr_no;
	
	pr_no  = pump.cmd[2] - 0x41; //ÓÍÆ·µØÖ·µ½ÓÍÆ·±àºÅ0,1,...7.
	if ((0x04 != pump.cmd[8]) || (0x00 != pump.cmd[9]) || pr_no > 7) {
		RunLog("±ä¼ÛĞÅÏ¢´íÎó,²»ÄÜÏÂ·¢");
		return -1;
	}
	
	if ((pump.newPrice[pr_no][2] | pump.newPrice[pr_no][3]) == 0) {
		// ÉÏ´ÎÖ´ĞĞ±ä¼Û(ChangePrice)ºóµÚÒ»´ÎÊÕµ½WriteFM·¢À´µÄ±ä¼ÛÃüÁî
		pump.newPrice[pr_no][0] = 1;
	} else {
		pump.newPrice[pr_no][0]++;
	}

	// ²»¹ÜÊÇ·ñÊÇÁ¬ĞøÊÕµ½Í¬ÖÖÓÍÆ·µÄ±ä¼ÛÃüÁî, Ò²²»¹Üµ¥¼ÛÊÇ·ñÒ»ÖÂ, ¾ùÊ¹ÓÃĞÂµ¥¼Û¸²¸ÇÀÏµ¥¼Û
	pump.newPrice[pr_no][2] = pump.cmd[10];
	pump.newPrice[pr_no][3] = pump.cmd[11];
#ifdef DEBUG
	RunLog("Recv New Price-PR_ID[%d]TIMES[%d] %02x.%02x",
		pr_no + 1, pump.newPrice[pr_no][0], pump.newPrice[pr_no][2], pump.newPrice[pr_no][3]);
#endif

	return 0;
}


/*
 * func: Ö´ĞĞ±ä¼Û, Ê§°Ü¸Ä±äFP×´Ì¬ÎªINOPERATIVE, ²¢Ğ´´íÎóÊı¾İ¿â
 */
static int ChangePrice() 
{
	int i, j;
	int ret, fm_idx;
	int trynum;
	__u8 pr_no, status;
	__u8 err_code;
	__u8 err_code_tmp;

	for (pr_no = 0; pr_no < FP_PR_NB_CNT; pr_no++) {    
              //Ò»Ì¨¼ÓÓÍ»úÖ»ÓĞÒ»¸öÓÍÆ·Ò²Ö»ÓĞÒ»¸öÓÍ¼Û
		if (pump.newPrice[pr_no][2] != 0x00 || pump.newPrice[pr_no][3] != 0x00) {	
			for (i = 0; i < pump.panelCnt; i++) {
				err_code = 0x99;
				for (j = 0; j < MAX_FP_LN_CNT; j++) { // ²»ÄÜÓÃgunCnt,1Ç¹¿ÉÄÜ²»ÓÃ
#if DEBUG
					RunLog(">>> fp_ln_data[%d][%d].PR_Id: %d", i, j,
							lpDispenser_data->fp_ln_data[i][j].PR_Id);
#endif
					if (lpDispenser_data->fp_ln_data[i][j].PR_Id - 1 == pr_no) {
					err_code_tmp = 0x99;
				CriticalLog("ÏÂ·¢±ä¼ÛÃüÁî¸ø½Úµã[%02d]Ãæ[%d]Ç¹[%d]......", node, i + 1, j + 1);
					
					for (trynum = 0; trynum < TRY_TIME; trynum++) {
						pump.cmd[0] = 0xf5;
						pump.cmd[1] = 0x04 | pump.panelPhy[i];

						//ÓÍ¼Û
						pump.cmd[2] = pump.newPrice[pr_no][2];
						pump.cmd[3] = pump.newPrice[pr_no][3]; 					
						pump.cmd[4] = 0xb2;
						pump.cmd[5] = Check_xor_bdr(&pump.cmd[0], 5);

						pump.cmdlen = 6;
						pump.wantLen = 5;

						Ssleep(INTERVAL);
						ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
								pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
						if (ret < 0) {
							RunLog("Node[%d].FP[%d]ÏÂ·¢±ä¼ÛÃüÁîÊ§°Ü", node, i + 1);
							continue;
						}

						// ±ä¼Û³É¹¦£¬ĞÂµ¥¼ÛĞ´ÈëÊı¾İ¿â & ÇåÁã»º´æ

						RunLog("Node[%d].FP[%d].PriceChange_CMD: %s",
							node, i + 1, Asc2Hex(pump.cmd, pump.cmdlen));

						memset(pump.cmd, 0, sizeof(pump.cmd));
						pump.cmdlen = sizeof(pump.cmd) - 1;

						ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
						if (ret < 0) {
							RunLog("Node[%d].FP[%d]ÏÂ·¢±ä¼ÛÃüÁî³É¹¦,È¡·µ»ØĞÅÏ¢Ê§°Ü",
												node, i + 1);
							continue;
						}

						if (Check_xor_bdr(&pump.cmd[0], pump.wantLen - 1) !=
										pump.cmd[pump.wantLen - 1]) {
							RunLog("Node[%d].FP[%d]·µ»ØÊı¾İĞ£Ñé´íÎó", node, i + 1);
							Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL);
							continue;
						}

						if (pump.cmd[2] != 0x59 || pump.cmd[0] != 0xF5) {
							RunLog("Node[%d].FP[%d]Ö´ĞĞ±ä¼ÛÃüÁîÊ§°Ü", node, i + 1);
							err_code_tmp = 0x37;
							Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL);
							continue;
						}
						
						ret = GetPrice(i);	
						if (ret < 0) {
							continue;
						}

						if (memcmp(&pump.cmd[2], &pump.newPrice[pr_no][2], 2) != 0) {
							RunLog("Node[%d].FP[%d]Ö´ĞĞ±ä¼ÛÃüÁîÊ§°Ü", node, i + 1);
							err_code_tmp = 0x37;     // Ö´ĞĞÊ§°Ü
							Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL);
							continue;
						}

						fm_idx = lpDispenser_data->fp_data[i].Default_Fuelling_Mode - 1;

						RunLog("Node[%d]·¢ËÍ±ä¼ÛÃüÁî³É¹¦,ĞÂµ¥¼Ûfp_fm_data[%d][%d].Prode_Price: %s",
							node, pr_no, fm_idx,
							Asc2Hex(lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price, 4));

						CriticalLog("½Úµã[%02d]Ãæ[%d]Ç¹[%d]±ä¼Û³É¹¦,ĞÂµ¥¼ÛÎª%02x.%02x",
							node, i + 1, j + 1,
							lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[2],
							lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[3]);

						err_code_tmp = 0x99;
						break;

					}

//					trynum = TRY_TIME; for test Error
					if (trynum >= TRY_TIME) {
						// Write Error Code: 0x36/0x37 - Price Change Error (Spare)
						if (0x37 != err_code_tmp) {
							err_code_tmp = 0x36;    // Í¨Ñ¶Ê§°Ü
				CriticalLog("½Úµã[%02d]Ãæ[%d]Ç¹[%d]±ä¼ÛÊ§°Ü[Í¨Ñ¶ÎÊÌâ],¸Ä±äÃæ×´Ì¬Îª[²»¿ÉÓÃ]",
											node, i + 1, j + 1);
						} else {
				CriticalLog("½Úµã[%02d]Ãæ[%d]Ç¹[%d]±ä¼ÛÊ§°Ü[ÓÍ»úÖ´ĞĞÊ§°Ü],¸Ä±äÃæ×´Ì¬Îª[²»¿ÉÓÃ]",
											node, i + 1, j + 1);
						}

						PriceChangeErrLog("%02x.%02x.%02x %02x.%02x %02x",
							node, i + 1, j + 1,
							pump.newPrice[pr_no][2], 
							pump.newPrice[pr_no][3], err_code);

					}
					if (0x99 == err_code) {
						// ¸ÃÃæÓĞÈÎºÎÓÍÇ¹±ä¼ÛÊ§°Ü, ÔòËãÕû¸öFP±ä¼ÛÊ§°Ü.
						err_code = err_code_tmp;
					}
					} // end of if.PR_Id match
				//	break;  // Èç¹û¼ÓÓÍ»úÄÜ¹»¸ù¾İÖ¸ÁîĞÅÏ¢´¦Àí¶àÇ¹Í¬ÓÍÆ·µÄÇé¿ö£¬ÄÇÕâÀïĞèÒªbreak.
				} // end of for.j < LN_CNT
				RunLog("---------- Node[%d]FP[%d] ER_DATA, error code: %02x -----", node, i + 1, err_code);

				int tmp_idx;
				for (tmp_idx = 0; tmp_idx < pump.newPrice[pr_no][0]; tmp_idx++) {
					ChangeER_DATA(node, 0x21 + i, err_code, NULL); // TranslaterÒÑ¾­»Ø¸´ÁËACK
				}

				if (err_code != 0x99) {
					ChangeFP_State(node, 0x21 + i, FP_State_Inoperative, NULL);
				}

				Ssleep(PRICE_CHANGE_RETRY_INTERVAL - INTERVAL);
			} // end of for.i < panelCnt

			bzero(&pump.newPrice[pr_no], 4);
		} // end of if.newPrice
	} // end of for.pr_no

	
       
	return 0;
}


/*
 * func: ¶ÔFP½øĞĞ×´Ì¬ÂÖÑ¯, ¸ù¾İ×´Ì¬×öÏàÓ¦´¦Àí
 */
static void DoPolling()
{
	int i,j;
	int ret;
	int tmp_lnz; // realy temp 3.27
	static int times = 0;
	static int fail_cnt = 0;  // ¼ÇÂ¼GetPumpStatus Ê§°Ü´ÎÊı, ÓÃÓÚÅĞ¶¨ÀëÏß
	__u8 status;
	
	for (i = 0; i < pump.panelCnt; i++) {
		ret = GetPumpStatus(i, &status);
		if (ret < 0) {
			if (gpIfsf_shm->node_online[IFSFpos] == node) {
				if ((++fail_cnt) < MAX_FAIL_CNT) {  // ¼ÓÓÍ»úÀëÏßÅĞ¶Ï
					/*
					 * 2008.12.29Ôö¼Ó´Ë²Ù×÷, ½â¾öÒòµçÑ¹²»ÎÈÔì³É
					 * µ¥Æ¬»ú¸´Î»ºó´®¿Ú¸´Î»×îÖÕµ¼ÖÂÍ¨Ñ¶²»Í¨µÄÎÊÌâ
					 */
					/*
					 * ÒòÖØÉè´®¿ÚÊôĞÔ»áµ¼ÖÂReadTTY½ÓÊÕµ½"·Ç·¨Êı¾İ", ¹ÊÔİÊ±ÒÆ³ı
					 * 2009.02.19
					if (fail_cnt >= (MAX_FAIL_CNT / 2)) {
						if (SetPort010() >= 0) {
							RunLog("½Úµã%dÖØÉè´®¿ÚÊôĞÔ³É¹¦", node);
						}
					}
					*/
					continue;
				}


				// Step.1
				memset(dostop_flag, 0, sizeof(dostop_flag));
				memset(fp_state, 0, sizeof(fp_state));

				// Step.2
//				gpIfsf_shm->node_online[IFSFpos] = 0;
				ChangeNode_Status(node, NODE_OFFLINE);
				pump.time_offline = (3 * gpIfsf_shm->fcc_com_sv_data.Heartbeat_Interval) + time(NULL);
				RunLog("====== Node %d ÀëÏß! =============================", node);

				// Step.3
				// ÓÍ»úÀëÏß, ÎŞĞèÉÏ´«×´Ì¬¸Ä±äĞÅÏ¢, ËùÒÔ²»Ê¹ÓÃChangeFP_State, Ö±½Ó²Ù×÷FP_State
				for (j = 0; j < pump.panelCnt; ++j) {
					lpDispenser_data->fp_data[j].FP_State = FP_State_Inoperative;
					lpDispenser_data->fp_data[j].Log_Noz_State = 0;
				}
			}

			// ±ÜÃâÀëÏßÆÚ¼äµôµç,ËùÒÔÀëÏßÆÚ¼äÒ²²»Ê±ÖØÉè´®¿ÚÊôĞ
			//ReDoSetPort010();
			continue;
		} else {
			if (gpIfsf_shm->node_online[IFSFpos] == 0) {
				if ((--fail_cnt) > 0) { // ÅÅ³ı¸ÉÈÅ/Ë«ÏßµçÁ÷»·Êı¾İ²É¼¯BugÔì³ÉµÄÓ°Ïì
					fail_cnt = 1;
					continue;
				} else if (time(NULL) < pump.time_offline) {
					RunLog("==================== wait for 30s ==============");
					continue;
				}

				// fail_cnt¸´Î»
				fail_cnt = 0;

				// Step.1
				for (j = 0; j < pump.panelCnt; ++j) {
					if (SetCtrlMode(j) != 0) {
						fail_cnt = 1;
						return;
					}
			      	}

				// Step.2
				ret = InitPumpTotal();    // ³õÊ¼»¯±ÃÂëµ½Êı¾İ¿â
				if (ret < 0) {
					RunLog("½Úµã[%d], ¸üĞÂ±ÃÂëµ½Êı¾İ¿âÊ§°Ü", node);
					fail_cnt = 1;
					continue;
				}

				// Step.3
				for (j = 0;  j < pump.panelCnt; ++j) {
					lpDispenser_data->fp_data[j].FP_State = FP_State_Closed;
				}

				// Step.4
//				gpIfsf_shm->node_online[IFSFpos] = node;
				ChangeNode_Status(node, NODE_ONLINE);
				RunLog("====== Node[%d]»Ö¸´ÁªÏß! =========================", node);
				keep_in_touch_xs(12);     // È·±£CDÒÑ¾­ÊÕµ½ĞÄÌøÖ®ºóÔÙ×öºóĞø²Ù×÷

				// Step.5                 // Ö÷¶¯ÉÏ´«µ±Ç°FP×´Ì¬
				for (j = 0; j < pump.panelCnt; ++j) {
					ChangeFP_State(node, j + 0x21, FP_State_Closed, NULL);
				}
				continue;                 // Note!
			}

			/*
			 * ³¬¹ı1´Î¾ÍÖØĞÂÉèÖÃÒ»ÏÂÖ÷¿ØÄ£Ê½, ·½Ê½ÓÍ»ú×ªÎª×ÔÖ÷¼ÓÓÍÄ£Ê½
			 * status & 0x08 ×´Ì¬Î»²»ÊÇÔÚËùÓĞ±¦µÂ¶ûÓÍ»úÉÏ¶¼ÓĞĞ§,
			 * ËùÒÔ¸ù¾İÊ§°Ü´ÎÊıÉèÖÃµÄ´¦ÀíÈÔĞè±£Áô
			 */
			if ((control_flag_valid != 0 && (status & 0x08) == 0) || fail_cnt >= 2 ||
				 ((status & 0xa0) == PUMP_BUSY) && 
				 (lpDispenser_data->fp_data[i].FP_State <= (__u8)FP_State_Calling) ) {

				// Á½¸öÃæ¶¼ÖØÉèÒ»ÏÂ
				reset_ctrl_flag[0] = 0xFF;
				reset_ctrl_flag[1] = 0xFF;
				fail_cnt = 0;
				RunLog("Node%dĞèÒªÖØĞÂÉèÖÃÖ÷¿Ø", node);
			}

			if ((reset_ctrl_flag[i] != 0) && ((status & 0xa0) != PUMP_BUSY)) {
					 // ±ÜÃâ¼ÓÓÍÖĞÉèÖÃÊ§°ÜÓ°Ïì½»Ò×´¦ÀíÁ÷³Ì
					RunLog("Node[%d]FP[%d]·ÇÊÜ¿Ø, ÖØĞÂÉèÖÃÖ÷¿Ø", node, i + 1);
					if (SetCtrlMode(i) != 0) {
						return;
					} else {
						reset_ctrl_flag[i] = 0;
						continue;
					}
			} 
		}


		if (gpIfsf_shm->auth_flag - FCC_SWITCH_AUTH != 0) {  //Èç¹ûÓĞºóÌ¨
			if ((__u8)FP_State_Closed == lpDispenser_data->fp_data[i].FP_State) {
				GetPumpStatus(i, &status);   // ±£³ÖÓëÓÍ»úÁª»ú,·ÀÖ¹Ä³Ğ©ÓÍ»ú×Ô¶¯ÇĞ»»Îª×ÔÖ÷¼ÓÓÍÄ£Ê½
				if (i == 0) {
					times += pump.panelCnt;					
				}
				if (times > (SENDSTATE *  pump.panelCnt)) {
					if (i >= pump.panelCnt -1)
						times = 0;
					RunLog("Node[%d].FP[%d]´¦ÓÚClosed×´Ì¬", node, i + 1);
				}

				// Closed ×´Ì¬ÏÂÒ²ÒªÄÜ¹»±ä¼Û
				ChangePrice();
				continue; 
			}
		} else {
			if ((__u8)FP_State_Closed == lpDispenser_data->fp_data[i].FP_State) {
				RunLog("DoPolling, Ã»ÓĞºóÌ¨¸Ä±äFP[%02x]µÄ×´Ì¬ÎªIDLE", i + 0x21);
				ret = SetHbStatus(node, 0);   // IFSF_HEARTBEAT.status set to 0
				ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);
			}
		}

		// ÈôFP×´Ì¬ÎªInoperative, ÒªÇóÈË¹¤¸ÉÔ¤,³ÌĞò²»ÄÜ¸Ä±äÆä×´Ì¬,Ò²²»ÄÜÔÚ¸Ã×´Ì¬ÏÂ×öÈÎºÎ²Ù×÷
		if ((__u8)FP_State_Inoperative == lpDispenser_data->fp_data[i].FP_State) {
			RunLog("Node[%d].FP[%d]´¦ÓÚInoperative×´Ì¬!", node, i + 1);
			// Inoperative ×´Ì¬ÏÂÒ²ÒªÄÜ¹»±ä¼Û
			ChangePrice();

			CheckPriceErr(i);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = 0;

		switch (status & 0xa0) {
		case PUMP_OFF:					// ¹ÒÇ¹
			dostop_flag[i] = 0;
			if (fp_state[i] >= 2) {                 // Note.¶ÔÃ»ÓĞ²Éµ½¹ÒÇ¹ĞÅºÅ(EOT)µÄ²¹³ä´¦Àí
				// ÈôÓĞ¼Ó¹ıÓÍ, Ôò½øĞĞ×´Ì¬¸Ä±äºÍ½»Ò×´¦Àí
				// 1. ChangeFP_State to IDLE first
				tmp_lnz = lpDispenser_data->fp_data[i].Log_Noz_State;
				lpDispenser_data->fp_data[i].Log_Noz_State = 0x00;     
				if ((__u8)FP_State_Idle != lpDispenser_data->fp_data[i].FP_State) {
					ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);
				}
				// Step2. Get & Save TR to TR_BUFFER
				lpDispenser_data->fp_data[i].Log_Noz_State = tmp_lnz;
				ret = DoEot(i);
			}

			lpDispenser_data->fp_data[i].Log_Noz_State = 0x00;
			fp_state[i] = 0;

			if (((__u8)FP_State_Idle != lpDispenser_data->fp_data[i].FP_State) && 
				((__u8)FP_State_Authorised != lpDispenser_data->fp_data[i].FP_State)) {
				ret = ChangeFP_State(node, i + 0x21, FP_State_Idle, NULL);
			}

			//  clear in_progress_offline_tr_flag
			EO_OFFLINE_TR(node, i);
			if (gpIfsf_shm->fp_offline_flag[node - 1][i] != 0) {
				upload_offline_tr(node, NULL);
			}

			for (j = 0; j < pump.panelCnt; j++) {		 
				 if ((__u8)FP_State_Idle != lpDispenser_data->fp_data[j].FP_State) {
				           break;
				 }
			}

			if (j >= pump.panelCnt) {
				ret = ChangePrice();
			} 
			break;
		case PUMP_CALL:					// Ì§Ç¹
			if ((dostop_flag[i] != 0)) {
				// ¼ÓÓÍ¹ı³ÌÖĞÖ´ĞĞDoStopÖ®ºóÓÍ»ú×´Ì¬»á±äÎªPUMP_CALL,
				// ËùÒÔ´ËÄ£¿é±ØĞë¼Ó´ËÅĞ¶Ï, ·ñÔòÁ¬Ğø¼ÓÓÍ»áµ¼ÖÂ½»Ò×¶ªÊ§
				break;
			}

			if ((__u8)FP_State_Calling > lpDispenser_data->fp_data[i].FP_State) {
				ret = DoCall(i);
				fp_state[i] = 1;

				DoStarted(i);
			} else if (FP_State_Authorised == lpDispenser_data->fp_data[i].FP_State) {
				DoStarted(i);						  
			}

			break;
		case PUMP_BUSY:                       // ¼ÓÓÍ
			if (dostop_flag[i] != 0) {    // Èç¹ûÔİÍ£³É¹¦Ôò±£³ÖIDLE×´Ì¬
				break;
			}

			// ²¹³ä´¦Àí
			if (fp_state[i] < 2) {
				if (DoStarted(i) < 0) {
					continue;
				}
			}

			// ²¹³ä´¦Àí
			if ((__u8)FP_State_Started > lpDispenser_data->fp_data[i].FP_State) {
				ret = ChangeFP_State(node, i + 0x21, FP_State_Started, NULL);
			}

			DO_INTERVAL(DO_BUSY_INTERVAL, ret = DoBusy(i););

			if ((__u8)FP_State_Fuelling > lpDispenser_data->fp_data[i].FP_State) {
				if (fp_state[i] == 3) {
					ret = ChangeFP_State(node, i + 0x21, FP_State_Fuelling, NULL);
					if (ret < 0) {
						break;
					}
				}
			}
			
			break;
		case PUMP_AUTH:
			if ((__u8)FP_State_Authorised != lpDispenser_data->fp_data[i].FP_State) {
				ret = ChangeFP_State(node, i + 0x21, FP_State_Authorised, NULL);
			}
			break;
		default:
			break;
		}
		
	}
}


/*
 * func: »ñÈ¡¸ÃFPËùÓĞÇ¹µÄ±ÃÂë
 */
static int GetPumpTotal(int pos)
{
	int ret;
	int trynum;

	for (trynum = 0; trynum <TRY_TIME; trynum++) {
		Ssleep(INTERVAL);

		pump.cmd[0] = 0xf5;
		pump.cmd[1] = 0x02 | pump.panelPhy[pos];
		pump.cmd[2] = 0xc5;
		pump.cmd[3] = Check_xor_bdr(&pump.cmd[0], 3);
		pump.cmdlen = 4;
		pump.wantLen = 14;
		
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]ÏÂ·¢¶Á±ÃÂëÃüÁîÊ§°Ü", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]ÏÂ·¢¶Á±ÃÂëÃüÁî³É¹¦, È¡·µ»ØĞÅÏ¢Ê§°Ü", node, pos + 1);
			continue;
		}

		if (0xF5 != pump.cmd[0] || 0xc5 != pump.cmd[12]) {
			RunLog("Node[%d].FP[%d]·µ»ØĞÅÏ¢´íÎó", node, pos + 1);
			continue;
		}

		if (Check_xor_bdr(&pump.cmd[0], pump.wantLen - 1) !=
						pump.cmd[pump.wantLen - 1]) {
			RunLog("Node[%d].FP[%d]·µ»ØĞÅÏ¢Ğ£Ñé´íÎó", node, pos + 1);
			continue;
		}

		return 0;
	}

	return -1;
}

/*
 * func: ¼ÓÓÍ½áÊø£¬½øĞĞ½»Ò×¼ÇÂ¼µÄ¶ÁÈ¡ºÍ´¦Àí
 *       Áã½»Ò×µÄÅĞ¶ÏºÍ´¦ÀíÓÉDoEotÀ´×ö, DoPoling ²»¹ØĞÄ
 * ret : 0 - ³É¹¦; -1 - Ê§°Ü
 *
 */
static int DoEot(int pos)
{
	int i;
	int ret;
	int pr_no;
	int err_cnt;
	__u8 price[5];
	__u8 volume[5];
	__u8 amount[5];
	__u8 total[7];
	__u8 status;
	__u8 msg[13];
	__u8 meter_total[7] = {0};
	__u8 amount_total[7] = {0};
	__u8 pre_total[7];
	
	RunLog("½Úµã[%d]Ãæ°å[%d]´¦ÀíĞÂ½»Ò×", node, pos + 1);
	
	//1. »ñµÃ±ÃÂë£¬ºÍÀÛ»ı¼ÓÓÍ½ğ¶î
	err_cnt = 0;
	do {
		ret = GetPumpTotal(pos);
		if (ret < 0) {
			RunLog("½Úµã[%d]Ãæ°å[%d]¶ÁÈ¡±ÃÂëÊ§°Ü", node, pos + 1);
			err_cnt++;
		}
	} while (ret < 0 && err_cnt <= 5);

	if (ret < 0) {
		return -1;
	}

	meter_total[0] = 0x0A;
	meter_total[1] = 0x00;
	memcpy(&meter_total[2], pump.cmd + 2, 5);

	amount_total[0] = 0x0A;
	amount_total[1] = 0x00;
	memcpy(&amount_total[2], pump.cmd +7, 5);

	memcpy(pre_total, (lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).Log_Noz_Vol_Total, 7);
	
	//3. µÃµ½ÓÍÆ·id ,Ó¦¸ÃÎª0	
	pr_no = (lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).PR_Id - 1;  // 0 - 7	
	if ((pr_no < 0) || (pr_no > 7)) {
		RunLog("DoEot PR_Id error, PR_Id[%d] <ERR>", pr_no + 1);
		// Ö»¼ÇÂ¼´íÎó²»ÍË³ö
	}
	

	//4. ¼ì²é±ÃÂëÊÇ·ñÓĞ±ä»¯,Èç¹ûÓĞÔò±£´æ½»Ò×¼ÇÂ¼
	if (memcmp(pre_total, meter_total, 7) != 0) {
	//---------------±ÃÂëÓĞ²îÒìÊ±²Å»áÈ¥È¡¶ÔÓ¦µÄ½»Ò×¼ÇÂ¼-----------------------//
	       //1. »ñµÃ¼ÓÓÍ¼ÇÂ¼
		err_cnt = 0;
		do {
			ret = GetTransData(pos);	
			if (ret != 0) {
				err_cnt++;
				RunLog("½Úµã[%d]Ãæ°å[%d]¶ÁÈ¡½»Ò×¼ÇÂ¼Ê§°Ü", node, pos + 1);
			}
		} while (ret < 0 && err_cnt <= 5);

		if (ret < 0) {
			return -1;
		}

		memset(msg, 0, sizeof(msg));
		memcpy(msg, pump.cmd, 12);
		msg[12] = 0;

		//2.»ñµÃÓÍ¼Û
		ret = GetPrice(pos);	
		if (ret != 0) {
			RunLog("Node[%d]FP[%d]È¡ÓÍ¼ÛÊ§°Ü", node, pos + 1);
			memcpy(price, lpDispenser_data->fp_fm_data[pr_no][lpDispenser_data->fp_data[pos].Default_Fuelling_Mode - 1].Prod_Price, 4);
		} else {
			price[0] = 0x04;
			price[1] = 0x00;
			price[2] = pump.cmd[2];
			price[3] = pump.cmd[3];
		}
					
		//Current_Unit_Price bin8 + bcd8
		memcpy(lpDispenser_data->fp_data[pos].Current_Unit_Price, price, 4);


		//Current_Volume  bin8 + bcd8
		volume[0] = 0x06;
		volume[1] = 0x00;
		memcpy(&volume[2],  msg+ 2, 3);
		memcpy(lpDispenser_data->fp_data[pos].Current_Volume, volume, 5);

		//Current_Amount  bin8 + bcd 8
		amount[0] = 0x06;
		amount[1] = 0x00;
		memcpy(&amount[2],  msg+5, 3);
		memcpy(lpDispenser_data->fp_data[pos].Current_Amount, amount, 5);

		//chinese tax dispenser transimit in XXXXXXX.X, so use 0b as first byte.
		memcpy(lpDispenser_data->fp_m_data[pr_no].Meter_Total, meter_total, 7);
		memcpy((lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).Log_Noz_Vol_Total, meter_total, 7);

#if 0
		RunLog("½Úµã[%02d]Ãæ°å[%d]Ç¹[%d] ## Amount: %02x%02x.%02x Volume: %02x%02x.%02x Price: %02x.%02x Total: %02x%02x%02x%02x.%02x", node, pos + 1, pump.gunLog[pos], amount[2], 
			amount[3], amount[4], volume[2], volume[3], volume[4], price[2],
			price[3], meter_total[2], meter_total[3], meter_total[4], meter_total[5], meter_total[6]);
#endif
		
		// Update Amount_Total
		memcpy((lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).Log_Noz_Amount_Total, amount_total, 7);
		memcpy(lpDispenser_data->fp_data[pos].Current_Prod_Nb, lpDispenser_data->fp_pr_data[pr_no].Prod_Nb, 4);

		err_cnt = 0;
		do {
			ret = EndTr4Ln(node, pos + 0x21, pump.gunLog[pos] + 0x10);
			if (ret < 0) {
				RunLog("½Úµã[%d]Ãæ°å[%d]DoEot.EndTr4Ln Ê§°Ü, ret: %d <ERR>", node,pos +1 ,ret);
				err_cnt++;
			}
		} while (ret < 0 && err_cnt <= 5);

		err_cnt = 0;
		do {
			ret = EndTr4Fp(node, pos + 0x21, pre_total,
				(lpDispenser_data->fp_ln_data[pos][pump.gunLog[pos] - 1]).Log_Noz_Vol_Total);
			if (ret < 0) {
				RunLog("½Úµã[%d]Ãæ°å[%d] DoEot.EndTr4Fp Ê§°Ü, ret: %d <ERR>", node,pos + 1,ret);
				err_cnt++;
			}
		} while (ret < 0 && err_cnt <= 8);

	}else {
		ret = EndZeroTr4Fp(node, pos + 0x21);
		if (ret < 0) {
			RunLog("½Úµã[%d]Ãæ°å[%d]DoEot.EndZeroTr4Fp Ê§°Ü, ret: %d <ERR>", node,pos+1,ret);
		}
	}
	
	
	return 0;
}

/*
 * ³õÊ¼»¯FPµÄµ±Ç°ÓÍ¼Û. ´ÓÅäÖÃÎÄ¼şÖĞ¶ÁÈ¡µ¥¼ÛÊı¾İ,
 * Ğ´Èë fp_data[].Current_Unit_Price
 * ¼ÙÉè PR_Id Óë PR_Nb ÊÇÒ»Ò»¶ÔÓ¦µÄ
 */
static int InitCurrentPrice() {
	int i, j;
	int ret;
	int pr_no;
	__u8 price[4];

	for (i = 0; i < pump.panelCnt; i++) {
		for (j = 0; j < pump.gunCnt[i]; j++) {
			pr_no = (lpDispenser_data->fp_ln_data[i][0]).PR_Id - 1;  // 0-7	
			if ((pr_no < 0) || (pr_no > 7)) {
				RunLog("InitPumpPrice PR_Id error, PR_Id[%d]", pr_no + 1);
				return -1;
			}

			memcpy(lpDispenser_data->fp_data[i].Current_Unit_Price, 
					lpDispenser_data->fp_fm_data[pr_no][lpDispenser_data->fp_data[i].Default_Fuelling_Mode - 1].Prod_Price, 4);
#ifdef DEBUG
		RunLog("Init.FP %d LNZ %d PR_Id %d,Prod_Pricd: %02x%02x%02x%02x", i + 1, pump.gunLog[i], pr_no + 1,
				lpDispenser_data->fp_fm_data[pr_no][lpDispenser_data->fp_data[i].Default_Fuelling_Mode - 1].Prod_Price[0],
				lpDispenser_data->fp_fm_data[pr_no][lpDispenser_data->fp_data[i].Default_Fuelling_Mode - 1].Prod_Price[1],
				lpDispenser_data->fp_fm_data[pr_no][lpDispenser_data->fp_data[i].Default_Fuelling_Mode - 1].Prod_Price[2],
				lpDispenser_data->fp_fm_data[pr_no][lpDispenser_data->fp_data[i].Default_Fuelling_Mode - 1].Prod_Price[3]);
#endif
		}
	}


	return 0;
}


/*
 * func: ³õÊ¼»¯ÓÍÇ¹±ÃÂë
 */
static int InitPumpTotal()
{
	int ret;
	int i;
	int pr_no;
	__u8 status;
	__u8 meter_total[7] = {0};
	__u8 amount_total[7] = {0};

	for (i = 0; i < pump.panelCnt; i++) {
		ret = GetPumpTotal(i);
		if (ret < 0) {
			RunLog("È¡Node[%d].FP[%d]µÄ±ÃÂëÊ§°Ü", node, i);
			return -1;
		}

		// Volume_Total
		pr_no = (lpDispenser_data->fp_ln_data[i][pump.gunLog[i] - 1]).PR_Id - 1;  // 0-7	
		
		meter_total[0] = 0x0A;
		meter_total[1] = 0x00;
		memcpy(&meter_total[2], pump.cmd + 2, 5);
		
		memcpy(lpDispenser_data->fp_m_data[pr_no].Meter_Total, meter_total, 7);
		memcpy((lpDispenser_data->fp_ln_data[i][pump.gunLog[i] - 1]).Log_Noz_Vol_Total, meter_total, 7);
#if DEBUG
		RunLog("Init.fp_ln_data[%d][%d].Vol_Total: %s ", i, pump.gunLog[i] - 1,
			Asc2Hex(lpDispenser_data->fp_ln_data[i][pump.gunLog[i] - 1].Log_Noz_Vol_Total, 7));
		RunLog("Init.fp_m_data[%d].Meter_Total[%s] ", pr_no, Asc2Hex(meter_total, 7));
				
#endif

	       // Amount_Total
		amount_total[0] = 0x0A;
		amount_total[1] = 0x00;
		memcpy(&amount_total[2], pump.cmd + 7, 5);
		memcpy((lpDispenser_data->fp_ln_data[i][pump.gunLog[i] - 1]).Log_Noz_Amount_Total, amount_total, 7);
		RunLog("Init.fp_ln_data[%d][%d].Amount_Total: %s ", i, pump.gunLog[i] - 1,
			Asc2Hex(lpDispenser_data->fp_ln_data[i][pump.gunLog[i] - 1].Log_Noz_Amount_Total, 7));

	}

	return 0;
}



/*
 * func: ¶ÁÈ¡µ±´Î½»Ò×¼ÇÂ¼
 */ 
static int GetTransData(int pos)
{
	int ret;
	int trynum;

	for (trynum = 0; trynum < TRY_TIME; ++trynum) { 
		Ssleep(INTERVAL);
		pump.cmd[0] = 0xf5;
		pump.cmd[1] = 0x02 | pump.panelPhy[pos];
		pump.cmd[2] = 0xd9;
		pump.cmd[3] = Check_xor_bdr(&pump.cmd[0], 3);
		pump.cmdlen = 4;
		pump.wantLen = 10;

		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
			       	pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]ÏÂ·¢¶Á½»Ò×ÃüÁîÊ§°Ü", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
	 		RunLog("Node[%d].FP[%d]ÏÂ·¢¶Á½»Ò×ÃüÁî³É¹¦, È¡·µ»ØĞÅÏ¢Ê§°Ü", node, pos + 1);
			continue;
		}

		if (0xF5 != pump.cmd[0] || 0xd9 != pump.cmd[8]) {
			RunLog("Node[%d].FP[%d]·µ»ØÊı¾İ´íÎó", node, pos + 1);
			continue;
		}

		if (Check_xor_bdr(&pump.cmd[0], pump.wantLen - 1) !=
						pump.cmd[pump.wantLen - 1]) {
			RunLog("Node[%d].FP[%d]·µ»ØÊı¾İĞ£Ñé´íÎó", node, pos + 1);
			continue;
		}
			
		return 0;
	}

	return -1;
}


/*
 * func: Release_FP
 */
static int DoAuth(int pos)
{
	int ret;
	int trynum;
	__u8 status;

	RunLog("Node[%d].FP[%d] DoAuth", node, pos + 1);

	for (trynum = 0; trynum < TRY_TIME; trynum++) {
		pump.cmd[0] = 0xf5;
		pump.cmd[1] = 0x02 | pump.panelPhy[pos];
		pump.cmd[2] = 0xc3;
		pump.cmd[3] = Check_xor_bdr(&pump.cmd[0], 3);
		pump.cmdlen = 4;
		pump.wantLen = 5;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]ÏÂ·¢ÊÚÈ¨ÃüÁîÊ§°Ü", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]ÏÂ·¢ÊÚÈ¨³É¹¦, È¡·µ»ØĞÅÏ¢Ê§°Ü",
								node, pos + 1);
			continue;
		}

		if (0x59 != pump.cmd[2] || 0xF5 != pump.cmd[0]) {
			RunLog("Node[%d].FP[%d]·µ»ØÊı¾İ´íÎó", node, pos + 1);
			continue;
		}

		if (Check_xor_bdr(&pump.cmd[0], pump.wantLen - 1) !=
						pump.cmd[pump.wantLen - 1]) {
			RunLog("Node[%d].FP[%d]·µ»ØÊı¾İĞ£Ñé´íÎó", node, pos + 1);
			continue;
		}

#if 0						
		ret = GetPumpStatus(pos, &status);
		if (ret < 0) {
			continue;
		}

		if ((status & 0xa0) == PUMP_BUSY) {
			if (fp_state[pos] < 2) {
				DoStarted(pos);
			}
			return 0;
		}


		if ((status & 0xa0) == PUMP_AUTH) {				
			return 0;
		}
#endif
		return 0;
	}

	return -1;
}

/*
 * func: ÉÏ´«ÊµÊ±¼ÓÓÍÊı¾İ
 */
static int DoBusy(int pos)
{
	int ret;
	int trynum;
	__u8 amount[5],volume[5];
	static __u8 arr_zero[3] = {0};

	//1. »ñµÃ¼ÓÓÍÁ¿ºÍ¼ÓÓÍ½ğ¶î
	ret = GetTransData(pos);
	if (ret < 0) {
		return -1;
	}
	
	volume[0] = 0x06;
	volume[1] = 0x00;
	memcpy(&volume[2], &pump.cmd[2], 3);

	amount[0] = 0x06;
	amount[1] = 0x00;
	memcpy(&amount[2], &pump.cmd[5], 3);

	if (fp_state[pos] != 3) {
		if (memcmp(arr_zero, &volume[2], 3) == 0) {
			return 0;
		}
		fp_state[pos] = 3;
		return 0;
	}

	if ((__u8)FP_State_Fuelling == lpDispenser_data->fp_data[pos].FP_State) {	 
	    ret = ChangeFP_Run_Trans(node, pos + 0x21, amount,  volume);//Ö»ÓĞÒ»¸öÃæ°å
	}

	return 0;
}

/*
 * func: Ì§Ç¹´¦Àí - ±¨¸æÌ§Ç¹ĞÅºÅ
 */
static int DoCall(int pos)
{
	int ret;

	ret = GetCurrentLNZ(pos);
	if (ret < 0) {
		RunLog("½Úµã[%d]Ãæ°å[%d], »ñÈ¡µ±Ç°ÓÍÇ¹Ç¹ºÅÊ§°Ü", node, pos + 1);
		return -1;
	}

	ret = ChangeFP_State(node, pos + 0x21, FP_State_Calling, NULL);

	//ÉÏÎ»»úÔÚÏß,µ«ÊÇÔÊĞí×Ô¶¯ÊÚÈ¨;ÉÏÎ»»ú²»ÔÚÏß, ÊÚÈ¨¿ª¹ØÖÃÓÚ¿ªµÄ×´Ì¬,´ËÊ±×Ô¶¯ÊÚÈ¨
	if ((gpIfsf_shm->auth_flag == FCC_SWITCH_AUTH) && (lock_status() == 1)) {
		ret = DoAuth(pos);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]×Ô¶¯ÊÚÈ¨Ê§°Ü", node, pos + 1);
		}
		ret = ChangeFP_State(node, pos + 0x21, FP_State_Started, NULL);
		RunLog("Node[%d].FP[%d]×Ô¶¯ÊÚÈ¨³É¹¦", node, pos + 1);
		SO_OFFLINE_TR(node, pos);
	}

	return ret;
}


/*
 * func: ¶ÁÈ¡FP×´Ì¬
 */
static int GetPumpStatus(int pos, __u8 *status)
{
	int trycnt;
	int ret;
	__u8  pump_pos;
	static __u8 pre_status[MAX_FP_CNT] = {0};

	*status = 0x80;
	
	for (trycnt = 0; trycnt < 2; ++trycnt) {
		pump.cmd[0] = 0xf5;
		pump.cmd[1] = 0x02 | pump.panelPhy[pos];

		pump_pos = pump.cmd[1];
		pump.cmd[2] = 0xd5;
		pump.cmd[3] = Check_xor_bdr(&pump.cmd[0], 3);
		pump.cmdlen = 4;
		pump.wantLen = 5;

		Ssleep(INTERVAL); 
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]ÏÂ·¢¶Á×´Ì¬ÃüÁîÊ§°Ü", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]ÏÂ·¢¶Á×´Ì¬ÃüÁî³É¹¦, È¡·µ»ØĞÅÏ¢Ê§°Ü", node, pos + 1);
			continue;
		} 
		
		if (0xF5 != pump.cmd[0] || ((pump.cmd[1] & 0xF0) != (pump_pos & 0xF0))) {
			RunLog("Node[%d].FP[%d]·µ»Ø×´Ì¬Êı¾İ´íÎó", node, pos + 1);
			continue;
		}

		if (Check_xor_bdr(&pump.cmd[0], pump.wantLen - 1) !=
						pump.cmd[pump.wantLen - 1]) {
			RunLog("Node[%d].FP[%d]·µ»ØÊı¾İĞ£Ñé´íÎó", node, pos + 1);
			continue;
		}		
			  	
		*status = pump.cmd[2];

		if (*status != pre_status[pos]) {
			pre_status[pos] = *status;
			RunLog("Node[%d].FP[%d]µ±Ç°×´Ì¬[%02x], status & 0xa0 = [%02x]",
							node, pos + 1, *status, (*status & 0xa0));
		}

		return 0;
	}

	return -1;
}

//test wether the dispenser is online, we only use fp 1 to test.
/*
 * func: ÅĞ¶ÏDispenserÊÇ·ñÔÚÏß
 */
static int GetPumpOnline(int pos, __u8 *status)
{
	int i;
	int ret;
	__u8  pump_pos;
	*status = 0x00;

	for (i = 0; i < TRY_TIME; ++i) {
		pump.cmd[0] = 0xf5;
		pump.cmd[1] = 0x02 | pump.panelPhy[pos];

		pump_pos = pump.cmd[1];
		pump.cmd[2] = 0xd5;
		pump.cmd[3] = Check_xor_bdr(&pump.cmd[0], 3);
		pump.cmdlen = 4;
		pump.wantLen = 5;

		Ssleep(INTERVAL); 
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]ÏÂ·¢¶Á×´Ì¬ÃüÁîÊ§°Ü", node, pos + 1);
			continue;
		}

		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;
		
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]ÏÂ·¢¶Á×´Ì¬ÃüÁî³É¹¦, È¡·µ»ØĞÅÏ¢Ê§°Ü", node, pos + 1);
			continue;
		} 
		
		if (0xF5 != pump.cmd[0] || ((pump.cmd[1] & 0xF0) != (pump_pos & 0xF0))) {
			RunLog("Node[%d].FP[%d]·µ»Ø×´Ì¬Êı¾İ´íÎó", node, pos + 1);
			continue;
		}

		if (Check_xor_bdr(&pump.cmd[0], pump.wantLen - 1) !=
						pump.cmd[pump.wantLen - 1]) {
			RunLog("Node[%d].FP[%d]·µ»ØÊı¾İĞ£Ñé´íÎó", node, pos + 1);
			continue;
		}

		*status = pump.cmd[2];
		return 0;
	}

	return -1;
}

/*
 *  func: ³õÊ¼µÄgunLog, Ô¤ÖÃ¼ÓÓÍÖ¸ÁîĞèÒª; ²»ĞŞ¸Ä Log_Noz_State
 *  Èı½ğ¼ÓÓÍ»úÒ»ÌõÍ¨µÀ¶ÔÓ¦Ò»ÌõÇ¹
 */
static int InitGunLog()
{
	int i;

	for (i = 0; i < pump.panelCnt; ++i) {
		pump.gunLog[i] =  1;   
	}

	return 0;
}


/*
 * func: »ñÈ¡µ±Ç°Ì§ÆğÓÍÇ¹µÄÇ¹ºÅ, ½á¹ûĞ´Èëpump.gunLog[]£¬²¢ChangeLog_Noz_State
 * µ¥Ç¹»ú
 * ret : 0 - ³É¹¦, -1 - Ê§°Ü 
 */
static int GetCurrentLNZ(int pos)
{	
	  pump.gunLog[pos] = 1;	// µ±Ç°Ì§ÆğµÄÓÍÇ¹	
	  RunLog("Node %d FP %d µÄ %d ºÅÇ¹Ì§Æğ", node, pos + 1, pump.gunLog[pos]);
	  lpDispenser_data->fp_data[pos].Log_Noz_State = 1;

	  return 0;
}

/*
 * func: Éè¶¨´®¿ÚÊôĞÔ¡ªÍ¨Ñ¶ËÙÂÊ¡¢Ğ£ÑéÎ»µÈ
 */
static int SetPort010()
{
	int ret;

	pump.cmd[0] = 0x1C;
	pump.cmd[1] = 0x52;
	pump.cmd[2] = 0x30 | (pump.chnPhy - 1);
	pump.cmd[3] = 0x06;                             //4800bps
	pump.cmd[4] = 0x01<<((pump.chnPhy - 1) % 8);	//EVEN parity
	pump.cmd[5] = 0x00;
	pump.cmd[6] = 0x01<<((pump.chnPhy - 1) % 8);	//EVEN parity
	pump.cmdlen = 7;
	pump.wantLen = 0;

	ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy,
		pump.cmd, pump.cmdlen, 0, pump.wantLen, TIME_OUT_WRITE_PORT);
	if (ret < 0) {
		RunLog("Í¨µÀ%dÉèÖÃ´®¿ÚÊôĞÔÊ§°Ü", pump.chnLog);
		return -1;
	}
	
	Ssleep(INTERVAL);

	return 0;
}


/*
 * func:  Í¨Ñ¶²»Í¨Ê±ÂÖÑ¯¹ı³ÌÖĞ¸ôÒ»¶ÎÊ±¼äÖØÉèÒ»ÏÂ´®¿ÚÊôĞÔ
 *        ±ÜÃâÒòÒâÍâµÄµçÑ¹²»ÎÈµ¼ÖÂµ¥Æ¬»ú¸´Î»--´®¿ÚÊôĞÔÉèÖÃ»Ö¸´Ô¤Éè9600,8,n,1
 *        [ÀëÏßÆÚ¼äÊ¹ÓÃ, ±ÜÃâÀëÏßÆÚ¼äµôµçÔì³Éµ¥Æ¬»ú¸´Î»]
 */
static int ReDoSetPort010()
{
#define RE_DO_FREQUENCY   10         //  10´ÎÂÖÑ¯Ê§°ÜÉèÖÃÒ»ÏÂ´®¿ÚÊôĞÔ

	int static polling_fail_cnt = 0;
	int ret;

	if ((++polling_fail_cnt) >= RE_DO_FREQUENCY) {
		ret = SetPort010();
		if (ret < 0) {
			RunLog("½Úµã %d ÖØÉè´®¿ÚÊôĞÔÊ§°Ü", node);
		}
		polling_fail_cnt = 0;
	}

	return ret;
}


/*
 * func: Éè¶¨Ö÷¿ØÄ£Ê½
 */
static int SetCtrlMode(int pos)
{
	int i;
	int ret;

	for (i = 0; i < TRY_TIME; i++) {
		pump.cmd[0] = 0xf5;
		pump.cmd[1] = 0x02 | pump.panelPhy[pos];
		pump.cmd[2] = 0xE5;
		pump.cmd[3] = Check_xor_bdr(&pump.cmd[0], 3);
		pump.cmdlen = 4;
		pump.wantLen = 4;

		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy,
			       	pump.cmd, pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]ÏÂ·¢ÉèÖÃÖ÷¿ØÃüÁîÊ§°Ü", node, pos + 1);
			continue;
		}
		
		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]ÏÂ·¢ÉèÖÃÖ÷¿ØÃüÁî³É¹¦, È¡·µ»ØĞÅÏ¢Ê§°Ü", node, pos + 1);
		}

		if (0xF5 != pump.cmd[0] || 0xE5 != pump.cmd[2]) {
			RunLog("Node[%d].FP[%d]·µ»ØÊı¾İ´íÎó", node, pos + 1);
			continue;
		}

		if (Check_xor_bdr(&pump.cmd[0], pump.wantLen - 1) !=
						pump.cmd[pump.wantLen - 1]) {
			RunLog("Node[%d].FP[%d]·µ»ØÊı¾İĞ£Ñé´íÎó", node, pos + 1);
			continue;
		}

		RunLog("Node[%d].FP[%d]ÏÂ·¢ÉèÖÃÖ÷¿ØÃüÁî³É¹¦", node, pos + 1);
		return 0;
	}

	return -1;
}


static int GetPrice(int pos)
{
	int ret;
	int trynum;
	
	for (trynum = 0; trynum < TRY_TIME; trynum++) {
		pump.cmd[0] = 0xf5;
		pump.cmd[1] = 0x02 | pump.panelPhy[pos];
		pump.cmd[2] = 0xb6;
		pump.cmd[3] = Check_xor_bdr(&pump.cmd[0], 3);
		pump.cmdlen = 4;
		pump.wantLen = 6;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, 
			pump.cmd, pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]ÏÂ·¢¶Áµ¥¼ÛÃüÁîÊ§°Ü", node, pos + 1);
			continue;
		}
		memset(pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd) - 1;

		ret = GetReturnMsg(GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]ÏÂ·¢¶Áµ¥¼ÛÃüÁî³É¹¦, È¡·µ»ØĞÅÏ¢Ê§°Ü",
									node, pos + 1);
			continue;
		}

		if (0xF5 != pump.cmd[0] || 0xB6 != pump.cmd[4]) {
			RunLog("Node[%d].FP[%d]·µ»ØÊı¾İ´íÎó", node, pos + 1);
			continue;
		}

		if (Check_xor_bdr(&pump.cmd[0], pump.wantLen - 1) !=
						pump.cmd[pump.wantLen - 1]) {
			RunLog("Node[%d].FP[%d]·µ»ØÊı¾İĞ£Ñé´íÎó", node, pos + 1);
			continue;
		}

		return 0;
	}

	return -1;
}


/*
 * func: ÔİÍ£¼ÓÓÍ£¬ÔÚ´Ë×öTerminate_FPÓÃ
 */
static int DoStop(int pos)
{
	int ret;
	int trynum;
	__u8 status;

	RunLog("Node[%d].FP[%d] DoStop", node, pos + 1);

	for (trynum = 0; trynum < TRY_TIME; trynum++) {
		pump.cmd[0] = 0xf5;
		pump.cmd[1] = 0x02 | pump.panelPhy[pos];
		pump.cmd[2] = 0xca;
		pump.cmd[3] = Check_xor_bdr(&pump.cmd[0], 3);
		pump.cmdlen = 4;
		pump.wantLen = 4;

		Ssleep(INTERVAL);
		ret = WriteToPort(pump.fd, pump.chnLog, pump.chnPhy, pump.cmd,
				pump.cmdlen, 1, pump.wantLen, TIME_OUT_WRITE_PORT);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]ÏÂ·¢Í£Ö¹¼ÓÓÍÃüÁîÊ§°Ü", node, pos + 1);
			continue;
		}

		memset( pump.cmd, 0, sizeof(pump.cmd));
		pump.cmdlen = sizeof(pump.cmd)-1;

		ret = GetReturnMsg( GUN_MSG_TRAN, pump.chnLog, pump.cmd, &pump.cmdlen);
		if (ret < 0) {
			RunLog("Node[%d].FP[%d]ÏÂ·¢Í£Ö¹¼ÓÓÍÃüÁî³É¹¦, È¡·µ»ØĞÅÏ¢Ê§°Ü", node, pos + 1);
			continue;
		}

		Ssleep(150000);
		ret = GetPumpStatus(pos, &status);  // ÆğÑÓÊ±×÷ÓÃ, Ö´ĞĞ´ËÃüÁîºóÓÍ»ú×´Ì¬Òª400-1000msºó²Å¸Ä±ä

		ret = GetPumpStatus(pos, &status);
		if (ret < 0) {
			continue;
		}

		if (((status & 0xa0) == PUMP_CALL) || ((status & 0xa0) == PUMP_OFF)) {
			return 0;
		}
	}

	return -1;
}


/*
 * func: ÓëÓÍ»ú±£³ÖÍ¨Ñ¶ xÃë, ÁË½âÔÚµÈ´ıÆÚ¼äÓÍ»úµÄ×´Ì¬, ¶ÔÓÚÄ³Ğ©ÓÍ»ú»¹ÓĞ±£³ÖÖ÷¿ØÄ£Ê½µÄ×÷ÓÃ
 * return: GetPumpStatusÊ§°ÜµÄ´ÎÊı
 */
static int keep_in_touch_xs(int x)
{
	int i;
	int fail_times = 0;
	time_t return_time = 0;
	__u8 status;

	return_time = time(NULL) + (x);
	RunLog("==================return_time: %ld =======", return_time);
	do {
		sleep(1);
		for (i = 0; i < pump.panelCnt; ++i) {
			if (GetPumpStatus(i, &status) < 0) {
				fail_times++;
			}
		}
	} while (time(NULL) < return_time);

	return fail_times;
}


/*
 * ¼ì²éFPÉÏ¸÷Ç¹µÄÓÍÆ·µ¥¼ÛÊÇ·ñ·ûºÏÕıÈ·(·ûºÏÊµ¼Ê), Èç¹ûÈ«²¿ÕıÈ·Ôò¸Ä±ä×´Ì¬ÎªClosed
 */
static int CheckPriceErr(int pos)
{
	int ret, fm_idx;
	int lnz, pr_no;
	__u8 status;

	if ((__u8)FP_State_Inoperative != lpDispenser_data->fp_data[pos].FP_State) {
		return -1;
	}

	ret = GetPrice(pos);	
	if (ret < 0) {
		return -1;
	}

	for (lnz = 0; lnz < pump.gunCnt[pos]; lnz++) {
		pr_no = (lpDispenser_data->fp_ln_data[pos][lnz]).PR_Id - 1;  // 0 - 7	
		if ((pr_no < 0) || (pr_no > 7)) {
			RunLog("CheckPrice ERR PR_Id error, FP[%d]LNZ[%d] PR_Id[%d] <ERR>",
								pos + 1, lnz + 1, pr_no + 1);
			return -1;
		}

		fm_idx = lpDispenser_data->fp_data[pos].Default_Fuelling_Mode - 1;
		RunLog("Node %d FP %d LNZ %d Now price is %02x.%02x; DST Price: %02x.%02x",
			node,pos+1,lnz+1, pump.cmd[2], pump.cmd[3],
		       	lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[2], 
		       	lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[3]);

		if ((pump.cmd[2] != lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[2]) ||
			(pump.cmd[3] != lpDispenser_data->fp_fm_data[pr_no][fm_idx].Prod_Price[3])) {
			return -2;
		}
	}
	
	ChangeFP_State(node, 0x21 + pos, FP_State_Closed, NULL);
	RunLog("Node[%d]FP[%d]µ¥¼ÛÒÑ±ä¸üÕıÈ·, ÇĞ»»×´Ì¬ÎªClosed", node, pos + 1);

	return 0;
}

