/**
 * \file
 *
 * \brief Source for csvstorage plugin
 *
 * \copyright BSD License (see doc/COPYING or http://www.libelektra.org)
 *
 */


#ifndef HAVE_KDBCONFIG
# include "kdbconfig.h"
#endif

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <kdberrors.h>

#include "csvstorage.h"

static char *parseLine(char *_line, char delim, unsigned long offset)
{
	char *line = (_line + offset);
	if(*line == '\0')
		return NULL;
	char *ptr = strchr(line, delim);

	if(ptr == NULL)
	{
		line[strlen(line)-1]='\0';
	}
	else
		*ptr = '\0';
	return line;
}

static unsigned long getLineLength(FILE *fp)
{
	int startPos = ftell(fp);
	char c;
	while((c = fgetc(fp)) && (!feof(fp)))
	{
		if(c == '\n')
			break;
	}
	int  endPos = ftell(fp);
	fseek(fp, startPos, SEEK_SET);
	if((endPos - startPos) == 0)
		return 0;
	else
		return (endPos - startPos)+1;
}

static unsigned long getColumnCount(char *lineBuffer, char delim)
{
	char *ptr = lineBuffer;
	unsigned long counter = 0;
	while(*ptr != '\0')
	{
		if(*ptr == delim)
			++counter;
		++ptr;
	}
	++counter;
	return counter;
}

char *itostr(char *buf, unsigned long i, uint8_t len)
{
	snprintf(buf, len, "%lu", i);
	return buf;
}

static Key *getKeyByOrderNr(KeySet *ks, unsigned long n)
{
	Key *cur;
	char buf[16];
	ksRewind(ks);
	while((cur = ksNext(ks)) != NULL)
	{
		if(!strcmp(keyString(keyGetMeta(cur, "csvOrder")), itostr(buf, n, sizeof(buf)-1)))
			return cur; 
	}
	return NULL;
}
int elektraCsvstorageGet(Plugin *handle, KeySet *returned, Key *parentKey)
{
	if (!strcmp(keyName(parentKey), "system/elektra/modules/csvstorage"))
	{
		KeySet *contract = ksNew (30,
				keyNew ("system/elektra/modules/csvstorage",
					KEY_VALUE, "csvstorage plugin waits for your orders", KEY_END),
				keyNew ("system/elektra/modules/csvstorage/exports", KEY_END),
				keyNew ("system/elektra/modules/csvstorage/exports/get",
					KEY_FUNC, elektraCsvstorageGet, KEY_END),
				keyNew ("system/elektra/modules/csvstorage/exports/set",
					KEY_FUNC, elektraCsvstorageSet, KEY_END),
#include ELEKTRA_README(csvstorage)
				keyNew ("system/elektra/modules/csvstorage/infos/version",
					KEY_VALUE, PLUGINVERSION, KEY_END),
				KS_END);
		ksAppend (returned, contract);
		ksDel (contract);

		return 1; /* success */
	}
	/* get all keys */
	KeySet *config = elektraPluginGetConfig(handle);
	Key *delimKey = ksLookupByName(config, "/delimiter", 0);
	Key *readHeaderKey = ksLookupByName(config, "/useheader", 0);
	Key *key;
	KeySet *header = ksNew(0, KS_END);
	Key *dirKey;
	Key *cur;
	FILE *fp = NULL;
	char *col;
	char buf[15];
	int nr_keys = 1;
	
	char delim = ';';
	if(delimKey)
		delim = ((char *)keyString(delimKey))[0];
	
	uint8_t useHeader = 0;
	if(readHeaderKey)
	{
		if((((char *)keyString(readHeaderKey))[0] - '0') == 1)
			useHeader = 1;
	}
	const char *fileName;
	fileName = keyString(parentKey);
	fp = fopen(fileName, "rb");
	if(!fp)
	{
		ELEKTRA_SET_ERROR(114, parentKey, "couldn't open file");
		return -1;
	}

	unsigned long length = 0;
	length = getLineLength(fp);
	if(length == 0)
	{
		ELEKTRA_ADD_WARNING(116, parentKey, "Empty file");
		fclose(fp);
		return 1;
	}

	char *lineBuffer;
	lineBuffer = malloc(length * sizeof(char)+1);
	if(!lineBuffer)
	{
		ELEKTRA_SET_ERROR(117, parentKey, "Out of memory");
		return -1;
	}
	if(!fgets(lineBuffer, length, fp))
	{
		ELEKTRA_SET_ERROR(114, parentKey, "Cant read File");
		return -1;
	}
	
	unsigned long columns = 0;
	columns = getColumnCount(lineBuffer, delim);

	unsigned long colCounter = 0;
	unsigned long lineCounter = 0;
	unsigned long offset = 0;
	if(useHeader == 1)
	{
		colCounter = 0;
		offset = 0;
		while((col = parseLine(lineBuffer, delim, offset)) != NULL)
		{
			offset += strlen(col)+1;
			key = keyDup(parentKey);
			keyAddBaseName(key, col);
			keySetMeta(key, "csvOrder", itostr(buf, colCounter, sizeof(buf)-1));
			ksAppendKey(header, key);
			++colCounter;
		}
	}
	else
	{
		colCounter = 0;
		while(colCounter < columns)
		{
			key = keyDup(parentKey);
			col = itostr(buf, colCounter, sizeof(buf)-1);
			keyAddBaseName(key, col);
			keySetMeta(key, "csvOrder", col);
			ksAppendKey(header, key);
			++colCounter;
		}
	}

	while(!feof(fp))
	{
		length = getLineLength(fp);
		if(length == 0)
			break;
		lineBuffer = realloc(lineBuffer, length * sizeof(char)+1);
		fgets(lineBuffer, length, fp);
		dirKey = keyDup(parentKey);
		snprintf(buf, sizeof(buf)-1, "#%lu", lineCounter);
		keyAddBaseName(dirKey, buf);
		ksAppendKey(returned, dirKey);
		++nr_keys;
		offset = 0;
		colCounter = 0;
		while((col = parseLine(lineBuffer, delim, offset)) != NULL)
		{
			cur = getKeyByOrderNr(header, colCounter);
			offset += strlen(col)+1;
			key = keyDup(dirKey);
			keyAddBaseName(key, keyBaseName(cur));
			keySetString(key, col);
			keySetMeta(key, "csvOrder", itostr(buf, colCounter, sizeof(buf)-1));
			ksAppendKey(returned, key);
			keyDel(key);
			++nr_keys;
			++colCounter;
		}
		if(colCounter != columns)
		{
			ELEKTRA_ADD_WARNING(116, parentKey, "illegal number of columns");
		}
		++lineCounter;
		keyDel(dirKey);
	}

	key	= keyDup(parentKey);
	snprintf(buf, sizeof(buf)-1, "#%lu", lineCounter);
	keySetString(key, buf);
	ksAppendKey(returned, key);
	
	fclose(fp);
	free(lineBuffer);
	ksDel(header);
	return nr_keys;
}

int elektraCsvstorageSet(Plugin *handle, KeySet *returned, Key *parentKey)
{
	unsigned long colCounter = 0;
	unsigned long columns = 0;
	uint8_t printHeader = 0;
	Key *cur;
	KeySet *toWriteKS;
	Key *toWrite;
	FILE *fp;
	char outputDelim;
	KeySet *config = elektraPluginGetConfig(handle);
	Key *delimKey = ksLookupByName(config, "/outputdelim", 0);
	Key *printHeaderKey = ksLookupByName(config, "/useheader", 0);
	if(delimKey)
		outputDelim = ((char *)keyValue(delimKey))[0];
	else
		outputDelim = ';';
	if(printHeaderKey)
	{
		if((((char *)keyString(printHeaderKey))[0] - '0') == 1)
			printHeader = 1;
		else
			printHeader = 0;
	}
	else
		printHeader = 0;
	fp = fopen(keyString(parentKey), "w");
	if(!fp)
	{
		ELEKTRA_SET_ERROR(114, parentKey, "File not found");
		return -1;
	}

	ksLookup(returned, parentKey, KDB_O_POP);
	
	while((cur = ksNext(returned)) != NULL)
	{
		if(keyRel(parentKey, cur) != 1)
			continue;
		toWriteKS = ksCut(returned, cur);
		ksRewind(toWriteKS);
		colCounter = 0;
		if(printHeader)
		{
			while(1)
			{
				if(colCounter == ULONG_MAX)
				{
					ELEKTRA_SET_ERROR(115, parentKey, "number of columns exceeds ULONG_MAX");
					return -1;
				}
				toWrite = getKeyByOrderNr(toWriteKS, colCounter);
				if(!toWrite)
					break;
				if(colCounter)
					fprintf(fp, "%c", outputDelim);
				++colCounter;
				fprintf(fp, "%s", keyBaseName(toWrite));
			}
			fprintf(fp, "\n");
			if(!colCounter)
			{
				ELEKTRA_SET_ERROR(115, parentKey, "no columns");
				fclose(fp);
				return -1;
			}
			columns = colCounter;
			colCounter = 0;
			printHeader = 0;
		}
		else
		{
			while(1)
			{
				if(colCounter > columns)
					break;
				toWrite = getKeyByOrderNr(toWriteKS, colCounter);
				if(!toWrite)
					break;
				if(colCounter)
					fprintf(fp, "%c", outputDelim);
				++colCounter;
				fprintf(fp, "%s", keyString(toWrite));

			}
			ksDel(toWriteKS);
			fprintf(fp, "\n");
		}
		if(columns == 0)
			columns = colCounter;
		if(colCounter != columns)
		{    
			ELEKTRA_SET_ERROR(115, parentKey, "illegal number of columns\n");
			fclose(fp);
			return -1;
		}
	}
	fclose(fp);
	return 1; /* success */
}

Plugin *ELEKTRA_PLUGIN_EXPORT(csvstorage)
{
	return elektraPluginExport("csvstorage",
			ELEKTRA_PLUGIN_GET,	&elektraCsvstorageGet,
			ELEKTRA_PLUGIN_SET,	&elektraCsvstorageSet,
			ELEKTRA_PLUGIN_END);
}

