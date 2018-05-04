#ifndef __EMSGDEF_H__
#define __EMSGDEF_H__

ERRMSG gErrMsg[ ] = {
	{ COMM_EOF,					"描述符关闭" 		},
	{ TCP_ILL_PORT,				"非法端口"	 		},
	{ TCP_ILL_IP,				"非法IP地址" 		},
	{ TCP_ILL_FAM,				"非法协议族"		},
	{ COMM_LIB_ERROR,			"其他错误" 			}
};

#endif
