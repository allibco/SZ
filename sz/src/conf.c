/**
 *  @file   conf.c
 *  @author Sheng Di (sdi1@anl.gov or disheng222@gmail.com)
 *  @date   2015.
 *  @brief  Configuration loading functions for the SZ library.
 *  (C) 2015 by Mathematics and Computer Science (MCS), Argonne National Laboratory.
 *      See COPYRIGHT in top-level directory.
 */

#include <math.h>
#include "string.h"
#include "sz.h"
#include "iniparser.h"
#include "Huffman.h"
#include "pastri.h"

/*-------------------------------------------------------------------------*/
/**
    @brief      It reads the configuration given in the configuration file.
    @return     integer         1 if successfull.

    This function reads the configuration given in the SZ configuration
    file and sets other required parameters.

 **/
 
/*struct node_t *pool;
node *qqq;
node *qq;
int n_nodes = 0, qend;
unsigned long **code;
unsigned char *cout;
int n_inode;*/ 
 
unsigned int roundUpToPowerOf2(unsigned int base)
{
  base -= 1;

  base = base | (base >> 1);
  base = base | (base >> 2);
  base = base | (base >> 4);
  base = base | (base >> 8);
  base = base | (base >> 16);

  return base + 1;
} 
 
void updateQuantizationInfo(int quant_intervals)
{
	//allNodes = 2*quant_intervals;
	//stateNum = quant_intervals;
	intvCapacity = quant_intervals;
	intvRadius = quant_intervals/2;
} 
 
void clearHuffmanMem()
{
   	memset(pool, 0, allNodes*sizeof(struct node_t));
	memset(code, 0, stateNum*sizeof(long*)); //original:128; we just used '0'-'7', so max ascii is 55.
	memset(cout, 0, stateNum);
    n_nodes = 0;
    n_inode = 0;
    qend = 1;	
} 
 
double computeABSErrBoundFromPSNR(double psnr, double threshold, double value_range)
{
	double v1 = psnr + 10 * log10(1-2.0/3.0*threshold);
	double v2 = v1/(-20);
	double v3 = pow(10, v2);
	return value_range * v3;
} 
 
/*-------------------------------------------------------------------------*/
/**
 * 
 * 
 * @return the status of loading conf. file: 1 (success) or 0 (error code);
 * */
int SZ_ReadConf() {
    // Check access to SZ configuration file and load dictionary
    //record the setting in conf_params
    conf_params = (sz_params*)malloc(sizeof(sz_params));    
    
    int x = 1;
    char sol_name[256];
    char *modeBuf;
    char *errBoundMode;
    char *endianTypeString;
    dictionary *ini;
    char *par;

	char *y = (char*)&x;
	
	if(*y==1)
		sysEndianType = LITTLE_ENDIAN_SYSTEM;
	else //=0
		sysEndianType = BIG_ENDIAN_SYSTEM;
	conf_params->sysEndianType = sysEndianType;
    
    if(sz_cfgFile == NULL)
    {
		conf_params->dataEndianType = dataEndianType = LITTLE_ENDIAN_DATA;
		conf_params->sol_ID = sol_ID = SZ;
		conf_params->layers = layers = 0; //x
		conf_params->max_quant_intervals = 65536;
		maxRangeRadius = conf_params->max_quant_intervals/2;
		
		stateNum = maxRangeRadius*2;
		allNodes = maxRangeRadius*4;
		
		intvCapacity = maxRangeRadius*2;
		intvRadius = maxRangeRadius;
		
		conf_params->quantization_intervals = 0;
		optQuantMode = 1;
		conf_params->predThreshold = predThreshold = 0.99;
		conf_params->sampleDistance = sampleDistance = 100;
		conf_params->offset = offset = 0;
		
		conf_params->szMode = szMode = SZ_BEST_COMPRESSION;
		
		conf_params->gzipMode = gzipMode = 1;
		conf_params->errorBoundMode = errorBoundMode = PSNR;
		conf_params->psnr = psnr = 90;
		
		conf_params->pw_relBoundRatio = pw_relBoundRatio = 1E-3;
		conf_params->segment_size = segment_size = 36;
		
		conf_params->pwr_type = pwr_type = SZ_PWR_AVG_TYPE;
		
		SZ_Reset();
		return SZ_SCES;
	}
    
    if (access(sz_cfgFile, F_OK) != 0)
    {
        printf("[SZ] Configuration file NOT accessible.\n");
        return SZ_NSCS;
    }
    
    printf("[SZ] Reading SZ configuration file (%s) ...\n", sz_cfgFile);    
    ini = iniparser_load(sz_cfgFile);
    if (ini == NULL)
    {
        printf("[SZ] Iniparser failed to parse the conf. file.\n");
        return SZ_NSCS;
    }

	endianTypeString = iniparser_getstring(ini, "ENV:dataEndianType", "LITTLE_ENDIAN_DATA");
	if(strcmp(endianTypeString, "LITTLE_ENDIAN_DATA")==0)
		dataEndianType = LITTLE_ENDIAN_DATA;
	else if(strcmp(endianTypeString, "BIG_ENDIAN_DATA")==0)
		dataEndianType = BIG_ENDIAN_DATA;
	else
	{
		printf("Error: Wrong dataEndianType: please set it correctly in sz.config.\n");
		iniparser_freedict(ini);
		return SZ_NSCS;
	}

	conf_params->dataEndianType = dataEndianType;

	// Reading/setting detection parameters
	
	par = iniparser_getstring(ini, "ENV:sol_name", NULL);
	snprintf(sol_name, 256, "%s", par);
	
    if(strcmp(sol_name, "SZ")==0)
		sol_ID = SZ;
	else if(strcmp(sol_name, "PASTRI")==0)
		sol_ID = PASTRI;
	else{
		printf("[SZ] Error: wrong solution name (please check sz.config file)\n");
		iniparser_freedict(ini);
		return SZ_NSCS;
	}
	
	conf_params->sol_ID = sol_ID;

	if(sol_ID==SZ)
	{
		layers = (int)iniparser_getint(ini, "PARAMETER:layers", 0);
		conf_params->layers = layers;
		
		int max_quant_intervals = iniparser_getint(ini, "PARAMETER:max_quant_intervals", 65536);
		conf_params->max_quant_intervals = max_quant_intervals;
		
		int quantization_intervals = (int)iniparser_getint(ini, "PARAMETER:quantization_intervals", 0);
		conf_params->quantization_intervals = quantization_intervals;
		if(quantization_intervals>0)
		{
			updateQuantizationInfo(quantization_intervals);
			conf_params->max_quant_intervals = max_quant_intervals = quantization_intervals;
			stateNum = quantization_intervals;
			allNodes = quantization_intervals*2;
			optQuantMode = 0;
		}
		else //==0
		{
			maxRangeRadius = max_quant_intervals/2;
			stateNum = maxRangeRadius*2;
			allNodes = maxRangeRadius*4;

			intvCapacity = maxRangeRadius*2;
			intvRadius = maxRangeRadius;
			
			optQuantMode = 1;
		}
		
		if(quantization_intervals%2!=0)
		{
			printf("Error: quantization_intervals must be an even number!\n");
			iniparser_freedict(ini);
			return SZ_NSCS;
		}
		
		predThreshold = (float)iniparser_getdouble(ini, "PARAMETER:predThreshold", 0);
		conf_params->predThreshold = predThreshold;
		sampleDistance = (int)iniparser_getint(ini, "PARAMETER:sampleDistance", 0);
		conf_params->sampleDistance = sampleDistance;
		
		offset = (int)iniparser_getint(ini, "PARAMETER:offset", 0);
		conf_params->offset = offset;
		
		modeBuf = iniparser_getstring(ini, "PARAMETER:szMode", NULL);
		if(modeBuf==NULL)
		{
			printf("[SZ] Error: Null szMode setting (please check sz.config file)\n");
			iniparser_freedict(ini);
			return SZ_NSCS;					
		}
		else if(strcmp(modeBuf, "SZ_BEST_SPEED")==0)
			szMode = SZ_BEST_SPEED;
		else if(strcmp(modeBuf, "SZ_DEFAULT_COMPRESSION")==0)
			szMode = SZ_DEFAULT_COMPRESSION;
		else if(strcmp(modeBuf, "SZ_BEST_COMPRESSION")==0)
			szMode = SZ_BEST_COMPRESSION;
		else
		{
			printf("[SZ] Error: Wrong szMode setting (please check sz.config file)\n");
			iniparser_freedict(ini);
			return SZ_NSCS;	
		}
		conf_params->szMode = szMode;
		
		modeBuf = iniparser_getstring(ini, "PARAMETER:gzipMode", NULL);
		if(modeBuf==NULL)
		{
			printf("[SZ] Error: Null Gzip mode setting (please check sz.config file)\n");
			iniparser_freedict(ini);
			return SZ_NSCS;					
		}		
		else if(strcmp(modeBuf, "Gzip_NO_COMPRESSION")==0)
			gzipMode = 0;
		else if(strcmp(modeBuf, "Gzip_BEST_SPEED")==0)
			gzipMode = 1;
		else if(strcmp(modeBuf, "Gzip_BEST_COMPRESSION")==0)
			gzipMode = 9;
		else if(strcmp(modeBuf, "Gzip_DEFAULT_COMPRESSION")==0)
			gzipMode = -1;
		else
		{
			printf("[SZ] Error: Wrong gzip Mode (please check sz.config file)\n");
			return SZ_NSCS;
		}
		conf_params->gzipMode = gzipMode;
		//maxSegmentNum = (int)iniparser_getint(ini, "PARAMETER:maxSegmentNum", 0); //1024
		
		//spaceFillingCurveTransform = (int)iniparser_getint(ini, "PARAMETER:spaceFillingCurveTransform", 0);
		
		//reOrgSize = (int)iniparser_getint(ini, "PARAMETER:reOrgBlockSize", 0); //8
		
		errBoundMode = iniparser_getstring(ini, "PARAMETER:errorBoundMode", NULL);
		if(errBoundMode==NULL)
		{
			printf("[SZ] Error: Null error bound setting (please check sz.config file)\n");
			iniparser_freedict(ini);
			return SZ_NSCS;				
		}
		else if(strcmp(errBoundMode,"ABS")==0||strcmp(errBoundMode,"abs")==0)
			errorBoundMode=ABS;
		else if(strcmp(errBoundMode, "REL")==0||strcmp(errBoundMode,"rel")==0)
			errorBoundMode=REL;
		else if(strcmp(errBoundMode, "ABS_AND_REL")==0||strcmp(errBoundMode, "abs_and_rel")==0)
			errorBoundMode=ABS_AND_REL;
		else if(strcmp(errBoundMode, "ABS_OR_REL")==0||strcmp(errBoundMode, "abs_or_rel")==0)
			errorBoundMode=ABS_OR_REL;
		else if(strcmp(errBoundMode, "PW_REL")==0||strcmp(errBoundMode, "pw_rel")==0)
			errorBoundMode=PW_REL;
		else if(strcmp(errBoundMode, "PSNR")==0||strcmp(errBoundMode, "psnr")==0)
			errorBoundMode=PSNR;
		else if(strcmp(errBoundMode, "ABS_AND_PW_REL")==0||strcmp(errBoundMode, "abs_and_pw_rel")==0)
			errorBoundMode=ABS_AND_PW_REL;
		else if(strcmp(errBoundMode, "ABS_OR_PW_REL")==0||strcmp(errBoundMode, "abs_or_pw_rel")==0)
			errorBoundMode=ABS_OR_PW_REL;
		else if(strcmp(errBoundMode, "REL_AND_PW_REL")==0||strcmp(errBoundMode, "rel_and_pw_rel")==0)
			errorBoundMode=REL_AND_PW_REL;
		else if(strcmp(errBoundMode, "REL_OR_PW_REL")==0||strcmp(errBoundMode, "rel_or_pw_rel")==0)
			errorBoundMode=REL_OR_PW_REL;
		else
		{
			printf("[SZ] Error: Wrong error bound mode (please check sz.config file)\n");
			iniparser_freedict(ini);
			return SZ_NSCS;
		}
		conf_params->errorBoundMode = errorBoundMode;
		
		absErrBound = (double)iniparser_getdouble(ini, "PARAMETER:absErrBound", 0);
		conf_params->absErrBound = absErrBound;
		relBoundRatio = (double)iniparser_getdouble(ini, "PARAMETER:relBoundRatio", 0);
		conf_params->relBoundRatio = relBoundRatio;
		psnr = (double)iniparser_getdouble(ini, "PARAMETER:psnr", 0);
		conf_params->psnr = psnr;
		pw_relBoundRatio = (double)iniparser_getdouble(ini, "PARAMETER:pw_relBoundRatio", 0);
		conf_params->pw_relBoundRatio = pw_relBoundRatio;
		segment_size = (int)iniparser_getint(ini, "PARAMETER:segment_size", 0);
		conf_params->segment_size = segment_size;
		
		modeBuf = iniparser_getstring(ini, "PARAMETER:pwr_type", "MIN");
		
		if(strcmp(modeBuf, "MIN")==0)
			pwr_type = SZ_PWR_MIN_TYPE;
		else if(strcmp(modeBuf, "AVG")==0)
			pwr_type = SZ_PWR_AVG_TYPE;
		else if(strcmp(modeBuf, "MAX")==0)
			pwr_type = SZ_PWR_MAX_TYPE;
		else if(modeBuf!=NULL)
		{
			printf("[SZ] Error: Wrong pwr_type setting (please check sz.config file).\n");
			iniparser_freedict(ini);
			return SZ_NSCS;	
		}
		else //by default
			pwr_type = SZ_PWR_AVG_TYPE;
		conf_params->pwr_type = pwr_type;
    //initialization for Huffman encoding
    
		SZ_Reset();		
	}
	else if(sol_ID == PASTRI)
	{//load parameters for PSTRI
		pastri_par.bf[0] = (int)iniparser_getint(ini, "PARAMETER:basisFunction_0", 0);		
		pastri_par.bf[1] = (int)iniparser_getint(ini, "PARAMETER:basisFunction_1", 0);		
		pastri_par.bf[2] = (int)iniparser_getint(ini, "PARAMETER:basisFunction_2", 0);		
		pastri_par.bf[3] = (int)iniparser_getint(ini, "PARAMETER:basisFunction_3", 0);
		pastri_par.numBlocks = (int)iniparser_getint(ini, "PARAMETER:numBlocks", 0);		
		absErrBound = pastri_par.originalEb = (double)iniparser_getdouble(ini, "PARAMETER:absErrBound", 1E-3);
	}
	
    iniparser_freedict(ini);
    return SZ_SCES;
}

/*-------------------------------------------------------------------------*/
/**
    @brief      It reads and tests the configuration given.
    @return     integer         1 if successfull.

    This function reads the configuration file. Then test that the
    configuration parameters are correct (including directories).

 **/
/*-------------------------------------------------------------------------*/
int SZ_LoadConf() {
    int res = SZ_ReadConf();
    if (res != SZ_SCES)
    {
        printf("[SZ] ERROR: Impossible to read configuration.\n");
        return SZ_NSCS;
    }
    return SZ_SCES;
}

int checkVersion(char* version)
{
	int i = 0;
	for(;i<3;i++)
		if(version[i]!=versionNumber[i])
			return 0;
	return 1;
}

/*double fabs(double value)
{
	if(value<0)
		return -value;
	else
		return value;
}*/
