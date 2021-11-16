/***********************************************************************
							datesgit
	 Programa de recuperació de les dades dels fitxers unificats amb git.
			Joan Masó    Novembre de 2021

***********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "cJSON.h"
#include "Shlwapi.h"

struct COUNT_FOLDER
{
	unsigned long int folders;
	unsigned long int files;
	unsigned long int hidden_folders;
	unsigned long int hidden_files;

	//Only for Restoring
	unsigned long int identical_folders;
	unsigned long int identical_files;
	unsigned long int missing_folders;
	unsigned long int missing_files;
	unsigned long int error_folders;
	unsigned long int error_files;
};

BOOL LastAccessTimeSuppont=FALSE;

int AddFileDatesToJSON(cJSON *json, char *szfolder, struct COUNT_FOLDER *count_folder);
int RestoreFileDatesFromJSON(cJSON *json, char *szfolder, struct COUNT_FOLDER *count_folder);

int AddFileTimeToJSON(cJSON *json_object, const FILETIME *filetime);
int GetFileTimeFromJSON(cJSON *json_object, FILETIME *filetime, const char *szfilename);

LONG CompareFileTimeAsSystemTime(const FILETIME *lpFileTime1,const FILETIME *lpFileTime2);

char* replaceWord(const char* s, const char* oldW, const char* newW, size_t *cnt);
void GetMyStringFind(char *str, const char *key, size_t n);

/********************** Main **********************/

int main(int argc, char *argv[])
{
cJSON *json;
char file_name_json[MAX_PATH];
char *str_json, *str_json2;
FILE *pf_json;
struct COUNT_FOLDER count_folder={0,0,0,0,0,0,0,0,0,0};
HANDLE hFind = INVALID_HANDLE_VALUE;
WIN32_FIND_DATA ffd;
size_t size, cnt, i;
char str_find[MAX_PATH+20];

	puts("Application to manage original dates in files shared by git");
	if ( argc > 1 &&  (argv[1][0] == 63 /* '?' == 63 */ || 0==strcmp(argv[1], "/?") ))	
	{
		puts("Help:");
		puts("The Git system does not relay on dates to detect changes. Dates are not stored by a git repository and are no communicated back in a pull operation.");
		puts("This prevents to combine git with local comparisions based on date criteria.");
		puts("This program overcomes this problem by adding a datesgit.json file to the git repository with the actual dates of the file.");
		puts("");
		puts("Use DatesGit STORE before creating a commit and push, to store the dates and sent them to the git repository");
		puts("Use DatesGit RESTORE after requesting a pull, to restore the dates to their original values");
		puts("");
		puts("DatesGit STORE creates a file datesgit.json with the original creation and last write dates for each file on a folder and their subfolders");
		puts("The format of the JSON file is explained in this example");
		puts("{");
		puts("\t\"[folder_name]\":  //Optional");
		puts("\t\t\"[subfolder_name]\":   //Optional");
		puts("\t\t\t\"[subsubfolder_name]\":   //Optional recursively");
		puts("\t\t\t\t\"[file_name]\":");
		puts("\t\t\t\t\t\"cre\":{   //Creation date");
		puts("\t\t\t\t\t\t\"Y\":2021,\"M\":11,\"D\":14,\"h\":7,\"mi\":52,\"s\":7,\"ms\":670");
		puts("\t\t\t\t\t}");
		puts("\t\t\t\t\t\"mod\":{   //Last write date");
		puts("\t\t\t\t\t\t\"Y\":2021,\"M\":11,\"D\":14,\"h\":7,\"mi\":52,\"s\":7,\"ms\":670");
		puts("\t\t\t\t\t}");
		puts("\t\t\t\t}");
		puts("\t\t\t}");
		puts("\t\t}");
		puts("\t}");
		puts("}");
		return 0;
	}

	if (argc!=3)
	{
		fputs("Syntax Error\n", stderr);
		puts("Use one of the following:");
		puts("DatesGit /?");
		puts("DatesGit STORE [git clone folder]");
		puts("DatesGit RESTORE [git clone folder]");
		return 1;
	}

	if (0==strcmp(argv[1], "STORE"))
	{
		printf("Storing dates...\n");
		json = cJSON_CreateObject();

		AddFileDatesToJSON(json, argv[2], &count_folder);

		printf("\n\nSUMMARY:\n"
			"Subfolders processed: %lu\n"
			"Files processed: %lu...\n"
			"Hidden subfolders skipped: %lu\n"
			"Hidden files in visible folders skipped: %lu\n", count_folder.folders, count_folder.files, count_folder.hidden_folders, count_folder.hidden_files);

		//Saving dates to JSON file
		strncpy(file_name_json, argv[2], MAX_PATH);
		file_name_json[MAX_PATH-1]='\0';
		PathAppendA(file_name_json, "datesgit.json");
		if (NULL==(pf_json=fopen(file_name_json, "wt")))
		{
			fprintf(stderr, "Cannot create file: %s\n", file_name_json);
			return 1;
		}

		str_json=cJSON_Print(json);

		for (i=3; i<MAX_PATH; i++)
		{
			GetMyStringFind(str_find, "M", i);
			//Deducing the size of the file.
			str_json2=replaceWord(str_json, str_find, "\"M\":", &cnt);
			free(str_json);
			GetMyStringFind(str_find, "D", i);
			str_json=replaceWord(str_json2, str_find, "\"D\":", &cnt);
			free(str_json2);
			if (cnt==0)
				break;
			GetMyStringFind(str_find, "h", i);
			str_json2=replaceWord(str_json, str_find, "\"h\":", &cnt);
			free(str_json);
			GetMyStringFind(str_find, "m", i);
			str_json=replaceWord(str_json2, str_find, "\"mi\":", &cnt);
			free(str_json2);
			GetMyStringFind(str_find, "s", i);
			str_json2=replaceWord(str_json, str_find, "\"s\":", &cnt);
			free(str_json);
			GetMyStringFind(str_find, "ms", i);
			str_json=replaceWord(str_json2, str_find, "\"ms\":", &cnt);
			free(str_json2);
		}
		str_json2=replaceWord(str_json, "\"Y\":\t", "\"Y\":", &cnt);

		fputs(str_json2, pf_json);
		free(str_json);
		fclose(pf_json);
		cJSON_Delete(json);
		puts("File datesgit.json created. DONE!.");
	}
	else if (0==strcmp(argv[1], "RESTORE"))
	{
		printf("Restoring dates...\n");
		strncpy(file_name_json, argv[2], MAX_PATH);
		file_name_json[MAX_PATH-1]='\0';
		PathAppendA(file_name_json, "datesgit.json");

		hFind = FindFirstFile(file_name_json, &ffd);
		if (INVALID_HANDLE_VALUE == hFind) 
		{
			fprintf(stderr, "Cannot find file: %s. Create a datesgit.json with the STORE option.\n", file_name_json);
			return 1;
		}

		if (ffd.nFileSizeHigh>0)
		{
			fprintf(stderr, "File: %s is to big to be processed.\n", file_name_json);
			return 1;
		}
		if (NULL==(str_json=malloc(ffd.nFileSizeLow+1)))
		{
			fprintf(stderr, "No memory for %lu bytes. %s is to big to be processed.\n", ffd.nFileSizeLow, file_name_json);
			return 1;
		}

		puts("datesgit.json found. Processing...");

		if (NULL==(pf_json=fopen(file_name_json, "rt")))
		{
			fprintf(stderr, "Cannot open file: %s\n", file_name_json);
			return 1;
		}
		if (0==(size=fread(str_json, 1, ffd.nFileSizeLow, pf_json)))
		{
			fprintf(stderr, "Cannot read file: %s\n", file_name_json);
			return 1;
		}
		str_json[min(size, ffd.nFileSizeLow)]='\0';
		fclose(pf_json);

		if (NULL==(json = cJSON_Parse(str_json)))
		{
			fprintf(stderr, "Error passing the JSON content of file: %s\n", file_name_json);
			return 1;
		}
		free(str_json);

		RestoreFileDatesFromJSON(json, argv[2], &count_folder);

		printf("\n\nSUMMARY:\n"
				"Subfolders processed: %lu\nFiles processed: %lu\n"
				"Hidden subfolders skipped: %lu\nHidden files in visible folders skipped: %lu\n"
				"Subfolders with the same dates (not updated): %lu\nFiles with the same dates (skipped): %lu\n"
				"Missing subfolders in the JSON file: %lu\nMissing files in the JSON file: %lu\n"
				"Subfolders with errors: %lu\nFiles in visible folders with errors: %lu\n", 
						count_folder.folders, count_folder.files, 
						count_folder.hidden_folders, count_folder.hidden_files, 
						count_folder.identical_folders, count_folder.identical_files,
						count_folder.missing_folders, count_folder.missing_files, 
						count_folder.error_folders, count_folder.error_files);
		cJSON_Delete(json);
		puts("DONE!.");
	}
	else
	{
		printf("Unknown option: %s", argv[1]);
		return 0;
	}
	return 0;
}


//Recursive function that browsers through the folders and files and stores their dates. 
int AddFileDatesToJSON(cJSON *json, char *szfolder, struct COUNT_FOLDER *count_folder)
{
HANDLE hFind = INVALID_HANDLE_VALUE;
WIN32_FIND_DATA ffd;
cJSON *json_object, *json_date;
char szfolder_path[MAX_PATH];

	printf("\nProcessing folder \"%s\"...", szfolder);

	strncpy(szfolder_path, szfolder, MAX_PATH);
	szfolder_path[MAX_PATH-1]='\0';
	PathAppendA(szfolder_path, "*.*");
	hFind = FindFirstFile(szfolder_path, &ffd);

	if (INVALID_HANDLE_VALUE == hFind) 
	{
		fprintf(stderr, "Cannot explore folder: %s\n", szfolder);
		return 1;
	} 
   
	// List all the files in the folder with some info about them.
	do
	{
		if (0==strcmp(ffd.cFileName, ".") || 0==strcmp(ffd.cFileName, "..") || 0==strcmp(ffd.cFileName, "datesgit.json") || 0==stricmp(ffd.cFileName, "Thumbs.db"))
			continue;
		if (ffd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)
		{
			if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				count_folder->hidden_folders++;
			else
				count_folder->hidden_files++;
			continue;
		}

		json_object=cJSON_CreateObject();
		cJSON_AddItemToObject(json, ffd.cFileName, json_object);

		json_date=cJSON_CreateObject();
		cJSON_AddItemToObject(json_object, "cre", json_date);
		if (AddFileTimeToJSON(json_date, &ffd.ftCreationTime))
			return 1;
		json_date=cJSON_CreateObject();
		cJSON_AddItemToObject(json_object, "mod", json_date);
		if (AddFileTimeToJSON(json_date, &ffd.ftLastWriteTime))
			return 1;
		if (LastAccessTimeSuppont)
		{
			json_date=cJSON_CreateObject();
			cJSON_AddItemToObject(json_object, "acc", json_date);
			if (AddFileTimeToJSON(json_date, &ffd.ftLastAccessTime))
				return 1;
		}

		if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			strncpy(szfolder_path, szfolder, MAX_PATH);
			szfolder_path[MAX_PATH-1]='\0';
			PathAppendA(szfolder_path, ffd.cFileName);
			if (AddFileDatesToJSON(json_object, szfolder_path, count_folder))
				return 1;
			count_folder->folders++;
		}
		else
		{
			count_folder->files++;
		}
	}
	while (FindNextFile(hFind, &ffd) != 0);
	return 0;
}

int RestoreFileDatesFromJSON(cJSON *json, char *szfolder, struct COUNT_FOLDER *count_folder)
{
FILETIME creation_time, last_access_time, last_write_time;
HANDLE hFind = INVALID_HANDLE_VALUE, hFile = INVALID_HANDLE_VALUE;
WIN32_FIND_DATA ffd;
cJSON *json_object, *json_date;
char szfolderfile_path[MAX_PATH];
	
	printf("\nProcessing folder \"%s\"...", szfolder);

	strncpy(szfolderfile_path, szfolder, MAX_PATH);
	szfolderfile_path[MAX_PATH-1]='\0';
	PathAppendA(szfolderfile_path, "*.*");
	hFind = FindFirstFile(szfolderfile_path, &ffd);

	if (INVALID_HANDLE_VALUE == hFind) 
	{
		fprintf(stderr, "Cannot explore folder: %s\n", szfolder);
		return 1;
	} 
   
	// List all the files in the folder with some info about them.
	do
	{
		if (0==strcmp(ffd.cFileName, ".") || 0==strcmp(ffd.cFileName, "..") || 0==strcmp(ffd.cFileName, "datesgit.json") || 0==stricmp(ffd.cFileName, "Thumbs.db"))
			continue;
		if (ffd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)
		{
			if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				count_folder->hidden_folders++;
			else
				count_folder->hidden_files++;
			continue;
		}

		json_object=cJSON_GetObjectItem(json, ffd.cFileName);
		if (NULL==json_object)
		{
			fprintf(stderr, "Cannot find dates to restore for %s in %s\n", ffd.cFileName, szfolder);
			if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				count_folder->missing_folders++;
			else
				count_folder->missing_files++;
			continue;
		}

		if (NULL==(json_date=cJSON_GetObjectItem(json_object, "cre")))
		{
			fprintf(stderr, "Cannot find creationTime for: %s\n", ffd.cFileName);
			return 1;
		}
		if (GetFileTimeFromJSON(json_date, &creation_time, ffd.cFileName))
			return 1;
		if (NULL==(json_date=cJSON_GetObjectItem(json_object, "mod")))
		{
			fprintf(stderr, "Cannot find lastWriteTime for: %s\n", ffd.cFileName);
			return 1;
		}
		if (GetFileTimeFromJSON(json_date, &last_write_time, ffd.cFileName))
			return 1;
		
		if (LastAccessTimeSuppont)
		{
			if (NULL==(json_date=cJSON_GetObjectItem(json_object, "acc")))
			{
				fprintf(stderr, "Cannot find lastAccessTime for: %s\n", ffd.cFileName);
				return 1;
			}
			if (GetFileTimeFromJSON(json_date, &last_access_time, ffd.cFileName))
				return 1;
		}
		else
		{
			last_access_time.dwLowDateTime=0xFFFFFFFF; 
			last_access_time.dwHighDateTime=0xFFFFFFFF;
		}

		strncpy(szfolderfile_path, szfolder, MAX_PATH);
		szfolderfile_path[MAX_PATH-1]='\0';
		PathAppendA(szfolderfile_path, ffd.cFileName);

		if (0==CompareFileTimeAsSystemTime(&creation_time, &ffd.ftCreationTime) &&
			0==CompareFileTimeAsSystemTime(&last_write_time, &ffd.ftLastWriteTime) &&
			(LastAccessTimeSuppont==FALSE || 0==CompareFileTimeAsSystemTime(&last_access_time, &ffd.ftLastAccessTime)))
		{
			if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				count_folder->identical_folders++;
			else
				count_folder->identical_files++;
		}
		else
		{
			if (INVALID_HANDLE_VALUE==(hFile=CreateFileA(szfolderfile_path, FILE_WRITE_ATTRIBUTES, 0, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS/*https://stackoverflow.com/questions/7543787/changing-a-directory-time-date*/, NULL)))
		    {
				fprintf(stderr, "Cannot access %s %s\n", (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? "folder" :  "file", szfolderfile_path);
				if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
					count_folder->error_folders++;
				else
					count_folder->error_files++;
				continue;
			}
			if (FALSE==SetFileTime(hFile, &creation_time, &last_access_time, &last_write_time))
			{
				fprintf(stderr, "Cannot restore dates to %s %s\n", (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? "folder" :  "file", szfolderfile_path);
				if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
					count_folder->error_folders++;
				else
					count_folder->error_files++;
				CloseHandle(hFile);
				continue;
			}
			CloseHandle(hFile);
		}

		if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			if (RestoreFileDatesFromJSON(json_object, szfolderfile_path, count_folder))
				return 1;
			count_folder->folders++;
		}
		else
		{
			count_folder->files++;
		}
	}
	while (FindNextFile(hFind, &ffd) != 0);
	return 0;
}

int AddFileTimeToJSON(cJSON *json_object, const FILETIME *filetime)
{
SYSTEMTIME systime;
	if (FALSE==(FileTimeToSystemTime(filetime, &systime)))
		return 1;
	
	cJSON_AddNumberToObject(json_object, "Y", systime.wYear);
	cJSON_AddNumberToObject(json_object, "M", systime.wMonth);
	cJSON_AddNumberToObject(json_object, "D", systime.wDay);
	cJSON_AddNumberToObject(json_object, "h", systime.wHour);
	cJSON_AddNumberToObject(json_object, "m", systime.wMinute);
	cJSON_AddNumberToObject(json_object, "s", systime.wSecond);
	cJSON_AddNumberToObject(json_object, "ms", systime.wMilliseconds);
	return 0;
}

int GetFileTimeFromJSON(cJSON *json_object, FILETIME *filetime, const char *szfilename)
{
SYSTEMTIME systime;
cJSON *item;

	if (NULL==(item=cJSON_GetObjectItem(json_object, "Y")))
		fprintf(stderr, "Cannot find year in: %s.%s\n", szfilename, json_object->string);
	if (item->type!=cJSON_Number)
		fprintf(stderr, "Wrong type for year in: %s.%s\n", szfilename, json_object->string);
	systime.wYear=(WORD)item->valueint;

	if (NULL==(item=cJSON_GetObjectItem(json_object, "M")))
		fprintf(stderr, "Cannot find month in: %s.%s\n", szfilename, json_object->string);
	if (item->type!=cJSON_Number)
		fprintf(stderr, "Wrong type for month in: %s.%s\n", szfilename, json_object->string);
	systime.wMonth=(WORD)item->valueint;

	if (NULL==(item=cJSON_GetObjectItem(json_object, "D")))
		fprintf(stderr, "Cannot find day in: %s.%s\n", szfilename, json_object->string);
	if (item->type!=cJSON_Number)
		fprintf(stderr, "Wrong type for day in: %s.%s\n", szfilename, json_object->string);
	systime.wDay=(WORD)item->valueint;

	if (NULL==(item=cJSON_GetObjectItem(json_object, "h")))
		fprintf(stderr, "Cannot find hour in: %s.%s\n", szfilename, json_object->string);
	if (item->type!=cJSON_Number)
		fprintf(stderr, "Wrong type for hour in: %s.%s\n", szfilename, json_object->string);
	systime.wHour=(WORD)item->valueint;

	if (NULL==(item=cJSON_GetObjectItem(json_object, "mi")))
		fprintf(stderr, "Cannot find minute in: %s.%s\n", szfilename, json_object->string);
	if (item->type!=cJSON_Number)
		fprintf(stderr, "Wrong type for minute in: %s.%s\n", szfilename, json_object->string);
	systime.wMinute=(WORD)item->valueint;

	if (NULL==(item=cJSON_GetObjectItem(json_object, "s")))
		fprintf(stderr, "Cannot find second in: %s.%s\n", szfilename, json_object->string);
	if (item->type!=cJSON_Number)
		fprintf(stderr, "Wrong type for second in: %s.%s\n", szfilename, json_object->string);
	systime.wSecond=(WORD)item->valueint;

	if (NULL==(item=cJSON_GetObjectItem(json_object, "ms")))
		fprintf(stderr, "Cannot find milliseconds in: %s.%s\n", szfilename, json_object->string);
	if (item->type!=cJSON_Number)
		fprintf(stderr, "Wrong type for milliseconds in: %s.%s\n", szfilename, json_object->string);
	systime.wMilliseconds=(WORD)item->valueint;

	if (FALSE==(SystemTimeToFileTime(&systime, filetime)))
		return 1;
	return 0;
}

//CompareFileTimeAsSystemTime is necessary because CompareFileTime() detects changes below milliseconds that are not recovered by the JSON file (that is based on SystemTime) 
LONG CompareFileTimeAsSystemTime(const FILETIME *lpFileTime1,const FILETIME *lpFileTime2)
{
SYSTEMTIME systime1, systime2;

	if (FALSE==(FileTimeToSystemTime(lpFileTime1, &systime1)))
		return 1;
	if (FALSE==(FileTimeToSystemTime(lpFileTime2, &systime2)))
		return 1;
	
	if (systime1.wYear<systime2.wYear)
		return -1;
	if (systime1.wYear>systime2.wYear)
		return 1;
	if (systime1.wMonth<systime2.wMonth)
		return -1;
	if (systime1.wMonth>systime2.wMonth)
		return 1;
	if (systime1.wDay<systime2.wDay)
		return -1;
	if (systime1.wDay>systime2.wDay)
		return 1;
	if (systime1.wHour<systime2.wHour)
		return -1;
	if (systime1.wHour>systime2.wHour)
		return 1;
	if (systime1.wMinute<systime2.wMinute)
		return -1;
	if (systime1.wMinute>systime2.wMinute)
		return 1;
	if (systime1.wSecond<systime2.wSecond)
		return -1;
	if (systime1.wSecond>systime2.wSecond)
		return 1;
	if (systime1.wMilliseconds<systime2.wMilliseconds)
		return -1;
	if (systime1.wMilliseconds>systime2.wMilliseconds)
		return 1;
	return 0;
}

// Function to replace a string with another string: 
// https://www.geeksforgeeks.org/c-program-replace-word-text-another-given-word/
// Improved
char* replaceWord(const char* s, const char* oldW, const char* newW, size_t *cnt)
{
char* result;
size_t i, len, newWlen, oldWlen;

	newWlen = strlen(newW);
	oldWlen = strlen(oldW);
  
	*cnt = 0;
    // Counting the number of times old word
    // occur in the string
	len=strlen(s)-oldWlen;
    for (i = 0; i<len; i++) {
        if (strncmp(s+i, oldW, oldWlen) == 0) {
            (*cnt)++;
  
            // Jumping to index after the old word.
            i += oldWlen - 1;
        }
    }
  
    // Making new string of enough length
    result = malloc(strlen(s) + *cnt * (newWlen - oldWlen) + 1);
  
    i = 0;
    while (*s) {
        // compare the substring with the result
        if (strncmp(s, oldW, oldWlen) == 0) {
            strcpy(result+i, newW);
            i += newWlen;
            s += oldWlen;
        }
        else
            result[i++] = *s++;
    }
  
    result[i] = '\0';
    return result;
}

void GetMyStringFind(char *str, const char *key, size_t n)
{
size_t i;

	strcpy(str, "\n");
	for (i=0; i<n; i++)
		strcat(str, "\t");
	strcat(str, "\"");
	strcat(str, key);
	strcat(str, "\":\t");
}