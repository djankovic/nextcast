#ifndef AAC_DLL_H
#define AAC_DLL_H

#include <stdint.h>
#include <fdk-aac/aacenc_lib.h>

typedef AACENC_ERROR (*aacEncOpenPtr)(HANDLE_AACENCODER *, UINT, UINT);
extern aacEncOpenPtr aacEncOpen_butt;

typedef AACENC_ERROR (*aacEncoder_SetParamPtr)(HANDLE_AACENCODER, AACENC_PARAM, UINT);
extern aacEncoder_SetParamPtr aacEncoder_SetParam_butt;

typedef AACENC_ERROR (*aacEncoder_GetParamPtr)(HANDLE_AACENCODER, AACENC_PARAM);
extern aacEncoder_GetParamPtr aacEncoder_GetParam_butt;

typedef AACENC_ERROR (*aacEncEncodePtr)(HANDLE_AACENCODER, const AACENC_BufDesc *, const AACENC_BufDesc *, const AACENC_InArgs *, AACENC_OutArgs *);
extern aacEncEncodePtr aacEncEncode_butt;

typedef AACENC_ERROR (*aacEncInfoPtr)(HANDLE_AACENCODER, AACENC_InfoStruct *);
extern aacEncInfoPtr aacEncInfo_butt;

typedef AACENC_ERROR (*aacEncClosePtr)(HANDLE_AACENCODER *);
extern aacEncClosePtr aacEncClose_butt;

#endif // AAC_DLL_H
